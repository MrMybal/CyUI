"""Typed property editing. Hosts validate and apply committed proposals."""
from dataclasses import dataclass, field, replace
from copy import deepcopy
from .common import Edit, ui, item_id
from .locale import tr
from .inspector_data import (PropertyVector, PropertyColor, PropertyAsset, PropertyFloat, PropertyArray,
    PropertyObject, PropertyCustom, make_property_array,
    PropertySet, PropertyMap, PropertyOptional, PropertyEnum, PropertyMetadata,
    make_property_set, make_property_map, make_property_optional, property_value_text, property_value_key,
    property_object_field, edit_property_collection,
    property_type, normalize_property_value, evaluate_property_expression, property_to_json,
    property_from_json, property_category_to_json, property_category_from_json, common_properties)


@dataclass
class Property:
    id: int
    label: str
    value: object
    help: str = ''
    read_only: bool = False
    password: bool = False
    multiline: bool = False
    minimum: object = None
    maximum: object = None
    choices: tuple = ()
    key: str = ''
    category: str = ''
    mixed: bool = False
    default_value: object = None


@dataclass
class InspectorState:
    drafts: dict = field(default_factory=dict)
    active: set = field(default_factory=set)
    before: dict = field(default_factory=dict)
    errors: dict = field(default_factory=dict)
    revision: int = 0
    expressions: dict = field(default_factory=dict)
    expression_before: dict = field(default_factory=dict)
    clipboard_error: str = ''
    collapsed_categories: set = field(default_factory=set)
    lists: dict = field(default_factory=dict)
    elements: dict = field(default_factory=dict)
    cached: set = field(default_factory=set)
    active_ids: dict = field(default_factory=dict)
    pending: dict = field(default_factory=dict)
    drag_collection: int = 0


def property_matches(filter_state, prop, context=''):
    return filter_state.matches(prop.label, context+' '+tr(context)+' '+prop.help+' '+tr(prop.help))


