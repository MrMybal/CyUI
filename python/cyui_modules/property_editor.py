"""Fixed-height typed collection rows; only visible rows and the active row are submitted."""
from copy import deepcopy
from dataclasses import dataclass, replace
from .common import ui
from .inspector_data import (PropertyArray, PropertyObject, PropertyCustom, PropertyVector,
    PropertyColor, PropertyAsset, PropertyFloat, property_to_json, property_from_json,
    normalize_property_value, evaluate_property_expression)


@dataclass
class DrawEdit:
    changed: bool = False
    immediate: bool = False
    activated: bool = False
    active: bool = False
    deactivated: bool = False
    edited: bool = False
    cancelled: bool = False

    def observe(self, i):
        self.activated |= i.is_item_activated()
        self.active |= i.is_item_active()
        self.deactivated |= i.is_item_deactivated()
        self.edited |= i.is_item_deactivated_after_edit()

    def merge(self, other):
        for name in self.__dataclass_fields__: setattr(self, name, getattr(self, name) or getattr(other, name))


def _array(array, root, state, path, height, edit_custom, browse_asset):
    i, c = ui()
    out = DrawEdit()
    listing = state.lists.setdefault(i.get_id('##array'), {'active_row': -1, 'rendered_rows': 0})
    listing['rendered_rows'] = 0
    action = None
    if i.begin_child('##array', (0, c.dp(max(60, height))), i.ChildFlags_.borders):
        pitch = i.get_frame_height_with_spacing()
        clip = i.ListClipper()
        clip.begin(len(array.values), pitch)
        if 0 <= listing['active_row'] < len(array.values): clip.include_item_by_index(listing['active_row'])
        while clip.step():
            for index in range(clip.display_start, clip.display_end):
                listing['rendered_rows'] += 1
                i.push_id(index)
                y = i.get_cursor_pos_y()
                i.align_text_to_frame_padding(); c.text_dim(str(index)); i.same_line(c.dp(52))
                i.set_next_item_width(-c.dp(30))
                edit, array.values[index] = draw_property_value(array.values[index], root, state, path+(str(index),), height, edit_custom, browse_asset)
                out.merge(edit)
                if edit.active or edit.activated: listing['active_row'] = index
                elif listing['active_row'] == index: listing['active_row'] = -1
                if i.begin_popup_context_item('##elementMenu'):
                    field = replace(root, value=array.values[index])
                    if i.menu_item(c.tr_label('Copy as JSON'), '', False, not root.password)[0]:
                        try: i.set_clipboard_text(property_to_json(field))
                        except ValueError as error: state.clipboard_error = str(error)
                    if i.menu_item(c.tr_label('Paste value'), '', False, not root.read_only)[0]:
                        try:
                            array.values[index] = property_from_json(field, i.get_clipboard_text())[0][1].value
                            out.changed = out.immediate = True; state.clipboard_error = ''
                        except (ValueError, KeyError, TypeError) as error: state.clipboard_error = str(error)
                    for name, enabled in [('Duplicate', True), ('Move up', index > 0), ('Move down', index+1 < len(array.values)), ('Remove', True)]:
                        if i.menu_item(c.tr_label(name), '', False, enabled and not root.read_only)[0]: action = name, index
                    i.end_popup()
                i.same_line()
                if c.ghost_button('-'): action = 'Remove', index
                i.set_cursor_pos_y(y+pitch-i.get_style().item_spacing.y)
                i.dummy((0, 0))
                i.pop_id()
    i.end_child()
    if action:
        name, index = action
        if name == 'Duplicate': array.values.insert(index+1, deepcopy(array.values[index]))
        elif name == 'Remove': array.values.pop(index)
        else:
            target = index-1 if name == 'Move up' else index+1
            array.values[index], array.values[target] = array.values[target], array.values[index]
        state.elements.clear(); listing['active_row'] = -1; i.internal.clear_active_id()
        out.changed = out.immediate = True
    if c.ghost_button(c.tr_label('Add element')):
        source = array.element_template if array.element_template is not None else (array.values[-1] if array.values else '')
        array.values.append(deepcopy(source)); out.changed = out.immediate = True
    i.same_line(); c.text_dim(str(len(array.values)))
    return out


