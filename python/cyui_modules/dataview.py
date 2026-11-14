"""Sortable virtual tables and atomic typed TSV paste. Models perform no I/O."""
from dataclasses import dataclass, field, replace
import csv
import io
import math
from functools import cmp_to_key
from .common import ui, item_id, Edit
from .numeric import evaluate_numeric_expression
from .inspector import Property, PropertyMetadata, InspectorState, property_value_editor
from .inspector_data import (PropertyFloat, PropertyEnum, PropertyVector, PropertyColor, property_value_text, property_value_key,
                            property_to_json, property_from_json, normalize_property_value, evaluate_property_expression)


@dataclass
class DataColumn:
    id: int
    label: str
    editable: bool = False
    minimum: object = None
    maximum: object = None
    width: float = 140
    metadata: PropertyMetadata = field(default_factory=PropertyMetadata)


@dataclass
class DataRow:
    id: int
    cells: list
    read_only: bool = False


@dataclass
class DataViewState:
    filters: list = field(default_factory=list)
    sort: list = field(default_factory=list)  # (column index, descending)
    column_order: list = field(default_factory=list)
    anchor: int = 0
    active: int = 0
    anchor_column: int = 0
    active_column: int = 0
    editing: object = None
    draft: str = ''
    error: str = ''
    revision: int = 0
    editors: InspectorState = field(default_factory=InspectorState)
    rich_property: object = None
    edit_focus: bool = False


def data_cell_text(value): return property_value_text(value)


def _clipboard_cell(value):
    return data_cell_text(value) if type(value) in (str, bool, int, float) or isinstance(value, PropertyFloat) else property_to_json(Property(0, '', value))


def _column_property(column, value):
    m = column.metadata
    return Property(column.id, column.label, value, m.help, not column.editable or m.read_only, m.password, False,
                    m.minimum if m.minimum is not None else column.minimum, m.maximum if m.maximum is not None else column.maximum, m.choices)


def data_view_rows(rows, state):
    result = [n for n, row in enumerate(rows) if all(not f or (c < len(row.cells) and f.casefold() in data_cell_text(row.cells[c]).casefold()) for c, f in enumerate(state.filters))]
    def compare(a, b):
        for column, descending in state.sort:
            if column >= len(rows[a].cells) or column >= len(rows[b].cells): continue
            x, y = rows[a].cells[column], rows[b].cells[column]
            if type(x) is type(y):
                if type(x) is float and (math.isnan(x) or math.isnan(y)): result = int(math.isnan(x))-int(math.isnan(y))
                elif type(x) in (str, bool, int, float): result = (x > y)-(x < y)
                else:
                    def key(value):
                        if isinstance(value, PropertyEnum): return value.value
                        if isinstance(value, PropertyFloat): return value.value
                        if isinstance(value, PropertyVector): return tuple(value.values)
                        if isinstance(value, PropertyColor): return tuple(value.rgba)
                        return property_value_key(value)
                    result = (key(x) > key(y))-(key(x) < key(y))
            else: result = (type(x).__name__ > type(y).__name__)-(type(x).__name__ < type(y).__name__)
            if result: return -result if descending else result
        return 0
    return sorted(result, key=cmp_to_key(compare))


def _columns(state, count):
    return list(dict.fromkeys(c for c in state.column_order if 0 <= c < count)) if state.column_order else list(range(count))


def _range(rows, state):
    order = data_view_rows(rows, state)
    a = next((i for i, n in enumerate(order) if rows[n].id == state.anchor), len(order))
    b = next((i for i, n in enumerate(order) if rows[n].id == state.active), len(order))
    columns = _columns(state, max((len(row.cells) for row in rows), default=0))
    if state.anchor_column not in columns or state.active_column not in columns:
        lo = hi = len(columns)
    else:
        lo, hi = sorted((columns.index(state.anchor_column), columns.index(state.active_column)))
    return order, min(a, b), max(a, b), lo, hi


