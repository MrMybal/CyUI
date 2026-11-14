"""Typed JSON clipboard shared with the native Inspector; imports without a renderer."""
from dataclasses import dataclass, replace, field
from copy import deepcopy
import json
import math
import struct
from .numeric import evaluate_numeric_expression
from .common import Edit


@dataclass(eq=True)
class PropertyVector:
    values: list


@dataclass(eq=True)
class PropertyColor:
    rgba: tuple = (0., 0., 0., 1.)


@dataclass(eq=True)
class PropertyAsset:
    type: str
    path: str = ''


@dataclass(eq=True)
class PropertyFloat:
    value: float = 0.


@dataclass(eq=True)
class PropertyArray:
    values: list
    element_template: object = None


@dataclass(eq=True)
class PropertyObject:
    type: str
    fields: dict
    metadata: dict = field(default_factory=dict, compare=False)


@dataclass
class PropertyMetadata:
    label: str = ''
    help: str = ''
    read_only: bool = False
    password: bool = False
    multiline: bool = False
    minimum: object = None
    maximum: object = None
    choices: tuple = ()


@dataclass(eq=False)
class PropertySet:
    values: list = field(default_factory=list)
    element_template: object = None
    def __eq__(self, other): return isinstance(other, PropertySet) and property_value_key(self) == property_value_key(other)


@dataclass(eq=False)
class PropertyMap:
    entries: list = field(default_factory=list)  # (typed key, typed value); keys need not be strings/hashable Python objects
    key_template: object = None
    value_template: object = None
    def __eq__(self, other): return isinstance(other, PropertyMap) and property_value_key(self) == property_value_key(other)


@dataclass(eq=True)
class PropertyOptional:
    value: object = None
    element_template: object = field(default=None, compare=False)


@dataclass(eq=True)
class PropertyEnum:
    type: str
    value: int = 0
    items: list = field(default_factory=list, compare=False)
    flags: bool = False


@dataclass(eq=True)
class PropertyCustom:
    type: str
    json: str = 'null'


def make_property_array(element_template, count=0):
    if not 0 <= count <= 2147483647: raise ValueError('Invalid collection size')
    return PropertyArray([deepcopy(element_template) for _ in range(count)], deepcopy(element_template))


def make_property_set(element_template): return PropertySet([], deepcopy(element_template))
def make_property_map(key_template, value_template): return PropertyMap([], deepcopy(key_template), deepcopy(value_template))
def make_property_optional(element_template, present=False): return PropertyOptional(deepcopy(element_template) if present else None, deepcopy(element_template))


def property_type(value):
    if type(value) is bool: return 'bool'
    if type(value) is int: return 'int64'
    if type(value) is float: return 'double'
    if type(value) is str: return 'string'
    if isinstance(value, PropertyFloat): return 'float'
    if isinstance(value, PropertyVector): return f'vector{len(value.values)}'
    if isinstance(value, PropertyColor): return 'color'
    if isinstance(value, PropertyAsset): return 'asset:'+value.type
    if isinstance(value, PropertyArray): return 'array'
    if isinstance(value, PropertyObject): return 'object:'+value.type
    if isinstance(value, PropertyCustom): return 'custom:'+value.type
    if isinstance(value, PropertySet): return 'set'
    if isinstance(value, PropertyMap): return 'map'
    if isinstance(value, PropertyOptional): return 'optional'
    if isinstance(value, PropertyEnum): return ('flags:' if value.flags else 'enum:')+value.type
    raise ValueError('Unsupported property type')


