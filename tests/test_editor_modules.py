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
    PropertyObject, PropertyCustom, make_property_array, normalize_property_value,
    property_category_to_json, property_category_from_json, evaluate_property_expression, common_properties)
from cyui_modules.hierarchy import HierarchyItem, HierarchyState, hierarchy_rows, can_reparent_hierarchy, hierarchy
from cyui_modules.dataview import DataColumn, DataRow, DataViewState, data_view_rows, copy_data_cells, paste_data_cells, data_view
from cyui_modules.filepicker import FilePickerState, FilePickerOptions, FileListing, FileEntry, FileFilter, normalize_file_path, validate_file_picker, file_picker
from cyui_modules.commands import Command
from cyui_modules.command_registry import CommandRegistry
from cyui_modules.nodes import Node, Pin, Link, NodeState, node_editor, NodeGroup, NodeReroute, copy_node_selection, nodes_to_json, nodes_from_json, clone_nodes
from cyui_modules.timeline import Lane, Clip, Marker, TimelineState, timeline, split_timeline_clips, duplicate_timeline_clips, reorder_timeline_lane
from cyui_modules.timeline import add_timeline_marker
from cyui_modules.inspector import (PropertySet, PropertyMap, PropertyOptional, PropertyEnum, PropertyMetadata,
    make_property_set, make_property_map, make_property_optional, edit_property_collection, property_value_key,
    property_value_editor)
from cyui_modules.hierarchy import navigate_hierarchy
try:
    import cyui as c
    from imgui_bundle import imgui as i
except ImportError:
    c = i = None