def copy_data_cells(rows, state):
    order, a, b, lo, hi = _range(rows, state)
    if b >= len(order): return ''
    columns = _columns(state, max((len(row.cells) for row in rows), default=0))
    if hi >= len(columns): return ''
    output = io.StringIO(newline=''); writer = csv.writer(output, delimiter='\t', lineterminator='\n')
    for n in order[a:b+1]: writer.writerow([_clipboard_cell(rows[n].cells[col]) if col < len(rows[n].cells) else '' for col in columns[lo:hi+1]])
    text = output.getvalue().removesuffix('\n')
    if len(text.encode('utf8')) > 1024*1024: raise ValueError('Clipboard exceeds 1 MiB')
    return text


def _convert(column, original, text):
    prop = _column_property(column, original)
    if prop.read_only: raise ValueError('Column is read-only')
    if type(original) is str: return normalize_property_value(prop, text)
    if type(original) is bool:
        if text not in ('true', 'false', '1', '0'): raise ValueError('Expected true or false')
        return normalize_property_value(prop, text in ('true', '1'))
    if type(original) in (int, float) or isinstance(original, PropertyFloat): return evaluate_property_expression(prop, text)
    return property_from_json(prop, text)[0][1].value


def paste_data_cells(columns, rows, state, text, read_only=False, validate=None):
    if read_only: raise ValueError('Table is read-only')
    if len(text.encode('utf8')) > 1024*1024: raise ValueError('Clipboard exceeds 1 MiB')
    values = list(csv.reader(io.StringIO(text, newline=''), delimiter='\t', strict=True))
    order, a, _, first, _ = _range(rows, state)
    column_order = _columns(state, len(columns))
    if not values or a+len(values) > len(order): raise ValueError('Paste exceeds visible rows')
    result = []
    for n, cells in enumerate(values):
        row = rows[order[a+n]]
        if row.read_only or first+len(cells) > len(column_order): raise ValueError('Paste exceeds editable cells')
        for offset, text in enumerate(cells):
            col = column_order[first+offset]
            if col >= len(row.cells): raise ValueError('Paste exceeds editable cells')
            column = columns[col]
            if not column.editable: raise ValueError('Column is read-only')
            value = _convert(column, row.cells[col], text)
            error = validate(row.id, column.id, value) if validate else ''
            if error: raise ValueError(error)
            result.append((row.id, column.id, Edit('commit', value)))
    return result