def _encode(value, depth=0):
    if depth > 12: raise ValueError('Property nesting exceeds 12 levels')
    kind = property_type(value)
    if isinstance(value, PropertyVector): payload = value.values
    elif isinstance(value, PropertyColor): payload = value.rgba
    elif isinstance(value, PropertyFloat): payload = value.value
    elif isinstance(value, PropertyAsset): return {'type': 'asset', 'assetType': value.type, 'value': value.path}
    elif isinstance(value, (PropertyArray, PropertySet)):
        result = {'type': 'set' if isinstance(value, PropertySet) else 'array', 'value': [_encode(v, depth+1) for v in value.values]}
        if value.element_template is not None: result['element'] = _encode(value.element_template, depth+1)
        return result
    elif isinstance(value, PropertyObject): return {'type': 'object', 'objectType': value.type, 'value': {k: _encode(v, depth+1) for k, v in value.fields.items()}}
    elif isinstance(value, PropertyCustom): return {'type': 'custom', 'customType': value.type, 'value': _json(value.json)}
    elif isinstance(value, PropertyMap):
        result = {'type': 'map', 'value': [{'key': _encode(k, depth+1), 'value': _encode(v, depth+1)} for k, v in value.entries]}
        if value.key_template is not None: result['keyTemplate'] = _encode(value.key_template, depth+1)
        if value.value_template is not None: result['valueTemplate'] = _encode(value.value_template, depth+1)
        return result
    elif isinstance(value, PropertyOptional):
        result = {'type': 'optional', 'value': _encode(value.value, depth+1) if value.value is not None else None}
        if value.element_template is not None: result['element'] = _encode(value.element_template, depth+1)
        return result
    elif isinstance(value, PropertyEnum): return {'type': 'flags' if value.flags else 'enum', 'enumType': value.type, 'value': value.value, 'items': [{'value': v, 'label': label} for v, label in value.items]}
    else: payload = value
    return {'type': kind, 'value': payload}


def _decode(item, depth=0):
    if depth > 12: raise ValueError('Property nesting exceeds 12 levels')
    kind, value = item['type'], item['value']
    if kind == 'bool' and type(value) is bool: return value
    if kind == 'int64' and type(value) is int and -(1 << 63) <= value < (1 << 63): return value
    if kind == 'double' and type(value) in (int, float): return float(value)
    if kind == 'float' and type(value) in (int, float): return PropertyFloat(float(value))
    if kind == 'string' and type(value) is str: return value
    if kind in ('vector2', 'vector3', 'vector4') and type(value) is list and len(value) == int(kind[-1]): return PropertyVector(value)
    if kind == 'color' and type(value) is list and len(value) == 4: return PropertyColor(tuple(value))
    if kind == 'asset' and type(value) is str and type(item.get('assetType')) is str: return PropertyAsset(item['assetType'], value)
    if kind in ('array', 'set') and type(value) is list:
        return (PropertySet if kind == 'set' else PropertyArray)([_decode(v, depth+1) for v in value], _decode(item['element'], depth+1) if 'element' in item else None)
    if kind == 'map' and type(value) is list:
        return PropertyMap([(_decode(v['key'], depth+1), _decode(v['value'], depth+1)) for v in value],
                           _decode(item['keyTemplate'], depth+1) if 'keyTemplate' in item else None,
                           _decode(item['valueTemplate'], depth+1) if 'valueTemplate' in item else None)
    if kind == 'optional': return PropertyOptional(_decode(value, depth+1) if value is not None else None, _decode(item['element'], depth+1) if 'element' in item else None)
    if kind in ('enum', 'flags') and type(item.get('enumType')) is str and type(value) is int:
        return PropertyEnum(item['enumType'], value, [(e['value'], e['label']) for e in item.get('items', [])], kind == 'flags')
    if kind == 'object' and type(value) is dict and type(item.get('objectType')) is str:
        return PropertyObject(item['objectType'], {k: _decode(v, depth+1) for k, v in value.items()})
    if kind == 'custom' and type(item.get('customType')) is str:
        return PropertyCustom(item['customType'], json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(',', ':')))
    raise ValueError('Invalid typed property')


def normalize_property_value(prop, value):
    return _normalize(prop, deepcopy(value))


