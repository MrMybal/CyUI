"""Fixed-height typed collection rows; only visible rows and the active row are submitted."""
from copy import deepcopy
from dataclasses import dataclass, replace
from .common import ui
from .inspector_data import (PropertyArray, PropertyObject, PropertyCustom, PropertyVector,
    PropertyColor, PropertyAsset, PropertyFloat, property_to_json, property_from_json,
    normalize_property_value, evaluate_property_expression, PropertySet, PropertyMap, PropertyOptional, PropertyEnum,
    property_value_text, property_object_field)


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


def draw_property_value(value, root, state, path=(), height=180, edit_custom=None, browse_asset=None, full_array=False):
    i, c = ui()
    out = DrawEdit()
    i.begin_group()
    i.begin_disabled(root.read_only and not isinstance(value, (PropertyArray, PropertySet, PropertyMap, PropertyObject, PropertyOptional)))
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
    elif isinstance(value, PropertyEnum):
        if i.begin_combo('##value', property_value_text(value)):
            for key, label in value.items:
                selected = (value.value & key == key if key else value.value == 0) if value.flags else value.value == key
                clicked, _ = i.selectable(c.tr_label(label), selected, i.SelectableFlags_.no_auto_close_popups if value.flags else 0)
                if clicked:
                    value.value = (value.value & ~key if selected else value.value | key) if value.flags and key else key
                    out.changed = out.immediate = True
            i.end_combo()
        out.observe(i)
    elif isinstance(value, PropertyOptional):
        present = value.value is not None; width = i.calc_item_width(); i.begin_disabled(root.read_only)
        changed, present = i.checkbox('##present', present); out.observe(i); i.end_disabled()
        if changed:
            value.value = deepcopy(value.element_template) if present else None
            out.changed = out.immediate = True
        i.same_line()
        if value.value is not None:
            i.set_next_item_width(max(c.dp(30), width-i.get_frame_height_with_spacing())); i.push_id('optional')
            edit, value.value = draw_property_value(value.value, root, state, path+('value',), height, edit_custom, browse_asset)
            out.merge(edit); i.pop_id()
        else: c.text_dim('null')
    elif isinstance(value, (PropertyArray, PropertySet, PropertyMap)) and full_array:
        from .collection_editor import draw_collection
        edit, value = draw_collection(value, root, state, path, height, edit_custom, browse_asset); out.merge(edit)
    elif isinstance(value, (PropertyArray, PropertySet, PropertyMap, PropertyObject)):
        label = property_value_text(value)
        if c.button(label+'###details'): i.open_popup('##details')
        i.set_next_window_size((c.dp(520), 0), i.Cond_.appearing)
        if i.begin_popup('##details'):
            if isinstance(value, (PropertyArray, PropertySet, PropertyMap)):
                from .collection_editor import draw_collection
                edit, value = draw_collection(value, root, state, path, height, edit_custom, browse_asset); out.merge(edit)
            else:
                for key in value.fields:
                    child = property_object_field(root, value, key)
                    i.push_id(key); c.text_dim(c.tr(child.label)); i.set_next_item_width(-1)
                    if child.help and i.is_item_hovered(): i.set_tooltip(c.tr(child.help))
                    edit, value.fields[key] = draw_property_value(value.fields[key], child, state, path+(key,), height, edit_custom, browse_asset)
                    out.merge(edit); i.pop_id()
            if i.is_key_pressed(i.Key.escape): i.close_current_popup()
            out.active |= not out.immediate; i.end_popup()
    else: raise ValueError('Unsupported property type')
    i.end_disabled(); i.end_group()
    return out, value