def inspector(identifier, state, properties, revision=0, validate=None, filter_state=None, filter_context='',
              array_height=180, edit_custom=None, browse_asset=None):
    i, c = ui()
    events = []
    for listing in state.lists.values(): listing['rendered_rows'] = 0
    if state.revision != revision:
        if i.internal.get_active_id() in state.active_ids.values(): i.internal.clear_active_id()
        events.extend((key, Edit('cancel')) for key in state.active)
        state.active.clear()
        state.drafts.clear()
        state.errors.clear()
        state.expressions.clear()
        state.expression_before.clear()
        for listing in state.lists.values(): listing.update(active_row=-1, indexed_size=-1, selection=[])
        state.elements.clear(); state.cached.clear()
        state.pending.clear()
        state.active_ids.clear()
        state.revision = revision
    present = {p.id for p in properties}
    for key in set(state.drafts)-present:
        state.drafts.pop(key, None)
        state.before.pop(key, None)
        state.errors.pop(key, None)
        state.active.discard(key)
        state.cached.discard(key)
    i.push_id(identifier)
    visible = 0
    categories = set()
    for prop in properties:
        key = prop.id
        if filter_state is not None and not property_matches(filter_state, prop, filter_context):
            if key in state.active:
                i.push_id(item_id(key))
                if i.internal.get_active_id() in (i.get_id('##value'), state.active_ids.get(key, 0)):
                    i.internal.clear_active_id()
                i.pop_id()
                state.drafts[key] = deepcopy(state.before[key])
                state.active.discard(key)
                state.errors.pop(key, None)
                events.append((key, Edit('cancel', deepcopy(state.before[key]))))
            continue
        visible += 1
        if prop.category and prop.category not in categories:
            categories.add(prop.category)
            clicked, _ = i.selectable(('> ' if prop.category in state.collapsed_categories else 'v ')+c.tr_label(prop.category), False)
            if clicked:
                state.collapsed_categories.remove(prop.category) if prop.category in state.collapsed_categories else state.collapsed_categories.add(prop.category)
            if i.begin_popup_context_item('##category:'+prop.category):
                if i.menu_item(c.tr_label('Copy category as JSON'), '', False)[0]:
                    try: i.set_clipboard_text(property_category_to_json(properties, prop.category))
                    except ValueError as error: state.clipboard_error = str(error)
                if i.menu_item(c.tr_label('Paste category'), '', False)[0]:
                    try:
                        events += property_category_from_json(properties, prop.category, i.get_clipboard_text(), validate)
                        state.clipboard_error = ''
                        state.active.clear(); state.drafts.clear(); state.cached.clear(); state.elements.clear()
                        for listing in state.lists.values(): listing['active_row'] = -1
                    except (ValueError, KeyError, TypeError) as error:
                        state.clipboard_error = str(error)
                i.end_popup()
        if prop.category in state.collapsed_categories and not (filter_state and filter_state.active()):
            if key in state.active:
                if state.active_ids.get(key) == i.internal.get_active_id(): i.internal.clear_active_id()
                state.active.discard(key); state.drafts[key] = deepcopy(state.before[key]); events.append((key, Edit('cancel', deepcopy(state.before[key]))))
            continue
        cached = isinstance(prop.value, (PropertyArray, PropertyObject, PropertySet, PropertyMap, PropertyOptional))
        if key not in state.active and (not cached or key not in state.cached):
            state.drafts[key] = deepcopy(prop.value)
            if cached: state.cached.add(key)
        value = state.drafts[key]
        i.push_id(item_id(key))
        i.text_unformatted(c.tr(prop.label))
        if i.is_item_clicked(1): i.open_popup('##propertyMenu')
        if prop.help and i.is_item_hovered():
            i.set_tooltip(c.tr(prop.help))
        i.set_next_item_width(-1)
        i.begin_disabled(prop.read_only and type(value) in (bool, int, float, str))
        immediate = False
        compound = None
        if isinstance(value, bool):
            changed, value = i.checkbox('##value', value)
            immediate = True
        elif isinstance(value, int):
            # Decimal text preserves all int64 bits on every Python binding.
            changed, text = i.input_text('##value', str(value), i.InputTextFlags_.chars_decimal)
            try:
                value = int(text)
            except ValueError:
                changed = False
        elif isinstance(value, float):
            changed, value = i.input_double('##value', value, 0., 0., '%.9g')
        elif isinstance(value, str) and prop.choices:
            index = prop.choices.index(value) if value in prop.choices else -1
            changed, index = i.combo('##value', index, [c.tr(choice) for choice in prop.choices])
            if changed:
                value = prop.choices[index]
            immediate = True
        elif isinstance(value, str):
            flags = i.InputTextFlags_.password if prop.password else 0
            if prop.multiline:
                changed, value = i.input_text_multiline('##value', value, (0, c.dp(80)), flags)
            else:
                changed, value = i.input_text('##value', value, flags)
        elif isinstance(value, (PropertyVector, PropertyColor, PropertyAsset, PropertyFloat, PropertyArray, PropertyObject, PropertyCustom,
                                PropertySet, PropertyMap, PropertyOptional, PropertyEnum)):
            from .property_editor import draw_property_value
            compound, value = draw_property_value(value, prop, state, height=array_height, edit_custom=edit_custom,
                                                  browse_asset=browse_asset, full_array=True)
            changed, immediate = compound.changed, compound.immediate
        else:
            i.text_unformatted(str(value))
            changed = False
        activated, active = i.is_item_activated(), i.is_item_active()
        deactivated, edited = i.is_item_deactivated(), i.is_item_deactivated_after_edit()
        if compound:
            activated, active, deactivated, edited = compound.activated, compound.active, compound.deactivated, compound.edited
        if active and i.internal.get_active_id(): state.active_ids[key] = i.internal.get_active_id()
        escape = i.is_key_pressed(i.Key.escape, False)
        i.end_disabled()
        if prop.read_only:
            if changed: state.drafts[key] = deepcopy(prop.value)
            changed = False
        if not prop.read_only and (activated or (changed and key not in state.active)):
            state.active.add(key)
            state.before[key] = deepcopy(prop.value)
            events.append((key, Edit('begin', deepcopy(prop.value))))
        if changed:
            try:
                value = normalize_property_value(prop, value)
                state.errors[key] = validate(key, value) if validate else ''
                events.append((key, Edit('update', deepcopy(value))))
            except (ValueError, OverflowError) as error:
                state.errors[key] = str(error)
                value = deepcopy(prop.value)
            state.drafts[key] = value
        # As in C++: Escape cancels only the field being edited. A text field takes Escape
        # itself and deactivates on that frame, so a field that has just let go counts too.
        if key in state.active and ((compound and compound.cancelled) or (escape and (active or deactivated))):
            if active and hasattr(i, 'internal'):
                i.internal.clear_active_id()
            state.drafts[key] = deepcopy(state.before[key])
            state.active.discard(key)
            state.cached.discard(key)
            state.errors.pop(key, None)
            events.append((key, Edit('cancel')))
        elif key in state.active and ((immediate and changed) or edited) and not state.errors.get(key):
            # Switches and choices commit at once; text and numbers when they let go.
            state.active.discard(key)
            state.cached.discard(key)
            events.append((key, Edit('commit', deepcopy(state.drafts[key]))))
        elif key in state.active and deactivated and not active and not edited and not (immediate and changed):
            # Activated and left without an edit: let prop.value refresh the field again.
            state.drafts[key] = deepcopy(state.before[key])
            state.active.discard(key)
            state.errors.pop(key, None)
            events.append((key, Edit('cancel')))
        expression_requested = type(prop.value) in (int, float) or isinstance(prop.value, PropertyFloat)
        open_expression = expression_requested and not prop.read_only and i.is_item_hovered() and i.is_mouse_double_clicked(0)
        if not isinstance(prop.value, (PropertyArray, PropertySet, PropertyMap)) and i.is_item_clicked(1): i.open_popup('##propertyMenu')
        if i.begin_popup('##propertyMenu'):
            if i.menu_item(c.tr_label('Copy as JSON'), '', False, not prop.password and not prop.mixed)[0]:
                try: i.set_clipboard_text(property_to_json(prop))
                except ValueError as error: state.clipboard_error = str(error)
            if i.menu_item(c.tr_label('Paste value'), '', False, not prop.read_only)[0]:
                try:
                    events += property_from_json(prop, i.get_clipboard_text(), validate)
                    state.clipboard_error = ''
                    state.cached.discard(key); state.active.discard(key); state.elements.clear()
                except (ValueError, TypeError, KeyError) as error:
                    state.clipboard_error = str(error)
            if expression_requested and i.menu_item(c.tr_label('Edit expression...'), '', False, not prop.read_only)[0]:
                open_expression = True
            if prop.default_value is not None and i.menu_item(c.tr_label('Reset to default'), '', False, not prop.read_only)[0]:
                try:
                    events += property_from_json(prop, property_to_json(replace(prop, value=prop.default_value)), validate)
                    state.clipboard_error = ''
                    state.cached.discard(key); state.active.discard(key); state.elements.clear()
                except (ValueError, TypeError, KeyError) as error:
                    state.clipboard_error = str(error)
            i.end_popup()
        if open_expression:
            state.expressions[key] = str(prop.value.value if isinstance(prop.value, PropertyFloat) else prop.value)
            state.expression_before[key] = deepcopy(prop)
            i.open_popup('##expression')
        if i.begin_popup('##expression'):
            if key not in state.expression_before: i.close_current_popup()
            if i.is_window_appearing(): i.set_keyboard_focus_here()
            c.text_dim(c.tr('Use Value for the original value'))
            accepted, state.expressions[key] = i.input_text('##expressionText', state.expressions.get(key, ''), i.InputTextFlags_.enter_returns_true | i.InputTextFlags_.auto_select_all)
            if accepted and not prop.read_only and key in state.expression_before:
                try:
                    value = evaluate_property_expression(state.expression_before[key], state.expressions[key])
                    error = validate(key, value) if validate else ''
                    if error: raise ValueError(error)
                    events.append((key, Edit('commit', value)))
                    state.active.discard(key)
                    state.errors.pop(key, None)
                    i.close_current_popup()
                except ValueError as error:
                    state.errors[key] = str(error)
            if i.is_key_pressed(i.Key.escape) or prop.read_only: i.close_current_popup()
            i.end_popup()
        if prop.mixed: c.text_faint(c.tr('Multiple values'))
        if state.errors.get(key):
            c.text_coloured(c.palette().error, state.errors[key])
        i.pop_id()
    i.pop_id()
    if state.clipboard_error: c.text_coloured(c.palette().error, state.clipboard_error)
    if not visible and filter_state is not None and filter_state.active():
        c.text_dim(c.tr('No matching properties'))
    return events


