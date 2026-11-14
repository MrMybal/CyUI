"""Editor contracts, portable JSON/TSV, and real UI frames for new CyUI modules."""
from pathlib import Path
import itertools
import os
import sys
import unittest
from dataclasses import replace
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'python'))
from cyui_modules.numeric import evaluate_numeric_expression
from cyui_modules.inspector import (Property, InspectorState, inspector, PropertyVector, PropertyColor,
    PropertyAsset, PropertyArray, PropertyFloat, property_to_json, property_from_json,
    property_category_to_json, property_category_from_json, evaluate_property_expression, common_properties)
from cyui_modules.hierarchy import HierarchyItem, HierarchyState, hierarchy_rows, can_reparent_hierarchy, hierarchy
from cyui_modules.dataview import DataColumn, DataRow, DataViewState, data_view_rows, copy_data_cells, paste_data_cells, data_view
from cyui_modules.filepicker import FilePickerState, FilePickerOptions, FileListing, FileEntry, FileFilter, normalize_file_path, validate_file_picker, file_picker
from cyui_modules.commands import Command
from cyui_modules.command_registry import CommandRegistry
from cyui_modules.nodes import Node, Pin, Link, NodeState, node_editor, NodeGroup, NodeReroute, copy_node_selection, nodes_to_json, nodes_from_json, clone_nodes
from cyui_modules.timeline import Lane, Clip, Marker, TimelineState, timeline, split_timeline_clips, duplicate_timeline_clips, reorder_timeline_lane
try:
    import cyui as c
    from imgui_bundle import imgui as i
except ImportError:
    c = i = None


class Models(unittest.TestCase):
    def test_arithmetic_is_bounded_and_does_not_execute_code(self):
        self.assertAlmostEqual(evaluate_numeric_expression('Value*100', 2.56), 256)
        self.assertEqual(evaluate_numeric_expression('-2^2+2^3^2', 0), 508)
        self.assertEqual(evaluate_numeric_expression('clamp(max(Value, 2)*3,0,10)', 4), 10)
        for expression in ('__import__("os")', 'Value/0', '2 trailing', 'sqrt(-1)', '9^99999999', '['*100+'1'+']'*100):
            with self.assertRaises(ValueError): evaluate_numeric_expression(expression, 2)

    def test_inspector_expressions_types_and_exact_json(self):
        prop = Property(1, 'Value', 2.56, minimum=0, maximum=10)
        self.assertEqual(evaluate_property_expression(prop, 'Value*100'), 10.)
        for value in (2**63-1, 'héllo\n"quoted"', PropertyVector([1, 2, 3]), PropertyColor(), PropertyAsset('Texture', '/Game/T'), PropertyArray([3, 4]), PropertyFloat(1.5)):
            target = replace(prop, value=value, minimum=None, maximum=None)
            self.assertEqual(property_from_json(target, property_to_json(target))[0][1].value, value)
        with self.assertRaises(ValueError): property_from_json(replace(prop, value='text'), property_to_json(prop))
        with self.assertRaises(ValueError): property_from_json(replace(prop, value=PropertyAsset('Mesh')), property_to_json(replace(prop, value=PropertyAsset('Texture'))))
        native = '{"format":"cyui.inspector","kind":"property","property":{"type":"int64","value":9223372036854775807},"version":1}'
        self.assertEqual(property_from_json(replace(prop, value=0, minimum=None, maximum=None), native)[0][1].value, 2**63-1)

    def test_category_paste_is_atomic_and_keyed(self):
        properties = [Property(1, 'Strength', 2.56, key='strength', category='Material'), Property(2, 'Name', 'Terrain', key='name', category='Material')]
        text = property_category_to_json(properties, 'Material')
        destination = [replace(p, id=p.id+100, category='Other') for p in properties]
        events = property_category_from_json(destination, 'Other', text)
        self.assertEqual([key for key, _ in events], [101, 102])
        destination[1].read_only = True
        with self.assertRaises(ValueError): property_category_from_json(destination, 'Other', text)
        with self.assertRaises(ValueError): property_category_from_json(destination[:1], 'Other', text)
        other = [replace(p) for p in properties]; other[0].value = 4.
        self.assertTrue(common_properties([properties, other])[0].mixed)

    def test_hierarchy_preserves_ancestors_and_rejects_cycles(self):
        items = [HierarchyItem(1, 0, 'Root'), HierarchyItem(2, 1, 'Camera'), HierarchyItem(3, 2, 'Lens')]
        state = HierarchyState(collapsed={1}); self.assertEqual(len(hierarchy_rows(items, state)), 1)
        state.query = 'lens'; self.assertEqual(len(hierarchy_rows(items, state)), 3)
        self.assertFalse(can_reparent_hierarchy(items, [1], 3)); self.assertTrue(can_reparent_hierarchy(items, [3], 1))
        cycle = [HierarchyItem(1, 2, 'A'), HierarchyItem(2, 1, 'B')]
        self.assertEqual(len(hierarchy_rows(cycle, HierarchyState())), 2)

    def test_tables_sort_and_paste_typed_cells_atomically(self):
        columns = [DataColumn(1, 'Name', True), DataColumn(2, 'Number', True, 0, 10), DataColumn(3, 'Exact', True)]
        rows = [DataRow(1, ['A\tB\n"C"', 2.56, 2**63-1]), DataRow(2, ['Other', 1., 8])]
        state = DataViewState(anchor=1, active=1, active_column=2)
        self.assertEqual([e.value for _, _, e in paste_data_cells(columns, rows, state, copy_data_cells(rows, state))], rows[0].cells)
        state.column_order = [1, 0]; state.anchor_column = 1; state.active_column = 0
        self.assertEqual([column for _, column, _ in paste_data_cells(columns, rows, state, copy_data_cells(rows, state))], [2, 1])
        state.column_order = []
        state.anchor_column = state.active_column = 1
        self.assertEqual(paste_data_cells(columns, rows, state, 'Value*100')[0][2].value, 10.)
        with self.assertRaises(ValueError): paste_data_cells(columns, rows, state, '2\nwrong')
        state.sort = [(1, False)]; self.assertEqual(data_view_rows(rows, state), [1, 0])

    def test_virtual_file_picker_validates_snapshot_and_overwrite(self):
        self.assertEqual(normalize_file_path('/project/a/../b', '/project'), '/project/b')
        self.assertFalse(normalize_file_path('/project/../../b', '/project'))
        state = FilePickerState(directory='/project', filename='image.PNG')
        listing = FileListing('/project', [FileEntry(1, 'image.PNG')]); options = FilePickerOptions('saveFile', '/project', [FileFilter('Images', ('png',))])
        self.assertTrue(validate_file_picker(state, listing, options)['overwrite_requested'])
        self.assertTrue(validate_file_picker(state, listing, options, True)['accepted'])
        listing.loading = True; self.assertFalse(validate_file_picker(state, listing, options, True)['accepted'])

    def test_command_contexts_conflicts_and_portable_bindings(self):
        registry = CommandRegistry(); self.assertTrue(registry.add(Command(1, 'Save', 'Ctrl+S')))
        self.assertTrue(registry.add(Command(2, 'Editor save', 'Ctrl+S', context='editor')))
        self.assertEqual(registry.match('Ctrl+S', ['editor']), 2)
        self.assertFalse(registry.match('Ctrl+S', ['editor'], True))
        self.assertFalse(registry.add(Command(3, 'Conflict', 'Ctrl+S')))
        text = registry.bindings_json(); registry.rebind(1, 'Alt+S'); registry.load_bindings_json(text)
        self.assertEqual(registry.entries[1].shortcut, 'Ctrl+S')

    def test_graph_clone_remaps_ids_with_legacy_namespace_overlap(self):
        nodes = [Node(1, (0, 0), 'Source', outputs=[Pin(11)]), Node(2, (200, 0), 'Target', inputs=[Pin(21)])]
        fragment = copy_node_selection([1, 2], nodes, [Link(1, 11, 21)], reroutes=[NodeReroute(1, 1, (100, 50))])
        ids = itertools.count(100)
        cloned = clone_nodes(nodes_from_json(nodes_to_json(fragment)), lambda: next(ids))
        self.assertEqual(cloned.links[0].source, cloned.nodes[0].outputs[0].id)
        self.assertEqual(cloned.reroutes[0].link, cloned.links[0].id)
        with self.assertRaises(ValueError): clone_nodes(fragment, lambda: 1)

    def test_timeline_operations_preserve_locked_clips(self):
        clips = [Clip(1, 1, 1, 4), Clip(2, 1, 6, 2, locked=True)]; ids = itertools.count(100)
        split = split_timeline_clips(clips, [1, 2], 3, lambda: next(ids))
        self.assertEqual([p.duration for p in split], [2, 2, 2])
        self.assertEqual(len(duplicate_timeline_clips(clips, [1, 2], 5, lambda: next(ids))), 3)
        self.assertEqual(reorder_timeline_lane([Lane(1, 'A'), Lane(2, 'B')], 2, 1)[0].id, 2)