def data_view(identifier, state, columns, rows, size=(0, 350), read_only=False, validate=None, revision=0, edit_custom=None, browse_asset=None):
    i, c = ui(); events = []; i.push_id(identifier)
    if state.revision != revision: state.editing = None; state.revision = revision; state.editors = InspectorState(); state.rich_property = None
    state.filters = (state.filters+['']*len(columns))[:len(columns)]
    flags = i.TableFlags_.borders | i.TableFlags_.row_bg | i.TableFlags_.resizable | i.TableFlags_.reorderable | i.TableFlags_.hideable | i.TableFlags_.sortable | i.TableFlags_.sort_multi | i.TableFlags_.scroll_y | i.TableFlags_.scroll_x
    if columns and i.begin_table('##data', len(columns), flags, size):
        for n, column in enumerate(columns): i.table_setup_column(c.tr_label(column.label), i.TableColumnFlags_.width_fixed, c.dp(column.width), n)
        i.table_setup_scroll_freeze(0, 2); i.table_headers_row()
        specs = i.table_get_sort_specs()
        if specs and specs.specs_dirty:
            state.sort = [(specs.get_specs(n).column_user_id, specs.get_specs(n).sort_direction == i.SortDirection.descending) for n in range(specs.specs_count)]
            specs.specs_dirty = False
        i.table_next_row()
        positions = []
        for n in range(len(columns)):
            i.table_set_column_index(n)
            if not (i.table_get_column_flags(n) & i.TableColumnFlags_.is_enabled): continue
            positions.append((i.get_cursor_screen_pos().x, n))
            i.push_id(n); i.set_next_item_width(-1)
            _, state.filters[n] = i.input_text('##filter', state.filters[n]); i.pop_id()
        state.column_order = [n for _, n in sorted(positions)]
        order, a, b, lo, hi = _range(rows, state); clip = i.ListClipper(); clip.begin(len(order))
        if state.editing:
            active = next((n for n, row in enumerate(order) if rows[row].id == state.editing[0]), None)
            if active is not None: clip.include_item_by_index(active)
        while clip.step():
            for n in range(clip.display_start, clip.display_end):
                row = rows[order[n]]; i.push_id(item_id(row.id)); i.table_next_row(0, i.get_frame_height_with_spacing())
                for col, cell in enumerate(row.cells[:len(columns)]):
                    if not i.table_set_column_index(col): continue
                    i.push_id(col)
                    if state.editing == (row.id, col):
                        if (type(cell) not in (str, bool, int, float) and not isinstance(cell, PropertyFloat)) or columns[col].metadata.choices:
                            if state.rich_property is None: state.rich_property = _column_property(columns[col], cell)
                            prop = state.rich_property; prop.read_only = read_only or row.read_only or not columns[col].editable or columns[col].metadata.read_only
                            i.set_next_item_width(-1)
                            if state.edit_focus: i.set_keyboard_focus_here(); state.edit_focus = False
                            custom = (lambda owner, path, value: edit_custom(owner, (str(row.id),)+path, value)) if edit_custom else None
                            browse = (lambda owner, path: browse_asset(owner, (str(row.id),)+path)) if browse_asset else None
                            edit = property_value_editor('##richCell', state.editors, prop, revision,
                                (lambda owner, value: validate(row.id, columns[col].id, value)) if validate else None,
                                edit_custom=custom, browse_asset=browse)
                            state.error = state.editors.errors.get(prop.id, '')
                            if edit.phase != 'none': events.append((row.id, columns[col].id, edit))
                            if edit.committed or edit.phase == 'cancel' or prop.read_only or i.is_key_pressed(i.Key.escape): state.editing = None
                            i.pop_id(); continue
                        if state.edit_focus: i.set_keyboard_focus_here(); state.edit_focus = False
                        accepted, state.draft = i.input_text('##edit', state.draft, i.InputTextFlags_.enter_returns_true)
                        if accepted and not read_only and not row.read_only and columns[col].editable:
                            try:
                                value = _convert(columns[col], cell, state.draft)
                                state.error = validate(row.id, columns[col].id, value) if validate else ''
                                if not state.error: events.append((row.id, columns[col].id, Edit('commit', value))); state.editing = None
                            except ValueError as error: state.error = str(error)
                        if i.is_key_pressed(i.Key.escape) or read_only: state.editing = None
                    else:
                        position = state.column_order.index(col) if col in state.column_order else len(state.column_order)
                        clicked, _ = i.selectable(data_cell_text(cell)+'###cell', a <= n <= b and lo <= position <= hi, i.SelectableFlags_.allow_double_click, (0, i.get_frame_height()))
                        if clicked:
                            if not i.get_io().key_shift: state.anchor, state.anchor_column = row.id, col
                            state.active, state.active_column = row.id, col
                            if i.is_mouse_double_clicked(0) and columns[col].editable and not columns[col].metadata.read_only and not read_only and not row.read_only:
                                state.editing, state.draft = (row.id, col), data_cell_text(cell); state.edit_focus = True
                                state.editors = InspectorState(); state.rich_property = None
                    i.pop_id()
                i.pop_id()
        if i.is_window_focused(i.FocusedFlags_.child_windows) and not i.get_io().want_text_input and i.get_io().key_ctrl:
            try:
                if i.is_key_pressed(i.Key.c): i.set_clipboard_text(copy_data_cells(rows, state))
                if i.is_key_pressed(i.Key.v): events += paste_data_cells(columns, rows, state, i.get_clipboard_text(), read_only, validate)
            except ValueError as error: state.error = str(error)
        i.end_table()
    if state.error: c.text_coloured(c.palette().error, state.error)
    i.pop_id(); return events