def _readonly_signature(schema, value, depth=0):
    if depth > 12 or property_type(schema) != property_type(value): return ''
    parts = []
    if isinstance(schema, PropertyObject):
        for key, prototype in schema.fields.items():
            if key not in value.fields: continue
            rules = schema.metadata.get(key)
            part = property_value_key(value.fields[key]) if rules and rules.read_only else _readonly_signature(prototype, value.fields[key], depth+1)
            if part: parts.append((key, part))
    elif isinstance(schema, PropertyArray):
        prototype = schema.element_template if schema.element_template is not None else (schema.values[0] if schema.values else None)
        if prototype is not None:
            for index, item in enumerate(value.values):
                part = _readonly_signature(prototype, item, depth+1)
                if part: parts.append((str(index), part))
    elif isinstance(schema, PropertyOptional):
        prototype = schema.element_template if schema.element_template is not None else schema.value
        if prototype is not None and value.value is not None: return _readonly_signature(prototype, value.value, depth+1)
    return json.dumps(parts, ensure_ascii=False, separators=(',', ':')) if parts else ''


def _normalize(prop, value, depth=0):
    if depth > 12: raise ValueError('Property nesting exceeds 12 levels')
    if property_type(prop.value) != property_type(value): raise ValueError('Incompatible property type')
    def number(n):
        if type(n) not in (int, float) or not math.isfinite(n): raise ValueError('Non-finite value')
        if prop.minimum is not None and prop.maximum is not None and prop.minimum > prop.maximum: raise ValueError('Invalid numeric bounds')
        if prop.minimum is not None: n = max(prop.minimum, n)
        if prop.maximum is not None: n = min(prop.maximum, n)
        return n
    if type(value) is int:
        value = int(number(value))
        if not -(1 << 63) <= value < (1 << 63): raise ValueError('Integer overflow')
    elif type(value) is float: value = float(number(value))
    elif isinstance(value, PropertyFloat): value.value = struct.unpack('f', struct.pack('f', number(value.value)))[0]
    elif isinstance(value, PropertyVector):
        if not 2 <= len(value.values) <= 4: raise ValueError('Invalid vector shape')
        value.values = [float(number(n)) for n in value.values]
    elif isinstance(value, PropertyColor):
        if len(value.rgba) != 4 or any(not math.isfinite(n) for n in value.rgba): raise ValueError('Invalid color')
        value.rgba = tuple(max(0., min(1., n)) for n in value.rgba)
    elif isinstance(value, (PropertyArray, PropertySet)):
        if len(value.values) > 2147483647: raise ValueError('Too many collection rows')
        prototype = prop.value.element_template
        if prototype is None and prop.value.values: prototype = prop.value.values[0]
        if prototype is None: prototype = value.element_template
        if prototype is None and value.values: prototype = value.values[0]
        if value.element_template is not None: value.element_template = _normalize(replace(prop, value=prototype), value.element_template, depth+1)
        originals = {_readonly_signature(prototype, item): item for item in prop.value.values} if prototype is not None and _readonly_signature(prototype, prototype) else {}
        for index, atom in enumerate(value.values):
            current = prop.value.values[index] if index < len(prop.value.values) else prototype
            if originals: current = originals.get(_readonly_signature(prototype, atom), current)
            if current is None or property_type(current) != property_type(prototype): current = prototype
            value.values[index] = _normalize(replace(prop, value=current), atom, depth+1)
        if prop.value.element_template is not None: value.element_template = deepcopy(prop.value.element_template)
        if isinstance(value, PropertySet) and len({property_value_key(v) for v in value.values}) != len(value.values): raise ValueError('Duplicate set element')
    elif isinstance(value, PropertyMap):
        key = prop.value.key_template if prop.value.key_template is not None else (prop.value.entries[0][0] if prop.value.entries else value.key_template if value.key_template is not None else (value.entries[0][0] if value.entries else None))
        prototype = prop.value.value_template if prop.value.value_template is not None else (prop.value.entries[0][1] if prop.value.entries else value.value_template if value.value_template is not None else (value.entries[0][1] if value.entries else None))
        if value.key_template is not None: _normalize(replace(prop, value=key), value.key_template, depth+1)
        if value.value_template is not None: _normalize(replace(prop, value=prototype), value.value_template, depth+1)
        originals = {property_value_key(k): (k, v) for k, v in prop.value.entries}
        for index, (k, v) in enumerate(value.entries):
            current = originals.get(property_value_key(k), prop.value.entries[index] if index < len(prop.value.entries) else (key, prototype))
            key_schema = current[0] if current[0] is not None and property_type(current[0]) == property_type(key) else key
            value_schema = current[1] if current[1] is not None and property_type(current[1]) == property_type(prototype) else prototype
            value.entries[index] = (_normalize(replace(prop, value=key_schema), k, depth+1), _normalize(replace(prop, value=value_schema), v, depth+1))
        if len({property_value_key(k) for k, _ in value.entries}) != len(value.entries): raise ValueError('Duplicate map key')
        if prop.value.key_template is not None: value.key_template = deepcopy(prop.value.key_template)
        if prop.value.value_template is not None: value.value_template = deepcopy(prop.value.value_template)
    elif isinstance(value, PropertyOptional):
        prototype = prop.value.element_template if prop.value.element_template is not None else prop.value.value
        if prototype is None: raise ValueError('Optional template is required')
        if value.element_template is not None: _normalize(replace(prop, value=prototype), value.element_template, depth+1)
        if value.value is not None:
            current = prop.value.value if prop.value.value is not None and property_type(prop.value.value) == property_type(prototype) else prototype
            value.value = _normalize(replace(prop, value=current), value.value, depth+1)
        value.element_template = deepcopy(prototype)
    elif isinstance(value, PropertyEnum):
        if prop.value.items: value.items = deepcopy(prop.value.items)
        if not value.type or not value.items: raise ValueError('Enum items and type are required')
        keys = [v for v, label in value.items]
        if any(type(v) is not int or not -(1 << 63) <= v < (1 << 63) or not isinstance(label, str) for v, label in value.items) or len(set(keys)) != len(keys): raise ValueError('Invalid enum items')
        if type(value.value) is not int: raise ValueError('Invalid enum value')
        mask = 0
        for key in keys: mask |= key
        if value.flags:
            if any(key < 0 for key in keys) or value.value < 0 or value.value & ~mask: raise ValueError('Invalid enum value')
        elif value.value not in keys: raise ValueError('Invalid enum value')
    elif isinstance(value, PropertyObject):
        if not value.type or set(value.fields) != set(prop.value.fields): raise ValueError('Object schema mismatch')
        for key, v in value.fields.items():
            child = property_object_field(prop, prop.value, key)
            if child.read_only:
                if v != child.value: raise ValueError('Property is read-only')
            else: value.fields[key] = _normalize(child, v, depth+1)
        value.metadata = deepcopy(prop.value.metadata)
    elif isinstance(value, PropertyCustom):
        if not value.type: raise ValueError('Custom type identifier is required')
        value.json = json.dumps(_json(value.json), ensure_ascii=False, allow_nan=False, separators=(',', ':'))
    elif type(value) is str and prop.choices and value not in prop.choices: raise ValueError('Unavailable choice')
    return value