@unittest.skipIf(i is None, 'imgui-bundle is required for UI frames')
class Frames(unittest.TestCase):
    def test_new_modules_in_every_theme(self):
        i.create_context(); io_ = i.get_io(); io_.set_ini_filename(None); io_.display_size = (1200, 900); io_.delta_time = 1/60
        io_.backend_flags |= i.BackendFlags_.renderer_has_textures
        c.apply()
        def frame(draw):
            i.new_frame(); i.set_next_window_size((1100, 850)); i.begin('Tests'); draw(); i.end(); i.render()
        try:
            for theme in ('mid-grey', 'light-grey', 'amber-night', 'violet', 'navy'):
                c.set_theme(theme)
                state = InspectorState(); fields = [Property(1, 'Number', 2.56, minimum=0, maximum=10, category='Material'), Property(2, 'Color', PropertyColor(), category='Material'), Property(3, 'Position', PropertyVector([1., 2., 3.])), Property(4, 'Tags', PropertyArray(['one', 'two']))]
                frame(lambda: inspector('details', state, fields))
                frame(lambda: hierarchy('tree', HierarchyState(), [HierarchyItem(1, 0, 'Scene'), HierarchyItem(2, 1, 'Camera')]))
                frame(lambda: data_view('data', DataViewState(), [DataColumn(1, 'Name', True), DataColumn(2, 'Value', True)], [DataRow(1, ['Object', 2.56])]))
                frame(lambda: file_picker('files', FilePickerState(), FileListing('/', [FileEntry(1, 'a.txt')])) )
                frame(lambda: node_editor('nodes', NodeState(), [Node(1, (20, 40), 'A')], [], groups=[NodeGroup(10, (0, 0))]))
                frame(lambda: timeline('time', TimelineState(), [Lane(1, 'A')], [Clip(1, 1, 1, 2)], [Marker(1, 2, 'Cue')]))
                registry = CommandRegistry(); registry.add(Command(1, 'Save', 'Ctrl+S')); frame(lambda: registry.poll())
        finally: i.destroy_context()


if __name__ == '__main__':
    if os.environ.get('CYUI_REQUIRE_UI') == '1' and i is None: raise SystemExit('imgui-bundle is required')
    unittest.main(verbosity=2)