def draw_property_value(value, root, state, path=(), height=180, edit_custom=None, browse_asset=None, full_array=False):
    i, c = ui()
    out = DrawEdit()
    i.begin_group()
    if type(value) is bool:
        out.changed, value = i.checkbox('##value', value)
        out.immediate = True; out.observe(i)
    elif type(value) in (int, float) or isinstance(value, PropertyFloat):
        editor = state.elements.setdefault(i.get_id('##numeric'), {})
        if not editor.get('active'):
            editor.update(text=str(value.value if isinstance(value, PropertyFloat) else value), before=deepcopy(value), error='')
        accepted, editor['text'] = i.input_text('##value', editor['text'], i.InputTextFlags_.enter_returns_true)
        out.observe(i); editor['active'] = i.is_item_active()
        if accepted:
            try:
                value = evaluate_property_expression(replace(root, value=editor['before']), editor['text'])
                out.changed = out.immediate = True; editor['error'] = ''; editor['active'] = False
            except (ValueError, OverflowError) as error: editor['error'] = str(error)
        if editor['error'] and i.is_item_hovered(): i.set_tooltip(editor['error'])
    elif type(value) is str:
        if root.choices:
            selected = root.choices.index(value) if value in root.choices else -1
            out.changed, selected = i.combo('##value', selected, [c.tr(v) for v in root.choices])
            if out.changed: value = root.choices[selected]
            out.immediate = True
        else: out.changed, value = i.input_text('##value', value, i.InputTextFlags_.password if root.password else 0)
        out.observe(i)
    elif isinstance(value, PropertyVector):
        width = (i.calc_item_width()-i.get_style().item_inner_spacing.x*(len(value.values)-1))/max(1, len(value.values))
        for component, number in enumerate(value.values):
            i.push_id(component)
            if component: i.same_line(0, i.get_style().item_inner_spacing.x)
            i.set_next_item_width(width)
            edit, value.values[component] = draw_property_value(float(number), root, state, path+(str(component),), height, edit_custom, browse_asset)
            out.merge(edit); i.pop_id()
    elif isinstance(value, PropertyColor):
        out.changed, rgba = i.color_edit4('##value', value.rgba)
        value.rgba = tuple(rgba); out.observe(i)
    elif isinstance(value, PropertyAsset):
        width = i.calc_item_width(); i.set_next_item_width(max(c.dp(30), width-c.dp(34)))
        out.changed, value.path = i.input_text('##value', value.path); out.observe(i); i.same_line()
        if c.button('...') and browse_asset: browse_asset(root.id, path)
    elif isinstance(value, PropertyCustom):
        if edit_custom:
            edit = edit_custom(root.id, path, deepcopy(value))
            if edit and edit.phase in ('update', 'commit'): value = edit.value; out.changed = True
            if edit:
                out.immediate = edit.committed; out.activated = edit.phase == 'begin'; out.active = edit.phase in ('begin', 'update')
                out.cancelled = edit.phase == 'cancel'
            out.observe(i)
        else:
            editor = state.elements.setdefault(i.get_id('##jsonState'), {})
            if c.button(c.tr_label('Edit JSON...')):
                editor.update(text=value.json, error='', open=True); i.open_popup('##json')
            if i.begin_popup('##json'):
                if not editor.get('open') or root.read_only: i.close_current_popup()
                _, editor['text'] = i.input_text_multiline('##jsonText', editor.get('text', value.json), (c.dp(420), c.dp(140)))
                if c.button(c.tr_label('Apply')) and editor.get('open') and not root.read_only:
                    try:
                        value = normalize_property_value(replace(root, value=value), PropertyCustom(value.type, editor['text']))
                        out.changed = out.immediate = True; editor['open'] = False; i.close_current_popup()
                    except ValueError as error: editor['error'] = str(error)
                if editor.get('error'): c.text_coloured(c.palette().error, editor['error'])
                if i.is_key_pressed(i.Key.escape): i.close_current_popup()
                out.active |= not out.immediate; i.end_popup()
    elif isinstance(value, PropertyArray) and full_array:
        out.merge(_array(value, root, state, path, height, edit_custom, browse_asset))
    elif isinstance(value, (PropertyArray, PropertyObject)):
        label = c.tr('Array')+' ('+str(len(value.values))+')' if isinstance(value, PropertyArray) else value.type
        if c.button(label+'###details'): i.open_popup('##details')
        i.set_next_window_size((c.dp(520), 0), i.Cond_.appearing)
        if i.begin_popup('##details'):
            if isinstance(value, PropertyArray): out.merge(_array(value, root, state, path, height, edit_custom, browse_asset))
            else:
                for key in value.fields:
                    i.push_id(key); c.text_dim(c.tr(key)); i.set_next_item_width(-1)
                    edit, value.fields[key] = draw_property_value(value.fields[key], root, state, path+(key,), height, edit_custom, browse_asset)
                    out.merge(edit); i.pop_id()
            if i.is_key_pressed(i.Key.escape): i.close_current_popup()
            out.active |= not out.immediate; i.end_popup()
    else: raise ValueError('Unsupported property type')
    i.end_group()
    return out, value