def evaluate_property_expression(prop, expression):
    original = prop.value.value if isinstance(prop.value, PropertyFloat) else prop.value
    if type(original) not in (int, float): raise ValueError('Expected numeric property')
    value = evaluate_numeric_expression(expression, original)
    if prop.minimum is not None: value = max(prop.minimum, value)
    if prop.maximum is not None: value = min(prop.maximum, value)
    if type(original) is int:
        if type(value) is not int: value = math.floor(value+.5) if value >= 0 else math.ceil(value-.5)
    else: value = PropertyFloat(float(value)) if isinstance(prop.value, PropertyFloat) else float(value)
    return normalize_property_value(prop, value)


def _json(text):
    if len(text.encode('utf8')) > 1024*1024: raise ValueError('Clipboard exceeds 1 MiB')
    def unique(pairs):
        result = {}
        for k, v in pairs:
            if k in result: raise ValueError('Duplicate JSON key')
            result[k] = v
        return result
    def invalid(value): raise ValueError('Non-finite JSON number')
    try:
        value = json.loads(text, object_pairs_hook=unique, parse_constant=invalid)
        def bounded(node, depth=0):
            nonlocal nodes
            nodes += 1
            if nodes > 65536: raise ValueError('JSON node budget exceeded')
            if depth > 32: raise ValueError('JSON is too deeply nested')
            if isinstance(node, dict):
                for v in node.values(): bounded(v, depth+1)
            elif isinstance(node, list):
                for v in node: bounded(v, depth+1)
        nodes = 0
        bounded(value)
        return value
    except (KeyError, TypeError, RecursionError) as error:
        raise ValueError('Invalid clipboard JSON') from error


