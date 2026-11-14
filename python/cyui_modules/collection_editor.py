"""Virtual, selectable Array/Set/Map rows; all mutations are validated parent transactions."""
from copy import deepcopy
from dataclasses import replace
from .common import ui
from .inspector_data import (PropertyArray, PropertySet, PropertyMap, PropertyFloat, property_value_key,
    property_value_text, property_to_json, normalize_property_value, edit_property_collection)


def draw_collection(collection, root, state, path, height, edit_custom, browse_asset):
    from .property_editor import DrawEdit, draw_property_value
    i, c = ui(); out = DrawEdit(); list_id = i.get_id('##array')
    listing = state.lists.setdefault(list_id, {})
    defaults = dict(active_row=-1, rendered_rows=0, selection=[], visible=[], anchor=0, indexed_size=-1,
                    query='', indexed_query='', bulk_text='', error='')
    for key, value in defaults.items(): listing.setdefault(key, value)
    listing['rendered_rows'] = 0
    is_map = isinstance(collection, PropertyMap); is_array = isinstance(collection, PropertyArray)
    values = collection.entries if is_map else collection.values
    i.set_next_item_width(-1); _, listing['query'] = i.input_text_with_hint('##collectionSearch', c.tr('Filter collection...'), listing['query'])
    if i.is_item_hovered(): i.set_tooltip(c.tr('Filter collection...'))
    if listing['indexed_size'] != len(values) or listing['indexed_query'] != listing['query']:
        if listing['indexed_query'] != listing['query']: listing['selection'].clear()
        query = listing['query'].casefold()
        listing['visible'] = [n for n, value in enumerate(values) if not query or query in property_value_key(value[1] if is_map else value).casefold() or (is_map and query in property_value_key(value[0]).casefold())]
        listing['indexed_size'] = len(values); listing['indexed_query'] = listing['query']
        listing['selection'] = [n for n in listing['selection'] if n < len(values)]
    selection = listing['selection']; visible = listing['visible']; action = None; before = None; text = ''
    def pick(row, extend, additive):
        if extend:
            anchor = visible.index(listing['anchor']) if listing['anchor'] in visible else visible.index(row)
            at = visible.index(row)
            if not additive: selection.clear()
            selection.extend(n for n in visible[min(anchor, at):max(anchor, at)+1] if n not in selection)
        else:
            if not additive: selection[:] = [row]
            elif row in selection: selection.remove(row)
            else: selection.append(row)
            listing['anchor'] = row
    if i.begin_child('##array', (0, c.dp(max(60, height))), i.ChildFlags_.borders):
        pitch = i.get_frame_height_with_spacing(); clip = i.ListClipper(); clip.begin(len(visible), pitch)
        if listing['active_row'] in visible: clip.include_item_by_index(visible.index(listing['active_row']))
        if i.is_window_focused(i.FocusedFlags_.child_windows) and not i.get_io().want_text_input:
            if i.get_io().key_ctrl and i.is_key_pressed(i.Key.a): selection[:] = visible
            if not root.read_only and selection and i.is_key_pressed(i.Key.delete): action = 'remove'
        while clip.step():
            for at in range(clip.display_start, clip.display_end):
                row = visible[at]; listing['rendered_rows'] += 1; i.push_id(row); y = i.get_cursor_pos_y()
                clicked, _ = i.selectable(str(row), row in selection, 0, (c.dp(44), i.get_frame_height()))
                if clicked: pick(row, i.get_io().key_shift, i.get_io().key_ctrl)
                context = i.is_item_clicked(1)
                if is_array and not root.read_only and i.begin_drag_drop_source():
                    if row not in selection: selection[:] = [row]
                    state.drag_collection = list_id; i.set_drag_drop_payload_py_id('CYUI_PROPERTY_ROWS', list_id)
                    c.text_dim(str(len(selection))); i.end_drag_drop_source()
                if is_array and not root.read_only and i.begin_drag_drop_target():
                    payload = i.accept_drag_drop_payload_py_id('CYUI_PROPERTY_ROWS')
                    if payload is not None and state.drag_collection == list_id: action, before = 'move', row
                    i.end_drag_drop_target()
                i.same_line(c.dp(52)); width = max(c.dp(40), i.get_content_region_avail().x-c.dp(30))
                if is_map:
                    key, value = values[row]; edit = DrawEdit(); parts = []
                    for name, part in [('key', key), ('value', value)]:
                        if parts: i.same_line(0, i.get_style().item_inner_spacing.x)
                        i.push_id(name); i.set_next_item_width((width-i.get_style().item_inner_spacing.x)*.5)
                        change, part = draw_property_value(part, root, state, path+(str(row), name), height, edit_custom, browse_asset)
                        edit.merge(change); parts.append(part); i.pop_id()
                    values[row] = tuple(parts)
                else:
                    i.set_next_item_width(width)
                    edit, values[row] = draw_property_value(values[row], root, state, path+(str(row),), height, edit_custom, browse_asset)
                out.merge(edit)
                if edit.changed: listing['indexed_size'] = -1
                if edit.active or edit.activated: listing['active_row'] = row
                elif listing['active_row'] == row: listing['active_row'] = -1
                context |= i.is_item_hovered(i.HoveredFlags_.allow_when_disabled) and i.is_mouse_released(1)
                if context: i.open_popup('##elementMenu')
                if i.begin_popup('##elementMenu'):
                    if row not in selection: selection[:] = [row]
                    if i.menu_item(c.tr_label('Copy as JSON'), '', False, not root.password)[0]:
                        try: i.set_clipboard_text(property_to_json(replace(root, value=values[row][1] if is_map else values[row])))
                        except ValueError as error: state.clipboard_error = str(error)
                    if i.menu_item(c.tr_label('Paste to selection'), '', False, not root.read_only)[0]: action, text = 'assign', i.get_clipboard_text()
                    if i.menu_item(c.tr_label('Duplicate selection'), '', False, not root.read_only and is_array)[0]: action = 'duplicate'
                    if i.menu_item(c.tr_label('Move up'), '', False, not root.read_only and is_array and row > 0)[0]: action, before = 'move', row-1
                    if i.menu_item(c.tr_label('Move down'), '', False, not root.read_only and is_array and row+1 < len(values))[0]: action, before = 'move', row+2
                    if i.menu_item(c.tr_label('Remove selection'), '', False, not root.read_only)[0]: action = 'remove'
                    i.end_popup()
                i.same_line(); i.begin_disabled(root.read_only)
                if c.ghost_button('-'): selection[:] = [row]; action = 'remove'
                i.end_disabled(); i.set_cursor_pos_y(y+pitch-i.get_style().item_spacing.y); i.dummy((0, 0)); i.pop_id()
        if is_array and i.get_drag_drop_payload_py_id() is not None:
            i.selectable(c.tr_label('Drop at end'), False)
            if i.begin_drag_drop_target():
                payload = i.accept_drag_drop_payload_py_id('CYUI_PROPERTY_ROWS')
                if payload is not None and state.drag_collection == list_id: action, before = 'move', len(values)
                i.end_drag_drop_target()
    i.end_child(); i.begin_disabled(root.read_only)
    add = c.ghost_button(c.tr_label('Add element')); i.end_disabled()
    if add:
        if is_array:
            source = collection.element_template if collection.element_template is not None else (values[-1] if values else '')
            values.append(deepcopy(source)); out.changed = out.immediate = True; listing['indexed_size'] = -1
        else:
            if is_map:
                key = collection.key_template if collection.key_template is not None else (values[-1][0] if values else '')
                value = collection.value_template if collection.value_template is not None else (values[-1][1] if values else '')
                pending = [deepcopy(key), deepcopy(value)]
            else: pending = deepcopy(collection.element_template if collection.element_template is not None else (values[-1] if values else ''))
            state.pending[list_id] = pending; listing['error'] = ''; i.open_popup('##addElement')
    i.same_line(); i.begin_disabled(root.read_only or not selection)
    if c.ghost_button(c.tr_label('Edit selection...')):
        value = values[selection[0]][1] if is_map else values[selection[0]]
        listing['bulk_text'] = property_value_text(value) if type(value) in (int, float) or isinstance(value, PropertyFloat) else property_to_json(replace(root, value=value))
        i.open_popup('##bulk')
    i.end_disabled(); i.same_line(); c.text_dim(str(len(selection))+' / '+str(len(values)))
    i.set_next_window_size((c.dp(520), 0), i.Cond_.appearing)
    if i.begin_popup('##addElement'):
        if list_id not in state.pending: i.close_current_popup()
        else:
            pending = state.pending[list_id]
            if is_map:
                for n, name in enumerate(('key', 'value')):
                    i.push_id(name); c.text_dim(c.tr('Key' if n == 0 else 'Value')); i.set_next_item_width(-1)
                    _, pending[n] = draw_property_value(pending[n], root, state, path+('new', name), height, edit_custom, browse_asset); i.pop_id()
            else:
                i.set_next_item_width(-1); _, pending = draw_property_value(pending, root, state, path+('new',), height, edit_custom, browse_asset)
                state.pending[list_id] = pending
            if c.button(c.tr_label('Apply')) and not root.read_only:
                candidate = deepcopy(collection)
                if is_map: candidate.entries.append(tuple(pending))
                else: candidate.values.append(pending)
                try:
                    collection = normalize_property_value(replace(root, value=collection), candidate)
                    out.changed = out.immediate = True; listing['indexed_size'] = -1; i.close_current_popup()
                except ValueError as error: listing['error'] = str(error)
            out.active |= not out.immediate
        if listing['error']: c.text_coloured(c.palette().error, listing['error'])
        if i.is_key_pressed(i.Key.escape): i.close_current_popup()
        i.end_popup()
    if i.begin_popup('##bulk'):
        if not selection: i.close_current_popup()
        c.text_dim(c.tr('Enter an expression or typed property JSON.')); i.set_next_item_width(c.dp(420))
        accepted, listing['bulk_text'] = i.input_text('##bulkText', listing['bulk_text'], i.InputTextFlags_.enter_returns_true)
        if (accepted or c.button(c.tr_label('Apply'))) and selection and not root.read_only:
            value = values[selection[0]][1] if is_map else values[selection[0]]
            text = listing['bulk_text']
            action = 'expression' if not text.lstrip().startswith('{') and (type(value) in (int, float) or isinstance(value, PropertyFloat)) else 'assign'
        if i.is_key_pressed(i.Key.escape): i.close_current_popup()
        out.active |= not out.immediate; i.end_popup()
    if action:
        try:
            collection = edit_property_collection(replace(root, value=collection), selection, action, text, before)[0][1].value
            out.changed = out.immediate = True; listing['indexed_size'] = -1; listing['active_row'] = -1
            selection.clear(); state.elements.clear(); i.internal.clear_active_id(); listing['error'] = ''
        except ValueError as error: listing['error'] = str(error)
    if listing['error']: c.text_coloured(c.palette().error, listing['error'])
    return out, collection