def property_value_editor(identifier, state, prop, revision=0, validate=None, edit_custom=None, browse_asset=None, array_height=180):
    from .property_editor import draw_property_value
    i, c = ui(); i.push_id(identifier); i.push_id(item_id(prop.id)); key = prop.id
    if state.revision != revision:
        state.drafts.clear(); state.active.clear(); state.before.clear(); state.elements.clear(); state.pending.clear(); state.cached.clear(); state.lists.clear(); state.revision = revision
    cached = isinstance(prop.value, (PropertyArray, PropertyObject, PropertySet, PropertyMap, PropertyOptional))
    if key not in state.active and (not cached or key not in state.cached):
        state.drafts[key] = deepcopy(prop.value); state.cached.add(key)
    value = state.drafts[key]
    edit, value = draw_property_value(value, prop, state, height=array_height, edit_custom=edit_custom, browse_asset=browse_asset)
    phase = 'none'
    if not prop.read_only:
        if edit.activated and key not in state.active:
            state.before[key] = deepcopy(prop.value); state.active.add(key); phase = 'begin'
        if edit.changed:
            if key not in state.active: state.before[key] = deepcopy(prop.value)
            state.active.add(key)
            try:
                value = normalize_property_value(prop, value); state.errors[key] = ''; phase = 'update'
            except (ValueError, OverflowError) as error: state.errors[key] = str(error)
            state.drafts[key] = value
        if (edit.immediate and edit.changed) or edit.edited:
            try:
                value = normalize_property_value(prop, value)
                error = validate(key, value) if validate else ''
                if error: raise ValueError(error)
                state.errors[key] = ''; state.drafts[key] = value; phase = 'commit'; state.active.discard(key); state.cached.discard(key)
            except (ValueError, OverflowError) as error: state.errors[key] = str(error); phase = 'none'
        if key in state.active and (edit.cancelled or ((edit.active or edit.deactivated) and i.is_key_pressed(i.Key.escape))):
            state.drafts[key] = deepcopy(state.before[key]); state.active.discard(key); state.cached.discard(key); state.errors.pop(key, None); phase = 'cancel'
        elif key in state.active and edit.deactivated and not edit.active and not edit.edited and not edit.changed:
            state.drafts[key] = deepcopy(state.before[key]); state.active.discard(key); state.cached.discard(key); phase = 'cancel'
    elif edit.changed: state.drafts[key] = deepcopy(prop.value)
    if state.errors.get(key) and i.is_item_hovered(): i.set_tooltip(state.errors[key])
    result = Edit(phase, deepcopy(state.drafts[key])) if phase != 'none' else Edit()
    i.pop_id(); i.pop_id(); return result
