"""Portable graph fragments with separate source ID namespaces and fresh clone IDs."""
from dataclasses import dataclass, field
from copy import deepcopy
from math import isfinite
import json


@dataclass
class NodeGroup:
    id: int
    position: tuple
    size: tuple = (400, 240)
    label: str = ''
    color_index: int = 0


@dataclass
class NodeReroute:
    id: int
    link: int
    position: tuple


@dataclass
class NodeFragment:
    nodes: list = field(default_factory=list)
    links: list = field(default_factory=list)
    groups: list = field(default_factory=list)
    reroutes: list = field(default_factory=list)


@dataclass
class NodeSelection:
    nodes: list = field(default_factory=list)
    groups: list = field(default_factory=list)
    links: list = field(default_factory=list)
    reroutes: list = field(default_factory=list)


def copy_node_selection(selection, nodes, links, groups=(), reroutes=()):
    return copy_graph_selection(NodeSelection(list(selection), list(selection)), nodes, links, groups, reroutes)


def copy_graph_selection(selection, nodes, links, groups=(), reroutes=()):
    selected, group_ids = set(selection.nodes), set(selection.groups)
    index = {g.id: g for g in groups}; pending = list(group_ids)
    for key in pending:
        group = index.get(key)
        if group is None: continue
        selected.update(n.id for n in nodes if all(group.position[k] <= n.position[k] <= group.position[k]+group.size[k] for k in (0, 1)))
        for child in groups:
            if child.id not in group_ids and all(group.position[k] <= child.position[k] and child.position[k]+child.size[k] <= group.position[k]+group.size[k] for k in (0, 1)):
                group_ids.add(child.id); pending.append(child.id)
    picked_groups = [g for g in groups if g.id in group_ids]
    link_ids = set(selection.links) | {r.link for r in reroutes if r.id in selection.reroutes}
    owners = {p.id: n.id for n in nodes for p in n.inputs+n.outputs}
    selected.update(owners[pin] for link in links if link.id in link_ids for pin in (link.source, link.target) if pin in owners)
    picked_nodes = [n for n in nodes if n.id in selected]; pins = {p.id for n in picked_nodes for p in n.inputs+n.outputs}
    picked_links = [link for link in links if link.source in pins and link.target in pins]; link_ids = {link.id for link in picked_links}
    return deepcopy(NodeFragment(picked_nodes, picked_links, picked_groups, [r for r in reroutes if r.link in link_ids]))


def nodes_to_json(fragment):
    pin = lambda p: dict(id=p.id, label=p.label, color=p.color_index)
    value = dict(format='cyui.nodes', version=1,
        nodes=[dict(id=n.id, position=n.position, label=n.label, color=n.color_index, inputs=[pin(p) for p in n.inputs], outputs=[pin(p) for p in n.outputs]) for n in fragment.nodes],
        links=[dict(id=l.id, **{'from': l.source, 'to': l.target}, color=l.color_index) for l in fragment.links],
        groups=[dict(id=g.id, position=g.position, size=g.size, label=g.label, color=g.color_index) for g in fragment.groups],
        reroutes=[dict(id=r.id, link=r.link, position=r.position) for r in fragment.reroutes])
    text = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(',', ':'))
    nodes_from_json(text)
    return text


def nodes_from_json(text):
    from .nodes import Node, Pin, Link
    if len(text.encode('utf8')) > 1024*1024: raise ValueError('Clipboard exceeds 1 MiB')
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result: raise ValueError('Duplicate JSON key')
            result[key] = value
        return result
    def invalid_constant(_): raise ValueError('Non-finite number')
    try: value = json.loads(text, object_pairs_hook=pairs, parse_constant=invalid_constant)
    except RecursionError as error: raise ValueError('JSON nesting limit exceeded') from error
    pending = [(value, 0)]; count = 0
    while pending:
        part, depth = pending.pop(); count += 1
        if depth > 32 or count > 65536: raise ValueError('JSON nesting or node limit exceeded')
        pending.extend((v, depth+1) for v in (part.values() if isinstance(part, dict) else part if isinstance(part, list) else ()))
    if not isinstance(value, dict) or value.get('format') != 'cyui.nodes' or type(value.get('version')) is not int or value['version'] != 1: raise ValueError('Unsupported node format')
    def integer(value, low, high):
        if type(value) is not int or not low <= value <= high: raise ValueError('Invalid integer')
        return value
    def label(value):
        if type(value) is not str: raise ValueError('Expected text')
        return value
    def color(value): return integer(value, -(1 << 31), (1 << 31)-1)
    def identifier(key, seen):
        if type(key) is not int or not 0 < key < (1 << 64) or key in seen: raise ValueError('Invalid or duplicate ID')
        seen.add(key); return key
    def point(p):
        if not isinstance(p, list) or len(p) != 2 or any(type(n) not in (int, float) or not isfinite(n) for n in p): raise ValueError('Invalid point')
        return tuple(p)
    node_ids, pin_ids, link_ids, group_ids, route_ids, inputs, outputs = (set() for _ in range(7))
    fragment = NodeFragment()
    for n in value['nodes']:
        def pins(name, target):
            result = []
            for p in n[name]:
                key = identifier(p['id'], pin_ids); target.add(key); result.append(Pin(key, label(p['label']), color(p['color'])))
            return result
        fragment.nodes.append(Node(identifier(n['id'], node_ids), point(n['position']), label(n['label']), pins('inputs', inputs), pins('outputs', outputs), color(n['color'])))
    for link in value['links']:
        source, target = (integer(link[key], 1, (1 << 64)-1) for key in ('from', 'to'))
        if source not in outputs or target not in inputs: raise ValueError('Dangling or reversed link')
        fragment.links.append(Link(identifier(link['id'], link_ids), source, target, color(link['color'])))
    for group in value['groups']:
        size = point(group['size'])
        if min(size) <= 0: raise ValueError('Invalid group size')
        fragment.groups.append(NodeGroup(identifier(group['id'], group_ids), point(group['position']), size, label(group['label']), color(group['color'])))
    for route in value['reroutes']:
        link = integer(route['link'], 1, (1 << 64)-1)
        if link not in link_ids: raise ValueError('Dangling reroute')
        fragment.reroutes.append(NodeReroute(identifier(route['id'], route_ids), link, point(route['position'])))
    return fragment


def clone_nodes(fragment, allocate_id, offset=(30, 30)):
    result = nodes_from_json(nodes_to_json(fragment)); used, pins, links = set(), {}, {}
    if not allocate_id or any(not isfinite(v) for v in offset): raise ValueError('Missing allocator or invalid offset')
    def new():
        key = allocate_id()
        if type(key) is not int or not 0 < key < (1 << 64) or key in used: raise ValueError('Allocator returned duplicate or zero ID')
        used.add(key); return key
    def move(point):
        value = tuple(point[k]+offset[k] for k in (0, 1))
        if any(not isfinite(v) for v in value): raise ValueError('Position overflow')
        return value
    for node in result.nodes:
        node.id = new(); node.position = move(node.position)
        for pin in node.inputs+node.outputs:
            old = pin.id; pin.id = new(); pins[old] = pin.id
    for link in result.links:
        old = link.id; link.id = new(); links[old] = link.id; link.source, link.target = pins[link.source], pins[link.target]
    for group in result.groups: group.id = new(); group.position = move(group.position)
    for route in result.reroutes: route.id = new(); route.link = links[route.link]; route.position = move(route.position)
    return result