def _load(text, kind):
    value = _json(text)
    try:
        if value['format'] != 'cyui.inspector' or value['version'] != 1 or value['kind'] != kind: raise ValueError('Unsupported clipboard format')
    except (KeyError, TypeError) as error: raise ValueError('Invalid clipboard envelope') from error
    return value


def _dump(kind, **values):
    text = json.dumps(dict(format='cyui.inspector', version=1, kind=kind, **values), ensure_ascii=False, allow_nan=False, separators=(',', ':'))
    _json(text)
    return text


def property_object_field(root, obj, key):
    child = replace(root, value=obj.fields[key], key=key, label=key)
    if key in obj.metadata:
        rules = obj.metadata[key]
        child = replace(child, label=rules.label or key, help=rules.help, read_only=root.read_only or rules.read_only,
                        password=rules.password, multiline=rules.multiline, minimum=rules.minimum, maximum=rules.maximum, choices=rules.choices)
    return child


def property_value_key(value):
    def identity(item):
        kind = item['type']
        result = {k: v for k, v in item.items() if k not in ('element', 'keyTemplate', 'valueTemplate', 'items')}
        if kind in ('array', 'set'):
            result['value'] = [identity(v) for v in item['value']]
            if kind == 'set': result['value'].sort(key=lambda v: json.dumps(v, ensure_ascii=False, sort_keys=True, separators=(',', ':')))
        elif kind == 'map':
            result['value'] = [{'key': identity(v['key']), 'value': identity(v['value'])} for v in item['value']]
            result['value'].sort(key=lambda v: json.dumps(v['key'], ensure_ascii=False, sort_keys=True, separators=(',', ':')))
        elif kind == 'object': result['value'] = {k: identity(v) for k, v in item['value'].items()}
        elif kind == 'optional' and item['value'] is not None: result['value'] = identity(item['value'])
        elif kind in ('float', 'double') and not item['value']: result['value'] = 0
        elif kind in ('color', 'vector2', 'vector3', 'vector4'): result['value'] = [v if v else 0 for v in item['value']]
        return result
    return json.dumps(identity(_encode(value)), ensure_ascii=False, allow_nan=False, sort_keys=True, separators=(',', ':'))


def property_value_text(value):
    if type(value) is str: return value
    if type(value) is bool: return 'true' if value else 'false'
    if type(value) in (int, float): return str(value)
    if isinstance(value, PropertyFloat): return str(value.value)
    if isinstance(value, PropertyAsset): return value.path
    if isinstance(value, PropertyCustom): return value.type+': '+value.json
    if isinstance(value, PropertyObject): return value.type
    if isinstance(value, PropertyEnum):
        labels = [label for key, label in value.items if key and value.value & key == key] if value.flags else [label for key, label in value.items if value.value == key]
        return ' | '.join(labels) or str(value.value)
    if isinstance(value, PropertyOptional): return property_value_text(value.value) if value.value is not None else 'null'
    if isinstance(value, (PropertyArray, PropertySet)): return property_type(value)+' ('+str(len(value.values))+')'
    if isinstance(value, PropertyMap): return 'map ('+str(len(value.entries))+')'
    return json.dumps(_encode(value)['value'], ensure_ascii=False, allow_nan=False, separators=(',', ':'))