class Models(unittest.TestCase):
    def test_set_map_typed_identity_and_uniqueness(self):
        set_value = make_property_set(0.)
        set_value.values = [1., 2., 3.]
        prop = Property(1, 'Set', set_value)
        self.assertEqual(property_from_json(prop, property_to_json(prop))[0][1].value, set_value)
        self.assertEqual(set_value, replace(set_value, values=[3., 1., 2.]))
        with self.assertRaises(ValueError): normalize_property_value(prop, replace(set_value, values=[0., -0.]))
        with self.assertRaises(ValueError): normalize_property_value(replace(prop, minimum=0., maximum=10.), replace(set_value, values=[10., 256.]))
        vectors = Property(2, 'Vectors', make_property_set(PropertyVector([0., 0.])))
        with self.assertRaises(ValueError): normalize_property_value(vectors, PropertySet([PropertyVector([0., -0.]), PropertyVector([-0., 0.])]))
        mapping = make_property_map(PropertyVector([0., 0.]), PropertyArray(['A']))
        mapping.entries = [(PropertyVector([1., 2.]), PropertyArray(['A'])), (PropertyVector([3., 4.]), PropertyArray(['B']))]
        target = Property(3, 'Map', mapping)
        self.assertEqual(property_from_json(target, property_to_json(target))[0][1].value, mapping)
        self.assertEqual(mapping, replace(mapping, entries=list(reversed(mapping.entries))))
        with self.assertRaises(ValueError): normalize_property_value(target, replace(mapping, entries=[mapping.entries[0], mapping.entries[0]]))
        with self.assertRaises(ValueError): property_from_json(target, property_to_json(replace(target, value=make_property_map('', PropertyArray(['A'])))))
        obj = PropertyObject('Key', {'x': 1., 'y': 2.})
        custom_map = Property(4, 'Struct keys', PropertyMap([(obj, PropertyColor())], obj, PropertyColor()))
        self.assertEqual(property_from_json(custom_map, property_to_json(custom_map))[0][1].value, custom_map.value)

    def test_optional_enums_and_flags(self):
        quality = PropertyEnum('Quality', 1, [(0, 'Low'), (1, 'High')])
        optional = Property(1, 'Optional', make_property_optional(quality))
        self.assertEqual(property_from_json(optional, property_to_json(optional))[0][1].value, optional.value)
        present = replace(optional, value=make_property_optional(quality, True))
        self.assertEqual(property_from_json(optional, property_to_json(present))[0][1].value.value, quality)
        with self.assertRaises(ValueError): property_from_json(optional, property_to_json(replace(optional, value=make_property_optional(''))))
        enum = Property(2, 'Quality', quality)
        with self.assertRaises(ValueError): normalize_property_value(enum, replace(quality, value=99))
        flags = Property(3, 'Flags', PropertyEnum('Channels', 3, [(0, 'None'), (1, 'A'), (2, 'B'), (4, 'C')], True))
        self.assertEqual(property_from_json(flags, property_to_json(flags))[0][1].value.value, 3)
        with self.assertRaises(ValueError): normalize_property_value(flags, replace(flags.value, value=8))
        copied = normalize_property_value(present, present.value); copied.value.value = 0
        self.assertEqual(present.value.value.value, 1)

    def test_struct_metadata_survives_clipboard_and_reordering(self):
        rules = {'id': PropertyMetadata(read_only=True), 'gain': PropertyMetadata(minimum=0., maximum=10.), 'name': PropertyMetadata(choices=('A', 'B'))}
        one = PropertyObject('Record', {'id': 1, 'gain': 2.56, 'name': 'A'}, rules)
        two = PropertyObject('Record', {'id': 2, 'gain': 2.56, 'name': 'B'}, rules)
        prop = Property(1, 'Records', PropertyArray([one, two], one))
        self.assertEqual(property_from_json(prop, property_to_json(prop))[0][1].value, prop.value)
        moved = edit_property_collection(prop, [0], 'move', before=2)[0][1].value
        self.assertEqual([v.fields['id'] for v in moved.values], [2, 1])
        self.assertEqual(edit_property_collection(prop, [0], 'remove')[0][1].value.values[0].fields['id'], 2)
        changed = make_property_array(one, 2); changed.values[1].fields['id'] = 2; changed.values[1].fields['gain'] = 256.
        normalized = normalize_property_value(prop, changed)
        self.assertEqual(normalized.values[1].fields['gain'], 10.)
        changed.values[1].fields['id'] = 9
        with self.assertRaises(ValueError): normalize_property_value(prop, changed)
        changed.values[1].fields['id'] = 2; changed.values[1].fields['name'] = 'C'
        with self.assertRaises(ValueError): normalize_property_value(prop, changed)
        mapping = Property(2, 'Map', PropertyMap([('one', one), ('two', two)], '', one))
        reverse = replace(mapping.value, entries=list(reversed(mapping.value.entries)))
        self.assertEqual(normalize_property_value(mapping, reverse), mapping.value)

    def test_collection_bulk_transactions_are_atomic(self):
        prop = Property(1, 'Values', PropertyArray([2.56, 4., 8.]), minimum=0., maximum=10.)
        value = edit_property_collection(prop, [0, 1], 'expression', 'Value*100')[0][1].value
        self.assertEqual(value.values, [10., 10., 8.]); self.assertEqual(prop.value.values, [2.56, 4., 8.])
        self.assertEqual(edit_property_collection(prop, [0, 2], 'move', before=1)[0][1].value.values, [2.56, 8., 4.])
        self.assertEqual(len(edit_property_collection(prop, [0, 2], 'duplicate')[0][1].value.values), 5)
        with self.assertRaises(ValueError): edit_property_collection(prop, [0, 1], 'expression', 'Value/0')
        with self.assertRaises(ValueError): edit_property_collection(replace(prop, read_only=True), [0], 'remove')
        with self.assertRaises(ValueError): edit_property_collection(prop, [0], 'remove', validate=lambda key, value: 'Host refuses')
        unique = Property(2, 'Set', PropertySet([1., 2.], 0.), minimum=0., maximum=10.)
        with self.assertRaises(ValueError): edit_property_collection(unique, [0, 1], 'expression', 'Value*100')
        with self.assertRaises(ValueError): edit_property_collection(unique, [0], 'duplicate')

    def test_optional_record_keeps_current_readonly_fields(self):
        from copy import deepcopy
        prototype = PropertyObject('Record', {'id': 1, 'gain': 2.56}, {'id': PropertyMetadata(read_only=True)})
        current = deepcopy(prototype); current.fields['id'] = 2
        prop = Property(1, 'Optional record', PropertyOptional(current, prototype))
        self.assertEqual(property_from_json(prop, property_to_json(prop))[0][1].value.value.fields['id'], 2)
        changed = deepcopy(prop); changed.value.value.fields['id'] = 9
        with self.assertRaises(ValueError): property_from_json(prop, property_to_json(changed))

    def test_rich_table_cells_round_trip_and_share_constraints(self):
        values = [PropertyVector([1., 2., 3.]), PropertyColor(), PropertyAsset('Texture', '/Game/Test'), PropertyCustom('Angle', '45'),
                  make_property_array(2.56, 2), PropertySet(['A', 'B'], ''), PropertyMap([('A', 2.56)], '', 0.),
                  make_property_optional(PropertyVector([0., 0., 0.])), PropertyEnum('Quality', 1, [(0, 'Low'), (1, 'High')])]
        columns = [DataColumn(n+1, str(n), True) for n in range(len(values))]
        rows = [DataRow(1, values)]
        state = DataViewState(anchor=1, active=1, anchor_column=0, active_column=len(values)-1)
        edits = paste_data_cells(columns, rows, state, copy_data_cells(rows, state))
        self.assertEqual([edit.value for row, col, edit in edits], values)
        columns[0].metadata = PropertyMetadata(minimum=0., maximum=10.)
        state.active_column = state.anchor_column = 0
        incoming = property_to_json(Property(1, '', PropertyVector([256., -1., 3.])))
        import csv, io
        output = io.StringIO(); csv.writer(output, delimiter='\t', lineterminator='').writerow([incoming])
        self.assertEqual(paste_data_cells(columns, rows, state, output.getvalue())[0][2].value.values, [10., 0., 3.])
        columns[-1].metadata.read_only = True; state.active_column = len(values)-1
        with self.assertRaises(ValueError): paste_data_cells(columns, rows, state, copy_data_cells(rows, state))

    def test_hierarchy_keyboard_and_timeline_marker_creation(self):
        items = [HierarchyItem(1, 0, 'Root'), HierarchyItem(2, 1, 'Child'), HierarchyItem(3, 1, 'Other'), HierarchyItem(4, 0, 'Last')]
        state = HierarchyState(selection=[1], focused=1, collapsed={1})
        navigate_hierarchy(items, state, 'right'); self.assertNotIn(1, state.collapsed)
        navigate_hierarchy(items, state, 'right'); self.assertEqual(state.selection, [2])
        navigate_hierarchy(items, state, 'down', True); self.assertEqual(state.selection, [2, 3])
        navigate_hierarchy(items, state, 'left'); self.assertEqual(state.focused, 1)
        navigate_hierarchy(items, state, 'end'); self.assertEqual(state.selection, [4])
        navigate_hierarchy(items, state, 'select_all'); self.assertEqual(state.selection, [1, 2, 3, 4])
        markers = add_timeline_marker([Marker(1, 1., 'A')], 2.5, lambda: 2, 'B')
        self.assertEqual((markers[-1].id, markers[-1].time), (2, 2.5))
        with self.assertRaises(ValueError): add_timeline_marker(markers, 3., lambda: 2)

    def test_recursive_collections_and_empty_schemas(self):
        record = PropertyObject('SpawnPoint', {'name': 'A', 'position': PropertyVector([1., 2., 3.]), 'weights': make_property_array(PropertyFloat(.5), 3)})
        prop = Property(1, 'Records', make_property_array(record, 2))
        self.assertEqual(property_from_json(prop, property_to_json(prop))[0][1].value, prop.value)
        empty = replace(prop, value=make_property_array(PropertyVector([0., 0., 0.])))
        self.assertEqual(property_from_json(empty, property_to_json(empty))[0][1].value, empty.value)
        with self.assertRaises(ValueError): property_from_json(empty, property_to_json(replace(empty, value=make_property_array(''))))
        wrong = make_property_array(record, 2); wrong.values[1].fields['name'] = 42.
        with self.assertRaises(ValueError): property_from_json(prop, property_to_json(replace(prop, value=wrong)))
        self.assertFalse(common_properties([[prop], [replace(prop, value=make_property_array(PropertyObject('SpawnPoint', {'name': 42.}), 1))]]))
        large = replace(prop, value=make_property_array(2.56, 10000))
        self.assertEqual(len(property_from_json(large, property_to_json(large))[0][1].value.values), 10000)
        self.assertIsNot(prop.value.values[0], prop.value.values[1])

    def test_custom_collection_payloads_and_recursive_bounds(self):
        prop = Property(1, 'Angles', make_property_array(PropertyCustom('Angle', '45'), 2))
        self.assertEqual(property_from_json(prop, property_to_json(prop))[0][1].value, prop.value)
        with self.assertRaises(ValueError): property_from_json(prop, property_to_json(replace(prop, value=make_property_array(PropertyCustom('Time', '45'), 2))))
        with self.assertRaises(ValueError): normalize_property_value(prop, make_property_array(PropertyCustom('Angle', '{broken}'), 1))
        vector = Property(2, 'Nested', make_property_array(make_property_array(PropertyVector([1., 2., 3.]), 2), 2), minimum=0., maximum=10.)
        changed = make_property_array(make_property_array(PropertyVector([256., -1., 3.]), 2), 2)
        self.assertEqual(normalize_property_value(vector, changed).values[0].values[0].values, [10., 0., 3.])
        deep = 1.
        for _ in range(14): deep = PropertyArray([deep])
        with self.assertRaises(ValueError): normalize_property_value(Property(3, 'Deep', deep), deep)

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
    def test_flags_popup_keeps_open_after_toggle(self):
        i.create_context(); io_ = i.get_io(); io_.set_ini_filename(None); io_.display_size = (1200, 900); io_.delta_time = 1/60
        io_.backend_flags |= i.BackendFlags_.renderer_has_textures; c.apply()
        prop = Property(1, 'Flags', PropertyEnum('Flags', 3, [(1, 'A'), (2, 'B')], True)); state = InspectorState()
        location = None; choice = None; selectable = i.selectable
        def observe_choice(label, *args, **kwargs):
            nonlocal choice
            result = selectable(label, *args, **kwargs)
            if label == c.tr_label('B'):
                lo, hi = i.get_item_rect_min(), i.get_item_rect_max(); choice = ((lo.x+hi.x)/2, (lo.y+hi.y)/2)
            return result
        i.selectable = observe_choice
        def frame():
            nonlocal location
            i.new_frame(); i.set_next_window_pos((0, 0)); i.set_next_window_size((1100, 850)); i.begin('Flags')
            edit = property_value_editor('flags', state, prop)
            if edit.committed: prop.value = edit.value
            lo, hi = i.get_item_rect_min(), i.get_item_rect_max(); location = ((lo.x+hi.x)/2, (lo.y+hi.y)/2)
            i.end(); i.render()
        try:
            frame(); frame(); io_.add_mouse_pos_event(*location); io_.add_mouse_button_event(0, True); frame()
            io_.add_mouse_button_event(0, False); frame(); frame()
            self.assertIsNotNone(choice); io_.add_mouse_pos_event(*choice)
            io_.add_mouse_button_event(0, True); frame(); io_.add_mouse_button_event(0, False); frame(); frame()
            self.assertEqual(prop.value.value, 1)
            self.assertTrue(i.is_popup_open('', i.PopupFlags_.any_popup_id))
        finally: i.selectable = selectable; i.destroy_context()

    def test_completion_widgets_every_theme_and_density(self):
        i.create_context(); io_ = i.get_io(); io_.set_ini_filename(None); io_.display_size = (1200, 900); io_.delta_time = 1/60
        io_.backend_flags |= i.BackendFlags_.renderer_has_textures; c.apply()
        def frame(draw):
            i.new_frame(); i.set_next_window_size((1100, 850)); i.begin('Completion'); draw(); i.end(); i.render()
        try:
            kinds = [PropertySet([1, 2, 3], 0), PropertyMap([('A', PropertyVector([1., 2., 3.])), ('B', PropertyVector([4., 5., 6.]))], '', PropertyVector([0., 0., 0.])),
                     make_property_optional(2.56), make_property_optional(PropertyVector([1., 2., 3.]), True),
                     PropertyEnum('Quality', 1, [(0, 'Low'), (1, 'High')]), PropertyEnum('Flags', 3, [(1, 'A'), (2, 'B')], True)]
            for theme in ('mid-grey', 'light-grey', 'amber-night', 'violet', 'navy'):
                c.set_theme(theme)
                for density in ('compact', 'comfortable'):
                    c.set_density(density)
                    for kind in kinds:
                        state = InspectorState(); prop = Property(1, 'Value', kind)
                        frame(lambda: inspector('completion', state, [prop], array_height=100))
                        frame(lambda: inspector('completion', state, [prop], array_height=100))
                    for cell in kinds + [PropertyColor(), PropertyAsset('Texture', '/Game/Test'), PropertyVector([1., 2., 3.])]:
                        state = DataViewState(editing=(1, 0))
                        frame(lambda: data_view('rich', state, [DataColumn(1, 'Value', True)], [DataRow(1, [cell])]))
                        frame(lambda: data_view('rich', state, [DataColumn(1, 'Value', True)], [DataRow(1, [cell])]))
            for kind in (PropertySet(list(range(100000)), 0), PropertyMap([(n, n*.5) for n in range(100000)], 0, 0.)):
                state = InspectorState(); prop = Property(2, 'Large', kind)
                frame(lambda: inspector('largeCompletion', state, [prop], array_height=150))
                frame(lambda: inspector('largeCompletion', state, [prop], array_height=150))
                self.assertLess(next(iter(state.lists.values()))['rendered_rows'], 20)
        finally: i.destroy_context()

    def test_live_collection_selection_filter_and_drag(self):
        from cyui_modules.common import Edit
        i.create_context(); io_ = i.get_io(); io_.set_ini_filename(None); io_.display_size = (1200, 900); io_.delta_time = 1/60
        io_.backend_flags |= i.BackendFlags_.renderer_has_textures; c.apply()
        state = InspectorState(); prop = Property(1, 'Rows', PropertyArray([PropertyCustom('Counter', str(n)) for n in range(8)]))
        window = None
        def editor(owner, path, value):
            nonlocal window
            window = i.internal.get_current_window(); c.button(value.json); return Edit()
        def draw():
            for _, edit in inspector('interactiveCollection', state, [prop], edit_custom=editor):
                if edit.committed: prop.value = edit.value
        def frame():
            i.new_frame(); i.set_next_window_pos((0, 0)); i.set_next_window_size((1100, 850)); i.begin('Selection'); draw(); i.end(); i.render()
        def click(row):
            io_.add_mouse_pos_event(window.pos.x+c.dp(20), window.pos.y+i.get_style().window_padding.y+i.get_frame_height()*.5+row*i.get_frame_height_with_spacing())
            io_.add_mouse_button_event(0, True); frame(); io_.add_mouse_button_event(0, False); frame()
        try:
            frame(); frame(); click(0)
            listing = next(iter(state.lists.values())); self.assertEqual(listing['selection'], [0])
            io_.add_key_event(i.Key.mod_ctrl, True); frame(); click(2)
            io_.add_key_event(i.Key.mod_ctrl, False); frame(); self.assertEqual(listing['selection'], [0, 2])
            io_.add_key_event(i.Key.mod_shift, True); frame(); click(4)
            io_.add_key_event(i.Key.mod_shift, False); frame(); self.assertEqual(listing['selection'], [2, 3, 4])
            io_.add_key_event(i.Key.delete, True); frame(); io_.add_key_event(i.Key.delete, False); frame()
            self.assertEqual([v.json for v in prop.value.values], ['0', '1', '5', '6', '7'])
            listing['query'] = '"value":7'; frame(); self.assertEqual(listing['visible'], [4])
            listing['query'] = ''; frame(); frame(); click(0)
            x = window.pos.x+c.dp(20); y = window.pos.y+i.get_style().window_padding.y+i.get_frame_height()*.5
            io_.add_mouse_pos_event(x, y); io_.add_mouse_button_event(0, True); frame()
            io_.add_mouse_pos_event(x, y+3*i.get_frame_height_with_spacing()); frame(); frame()
            io_.add_mouse_button_event(0, False); frame(); frame()
            self.assertEqual([v.json for v in prop.value.values], ['1', '5', '0', '6', '7'])
        finally: i.destroy_context()

    def test_node_group_contents_and_rich_table_commit(self):
        from cyui_modules.common import Edit
        i.create_context(); io_ = i.get_io(); io_.set_ini_filename(None); io_.display_size = (1200, 900); io_.delta_time = 1/60
        io_.backend_flags |= i.BackendFlags_.renderer_has_textures; c.apply()
        def frame(draw):
            i.new_frame(); i.set_next_window_pos((0, 0)); i.set_next_window_size((1100, 850)); i.begin('Gestures'); draw(); i.end(); i.render()
        try:
            nodes = [Node(1, (50, 80), 'Inside'), Node(2, (600, 80), 'Outside')]
            groups = [NodeGroup(10, (20, 20), (400, 240), 'Group')]; routes = [NodeReroute(20, 1, (180, 100)), NodeReroute(21, 1, (600, 100))]
            state = NodeState(); origin = None; result = None
            def draw():
                nonlocal origin, result
                cursor = i.get_cursor_screen_pos(); origin = (cursor.x, cursor.y)
                result = node_editor('groupMove', state, nodes, [], (1000, 330), groups=groups, reroutes=routes)
            frame(draw); frame(draw)
            io_.add_mouse_pos_event(origin[0]+30, origin[1]+30); io_.add_mouse_button_event(0, True); frame(draw)
            self.assertEqual(result['moved'].phase, 'begin')
            io_.add_mouse_pos_event(origin[0]+80, origin[1]+55); frame(draw)
            io_.add_mouse_button_event(0, False); frame(draw)
            self.assertEqual(result['moved'].phase, 'commit'); self.assertEqual(result['moved'].value, {1: (100, 105)})
            self.assertEqual(result['reroutes'].value[0].position, (230, 125)); self.assertEqual(result['reroutes'].value[1].position, (600, 100))
            self.assertEqual(nodes[0].position, (50, 80))
            state = DataViewState(editing=(11, 0)); rows = [DataRow(11, [PropertyCustom('Angle', '45')])]; location = None
            def editor(owner, path, value):
                nonlocal location
                self.assertEqual((owner, path), (8, ('11',)))
                clicked = c.button(value.json); lo, hi = i.get_item_rect_min(), i.get_item_rect_max(); location = ((lo.x+hi.x)/2, (lo.y+hi.y)/2)
                return Edit('commit', PropertyCustom(value.type, '60')) if clicked else Edit()
            def grid():
                for row, col, edit in data_view('richCommit', state, [DataColumn(8, 'Angle', True)], rows, edit_custom=editor):
                    if edit.committed: rows[0].cells[0] = edit.value
            frame(grid); frame(grid); io_.add_mouse_pos_event(*location); io_.add_mouse_button_event(0, True); frame(grid)
            io_.add_mouse_button_event(0, False); frame(grid)
            self.assertEqual(rows[0].cells[0].json, '60')
        finally: i.destroy_context()

    def test_virtual_collections_all_types_and_large_models(self):
        from cyui_modules.common import Edit
        i.create_context(); io_ = i.get_io(); io_.set_ini_filename(None); io_.display_size = (1200, 900); io_.delta_time = 1/60
        io_.backend_flags |= i.BackendFlags_.renderer_has_textures
        c.apply()
        def frame(draw):
            i.new_frame(); i.set_next_window_pos((0, 0)); i.set_next_window_size((1100, 850)); i.begin('Arrays'); draw(); i.end(); i.render()
        try:
            record = PropertyObject('Record', {'name': 'A', 'position': PropertyVector([1., 2., 3.]), 'weights': make_property_array(PropertyFloat(.5), 3)})
            kinds = [True, (1 << 63)-1, 2.56, PropertyFloat(2.56), 'Text', PropertyVector([1., 2.]), PropertyVector([1., 2., 3.]), PropertyVector([1., 2., 3., 4.]), PropertyColor(), PropertyAsset('Texture', '/Game/Test'), make_property_array(2.56, 3), record, PropertyCustom('Angle', '45')]
            for theme in ('mid-grey', 'light-grey', 'amber-night', 'violet', 'navy'):
                c.set_theme(theme)
                for kind in kinds:
                    state = InspectorState(); prop = Property(1, 'Collection', make_property_array(kind, 100))
                    frame(lambda: inspector('typed', state, [prop], array_height=150))
                    frame(lambda: inspector('typed', state, [prop], array_height=150))
                    listing = next(iter(state.lists.values()))
                    self.assertGreater(listing['rendered_rows'], 0); self.assertLess(listing['rendered_rows'], 20)
            state = InspectorState(); prop = Property(500, 'Large', make_property_array(PropertyCustom('Counter', '0'), 100000))
            rendered = []; first = None; window = None
            def editor(owner, path, value):
                nonlocal first, window
                self.assertEqual(owner, 500); self.assertEqual(len(path), 1)
                rendered.append(path[0]); window = i.internal.get_current_window()
                clicked = c.button(value.json+'##counter')
                if path[0] == '0':
                    lo, hi = i.get_item_rect_min(), i.get_item_rect_max(); first = ((lo.x+hi.x)/2, (lo.y+hi.y)/2)
                return Edit('commit', PropertyCustom(value.type, '1')) if clicked else Edit()
            def draw():
                rendered.clear()
                for _, edit in inspector('large', state, [prop], array_height=150, edit_custom=editor):
                    if edit.committed: prop.value = edit.value
            frame(draw); frame(draw)
            draft = state.drafts[500]
            frame(draw); self.assertIs(draft, state.drafts[500])
            self.assertLess(len(rendered), 20)
            self.assertAlmostEqual(window.content_size.y, i.get_frame_height_with_spacing()*100000, delta=i.get_frame_height_with_spacing()*4)
            io_.add_mouse_pos_event(*first); io_.add_mouse_button_event(0, True); frame(draw)
            window.scroll.y = window.scroll_max.y*.5
            frame(draw); frame(draw)
            self.assertIn('0', rendered); self.assertLess(len(rendered), 22)
            io_.add_mouse_button_event(0, False); frame(draw)
            window.scroll.y = 0; frame(draw); frame(draw)
            io_.add_mouse_pos_event(*first); io_.add_mouse_button_event(0, True); frame(draw)
            io_.add_mouse_button_event(0, False); frame(draw)
            self.assertEqual(prop.value.values[0].json, '1')
        finally: i.destroy_context()

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
