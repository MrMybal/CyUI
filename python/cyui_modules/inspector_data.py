"""Typed JSON clipboard shared with the native Inspector; imports without a renderer."""
from dataclasses import dataclass, replace
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


@dataclass(eq=True)
class PropertyCustom:
    type: str
    json: str = 'null'


def make_property_array(element_template, count=0):
    if not 0 <= count <= 2147483647: raise ValueError('Invalid collection size')
    return PropertyArray([deepcopy(element_template) for _ in range(count)], deepcopy(element_template))


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
    raise ValueError('Unsupported property type')


def _encode(value, depth=0):
    if depth > 12: raise ValueError('Property nesting exceeds 12 levels')
    kind = property_type(value)
    if isinstance(value, PropertyVector): payload = value.values
    elif isinstance(value, PropertyColor): payload = value.rgba
    elif isinstance(value, PropertyFloat): payload = value.value
    elif isinstance(value, PropertyAsset): return {'type': 'asset', 'assetType': value.type, 'value': value.path}
    elif isinstance(value, PropertyArray):
        result = {'type': 'array', 'value': [_encode(v, depth+1) for v in value.values]}
        if value.element_template is not None: result['element'] = _encode(value.element_template, depth+1)
        return result
    elif isinstance(value, PropertyObject): return {'type': 'object', 'objectType': value.type, 'value': {k: _encode(v, depth+1) for k, v in value.fields.items()}}
    elif isinstance(value, PropertyCustom): return {'type': 'custom', 'customType': value.type, 'value': _json(value.json)}
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
    if kind == 'array' and type(value) is list:
        return PropertyArray([_decode(v, depth+1) for v in value], _decode(item['element'], depth+1) if 'element' in item else None)
    if kind == 'object' and type(value) is dict and type(item.get('objectType')) is str:
        return PropertyObject(item['objectType'], {k: _decode(v, depth+1) for k, v in value.items()})
    if kind == 'custom' and type(item.get('customType')) is str:
        return PropertyCustom(item['customType'], json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(',', ':')))
    raise ValueError('Invalid typed property')


def normalize_property_value(prop, value):
    return _normalize(prop, deepcopy(value))


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
    elif isinstance(value, PropertyArray):
        if len(value.values) > 2147483647: raise ValueError('Too many collection rows')
        prototype = prop.value.element_template
        if prototype is None and prop.value.values: prototype = prop.value.values[0]
        if prototype is None: prototype = value.element_template
        if prototype is None and value.values: prototype = value.values[0]
        if value.element_template is not None: value.element_template = _normalize(replace(prop, value=prototype), value.element_template, depth+1)
        value.values = [_normalize(replace(prop, value=prototype), atom, depth+1) for atom in value.values]
        if prop.value.element_template is not None: value.element_template = deepcopy(prop.value.element_template)
    elif isinstance(value, PropertyObject):
        if not value.type or set(value.fields) != set(prop.value.fields): raise ValueError('Object schema mismatch')
        value.fields = {k: _normalize(replace(prop, value=prop.value.fields[k]), v, depth+1) for k, v in value.fields.items()}
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
    if isinstance(a, PropertyArray):
        left = a.element_template if a.element_template is not None else (a.values[0] if a.values else None)
        right = b.element_template if b.element_template is not None else (b.values[0] if b.values else None)
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