def edit_property_collection(prop, selection, action, text='', before=None, validate=None):
    if prop.read_only: raise ValueError('Property is read-only')
    value = deepcopy(prop.value)
    if not isinstance(value, (PropertyArray, PropertySet, PropertyMap)): raise ValueError('Expected a collection')
    values = value.entries if isinstance(value, PropertyMap) else value.values
    selected = sorted(set(selection))
    if not selected or selected[0] < 0 or selected[-1] >= len(values): raise ValueError('Invalid collection selection')
    if action == 'remove':
        for index in reversed(selected): values.pop(index)
    elif action in ('duplicate', 'move'):
        if not isinstance(value, PropertyArray): raise ValueError('Only arrays can be duplicated or reordered')
        copied = [deepcopy(values[n]) for n in selected]
        if before is None: before = selected[-1]+1
        if not 0 <= before <= len(values): raise ValueError('Invalid drop position')
        if action == 'move':
            before -= sum(n < before for n in selected)
            for index in reversed(selected): values.pop(index)
        values[before:before] = copied
    elif action in ('assign', 'expression'):
        for index in selected:
            original = values[index][1] if isinstance(value, PropertyMap) else values[index]
            field = replace(prop, value=original)
            changed = evaluate_property_expression(field, text) if action == 'expression' else property_from_json(field, text)[0][1].value
            if isinstance(value, PropertyMap): values[index] = (values[index][0], changed)
            else: values[index] = changed
    else: raise ValueError('Invalid collection action')
    value = normalize_property_value(prop, value)
    error = validate(prop.id, value) if validate else ''
    if error: raise ValueError(error)
    return [(prop.id, Edit('commit', value))]


def property_to_json(prop): return _dump('property', property=_encode(prop.value))


def property_from_json(prop, text, validate=None):
    if prop.read_only: raise ValueError('Property is read-only')
    value = normalize_property_value(prop, _decode(_load(text, 'property')['property']))
    error = validate(prop.id, value) if validate else ''
    if error: raise ValueError(error)
    return [(prop.id, Edit('commit', value))]


def property_category_to_json(properties, category):
    values = {}
    for prop in properties:
        if prop.category == category:
            key = prop.key or prop.label
            if key in values: raise ValueError('Duplicate clipboard key')
            values[key] = _encode(prop.value)
    return _dump('category', properties=values)


def property_category_from_json(properties, category, text, validate=None):
    values = _load(text, 'category')['properties']
    targets = [p for p in properties if p.category == category]
    if not targets or len({p.key or p.label for p in targets}) != len(targets) or set(values) != {p.key or p.label for p in targets}: raise ValueError('Category schema does not match')
    result = []
    for prop in targets:
        result += property_from_json(prop, _dump('property', property=values[prop.key or prop.label]), validate)
    return result


def _same_schema(a, b, depth=0):
    if depth > 12 or property_type(a) != property_type(b): return False
    if isinstance(a, (PropertyArray, PropertySet)):
        left = a.element_template if a.element_template is not None else (a.values[0] if a.values else None)
        right = b.element_template if b.element_template is not None else (b.values[0] if b.values else None)
        return left is None or right is None or _same_schema(left, right, depth+1)
    if isinstance(a, PropertyMap):
        left = [a.key_template, a.value_template]; right = [b.key_template, b.value_template]
        if a.entries: left = [v if v is not None else a.entries[0][n] for n, v in enumerate(left)]
        if b.entries: right = [v if v is not None else b.entries[0][n] for n, v in enumerate(right)]
        return all(x is None or y is None or _same_schema(x, y, depth+1) for x, y in zip(left, right))
    if isinstance(a, PropertyOptional):
        left = a.element_template if a.element_template is not None else a.value
        right = b.element_template if b.element_template is not None else b.value
        return left is None or right is None or _same_schema(left, right, depth+1)
    if isinstance(a, PropertyObject): return set(a.fields) == set(b.fields) and all(_same_schema(v, b.fields[k], depth+1) for k, v in a.fields.items())
    return True


def common_properties(objects):
    if not objects: return []
    result = []
    for prop in deepcopy(objects[0]):
        for obj in objects[1:]:
            other = next((p for p in obj if (p.key or p.label, p.category) == (prop.key or prop.label, prop.category)), None)
            if other is None or not _same_schema(other.value, prop.value): break
            prop.mixed |= other.value != prop.value
            prop.read_only |= other.read_only
            if other.minimum is not None: prop.minimum = max(prop.minimum, other.minimum) if prop.minimum is not None else other.minimum
            if other.maximum is not None: prop.maximum = min(prop.maximum, other.maximum) if prop.maximum is not None else other.maximum
        else:
            if prop.minimum is None or prop.maximum is None or prop.minimum <= prop.maximum: result.append(prop)
    return result
