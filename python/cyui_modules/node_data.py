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


def copy_node_selection(selection, nodes, links, groups=(), reroutes=()):
    selected = set(selection); picked_groups = [g for g in groups if g.id in selected]
    for group in picked_groups:
        selected.update(n.id for n in nodes if all(group.position[k] <= n.position[k] <= group.position[k]+group.size[k] for k in (0, 1)))
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
    return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(',', ':'))


def nodes_from_json(text):
    from .nodes import Node, Pin, Link
    if len(text.encode('utf8')) > 1024*1024: raise ValueError('Clipboard exceeds 1 MiB')
    value = json.loads(text)
    if value['format'] != 'cyui.nodes' or value['version'] != 1: raise ValueError('Unsupported node format')
    def identifier(key, seen):
        if type(key) is not int or not 0 <= key < (1 << 64) or key in seen: raise ValueError('Invalid or duplicate ID')
        seen.add(key); return key
    def point(p):
        if len(p) != 2 or any(type(n) not in (int, float) or not isfinite(n) for n in p): raise ValueError('Invalid point')
        return tuple(p)
    node_ids, pin_ids, link_ids, group_ids, route_ids, inputs, outputs = (set() for _ in range(7))
    fragment = NodeFragment()
    for n in value['nodes']:
        def pins(name, target):
            result = []
            for p in n[name]:
                key = identifier(p['id'], pin_ids); target.add(key); result.append(Pin(key, p['label'], p['color']))
            return result
        fragment.nodes.append(Node(identifier(n['id'], node_ids), point(n['position']), n['label'], pins('inputs', inputs), pins('outputs', outputs), n['color']))
    for link in value['links']:
        if link['from'] not in outputs or link['to'] not in inputs: raise ValueError('Dangling or reversed link')
        fragment.links.append(Link(identifier(link['id'], link_ids), link['from'], link['to'], link['color']))
    for group in value['groups']:
        size = point(group['size'])
        if min(size) <= 0: raise ValueError('Invalid group size')
        fragment.groups.append(NodeGroup(identifier(group['id'], group_ids), point(group['position']), size, group['label'], group['color']))
    for route in value['reroutes']:
        if route['link'] not in link_ids: raise ValueError('Dangling reroute')
        fragment.reroutes.append(NodeReroute(identifier(route['id'], route_ids), route['link'], point(route['position'])))
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
