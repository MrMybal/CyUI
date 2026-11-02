"""Typed property editing. Hosts validate and apply committed proposals."""
from dataclasses import dataclass, field
from copy import deepcopy
from .common import Edit, ui, item_id


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


@dataclass
class InspectorState:
    drafts: dict = field(default_factory=dict)
    active: set = field(default_factory=set)
    before: dict = field(default_factory=dict)
    errors: dict = field(default_factory=dict)
    revision: int = 0


def inspector(identifier, state, properties, revision=0, validate=None):
    i, c = ui()
    events = []
    if state.revision != revision:
        events.extend((key, Edit('cancel')) for key in state.active)
        state.active.clear()
        state.drafts.clear()
        state.errors.clear()
        state.revision = revision
    present = {p.id for p in properties}
    for key in set(state.drafts)-present:
        state.drafts.pop(key, None)
        state.before.pop(key, None)
        state.errors.pop(key, None)
        state.active.discard(key)
    i.push_id(identifier)
    for prop in properties:
        key = prop.id
        if key not in state.active:
            state.drafts[key] = deepcopy(prop.value)
        value = state.drafts[key]
        i.push_id(item_id(key))
        i.text_unformatted(prop.label)
        if prop.help and i.is_item_hovered():
            i.set_tooltip(prop.help)
        i.set_next_item_width(-1)
        i.begin_disabled(prop.read_only)
        if isinstance(value, bool):
            changed, value = i.checkbox('##value', value)
        elif isinstance(value, int):
            # Decimal text preserves all int64 bits on every Python binding.
            changed, text = i.input_text('##value', str(value), i.InputTextFlags_.chars_decimal)
            try:
                value = int(text)
            except ValueError:
                changed = False
        elif isinstance(value, float):
            changed, value = i.input_double('##value', value, 0., 0., '%.9g')
        elif prop.choices:
            index = prop.choices.index(value) if value in prop.choices else -1
            changed, index = i.combo('##value', index, list(prop.choices))
            if changed:
                value = prop.choices[index]
        elif isinstance(value, str):
            flags = i.InputTextFlags_.password if prop.password else 0
            if prop.multiline:
                changed, value = i.input_text_multiline('##value', value, (0, c.dp(80)), flags)
            else:
                changed, value = i.input_text('##value', value, flags)
        else:
            i.text_unformatted(str(value))
            changed = False
        activated, deactivated = i.is_item_activated(), i.is_item_deactivated_after_edit()
        i.end_disabled()
        if activated or (changed and key not in state.active):
            state.active.add(key)
            state.before[key] = deepcopy(prop.value)
            events.append((key, Edit('begin', deepcopy(prop.value))))
        if changed:
            if isinstance(value, (int, float)) and not isinstance(value, bool):
                if prop.minimum is not None:
                    value = max(prop.minimum, value)
                if prop.maximum is not None:
                    value = min(prop.maximum, value)
            state.drafts[key] = value
            state.errors[key] = validate(key, value) if validate else ''
            events.append((key, Edit('update', deepcopy(value))))
        if key in state.active and i.is_key_pressed(i.Key.escape):
            state.drafts[key] = state.before[key]
            state.active.discard(key)
            state.errors.pop(key, None)
            events.append((key, Edit('cancel')))
        elif deactivated and key in state.active and not state.errors.get(key):
            state.active.discard(key)
            events.append((key, Edit('commit', deepcopy(state.drafts[key]))))
        if state.errors.get(key):
            c.text_coloured(c.palette().error, state.errors[key])
        i.pop_id()
    i.pop_id()
    return events
