from dataclasses import dataclass, field
from copy import deepcopy
from math import hypot
from .common import Edit, select, snap, ui
from .canvas import Canvas, CanvasState


@dataclass
class Pin:
    id: int
    label: str = ''
    color_index: int = 0


@dataclass
class Node:
    id: int
    position: tuple
    label: str
    inputs: list = field(default_factory=list)
    outputs: list = field(default_factory=list)
    color_index: int = 0


@dataclass
class Link:
    id: int
    source: int
    target: int
    color_index: int = 0


@dataclass
class NodeState:
    canvas: CanvasState = field(default_factory=CanvasState)
    selection: list = field(default_factory=list)
    before: dict = field(default_factory=dict)
    preview: dict = field(default_factory=dict)
    grab: tuple = (0, 0)
    wire: object = None
    dragging: bool = False
    box: object = None
    focused: bool = False
    revision: int = 0


def arrange_nodes(nodes, links, spacing=(240, 130)):
    owners = {p.id: n.id for n in nodes for p in n.inputs+n.outputs}
    indegree, rank, successors = {n.id: 0 for n in nodes}, {}, {}
    for link in links:
        a, b = owners.get(link.source), owners.get(link.target)
        if a is not None and b is not None and a != b:
            successors.setdefault(a, []).append(b)
            indegree[b] += 1
    queue = [n.id for n in nodes if not indegree[n.id]]
    for a in queue:
        for b in successors.get(a, []):
            rank[b] = max(rank.get(b, 0), rank.get(a, 0)+1)
            indegree[b] -= 1
            if not indegree[b]:
                queue.append(b)
    rows, result = {}, {}
    for node in nodes:
        col = rank.get(node.id, 0)
        result[node.id] = col*spacing[0], rows.get(col, 0)*spacing[1]
        rows[col] = rows.get(col, 0)+1
    return result


def node_editor(identifier, state, nodes, links, size=(0, 320), revision=0, snap_step=0., read_only=False, can_connect=None, draw_body=None):
    i, c = ui()
    result = {'moved': Edit(), 'connect': None, 'remove_nodes': [], 'remove_link': None}
    with Canvas(identifier, state.canvas, size) as f:
        io, p, d = i.get_io(), c.palette(), f.draw
        if (state.dragging or state.wire) and (revision != state.revision or read_only or i.is_key_pressed(i.Key.escape)):
            state.dragging, state.wire = False, None
            result['moved'] = Edit('cancel')
        z = state.canvas.view.zoom
        width, header, row = c.dp(190)*z, c.dp(28)*z, c.dp(24)*z
        cards, pins = {}, {}
        for node in nodes:
            a = state.canvas.view.screen(state.preview.get(node.id, node.position) if state.dragging else node.position, f.top)
            b = a[0]+width, a[1]+header+row*max(1, len(node.inputs), len(node.outputs))+c.dp(8)*z
            cards[node.id] = a, b
            for output, ports in ((False, node.inputs), (True, node.outputs)):
                for n, port in enumerate(ports):
                    pins[port.id] = ((b[0] if output else a[0], a[1]+header+row*(n+.5)), output, node.id)
        def wire(a, b, color):
            bend = max(c.dp(35)*z, abs(b[0]-a[0])*.5)
            d.add_bezier_cubic(a, (a[0]+bend, a[1]), (b[0]-bend, b[1]), b, color, c.dp(2))
        hit_link, link_distance = None, c.dp(7)
        for link in links:
            if link.source in pins and link.target in pins:
                a, b = pins[link.source][0], pins[link.target][0]
                wire(a, b, c.categorical(link.color_index))
                bend, previous = max(c.dp(35)*z, abs(b[0]-a[0])*.5), a
                for sample in range(1, 33):
                    t = sample/32; u = 1-t
                    point = (u*u*u*a[0]+3*u*u*t*(a[0]+bend)+3*u*t*t*(b[0]-bend)+t*t*t*b[0], u*u*u*a[1]+3*u*u*t*a[1]+3*u*t*t*b[1]+t*t*t*b[1])
                    dx, dy = point[0]-previous[0], point[1]-previous[1]
                    k = max(0, min(1, ((io.mouse_pos.x-previous[0])*dx+(io.mouse_pos.y-previous[1])*dy)/max(1e-6, dx*dx+dy*dy)))
                    distance = hypot(io.mouse_pos.x-previous[0]-k*dx, io.mouse_pos.y-previous[1]-k*dy)
                    if distance < link_distance:
                        hit_link, link_distance = link.id, distance
                    previous = point
        hit_node, hit_pin = None, None
        for node in nodes:
            a, b = cards[node.id]
            if a[0] <= io.mouse_pos.x <= b[0] and a[1] <= io.mouse_pos.y <= b[1]:
                hit_node = node.id
            d.add_rect_filled(a, b, p.bg2, c.dp(4)*z)
            d.add_rect_filled(a, (b[0], a[1]+header), c.mix(p.bg2, c.categorical(node.color_index), .35), c.dp(4)*z)
            d.add_rect(a, b, p.accent if node.id in state.selection else p.line, c.dp(4)*z)
            d.add_text((a[0]+c.dp(8)*z, a[1]+c.dp(5)*z), p.text, node.label)
            for output, ports in ((False, node.inputs), (True, node.outputs)):
                for n, port in enumerate(ports):
                    d.add_text((a[0]+(width*.54 if output else c.dp(10)*z), a[1]+header+row*n), p.textDim, port.label)
            if draw_body:
                draw_body(node, (a, b))
        for key, (at, output, owner) in pins.items():
            d.add_circle_filled(at, c.dp(4)*max(.6, z), p.info)
            if hypot(at[0]-io.mouse_pos.x, at[1]-io.mouse_pos.y) < c.dp(9):
                hit_pin = key
        if f.hovered and hit_link and hit_node is None and hit_pin is None and not read_only and i.is_mouse_clicked(1):
            result['remove_link'] = hit_link
        if f.hovered and i.is_mouse_clicked(0) and not state.canvas.panning:
            state.focused = True
            if hit_pin is not None and not read_only:
                state.wire, state.revision = hit_pin, revision
            elif hit_node is not None:
                if hit_node not in state.selection or io.key_ctrl:
                    select(state.selection, hit_node, io.key_ctrl or io.key_shift)
                if not read_only:
                    state.before = {n.id: n.position for n in nodes if n.id in state.selection}
                    state.preview, state.grab, state.dragging, state.revision = dict(state.before), f.mouse, True, revision
                    result['moved'] = Edit('begin', dict(state.preview))
            else:
                if not io.key_ctrl and not io.key_shift:
                    state.selection.clear()
                state.box = f.mouse
        elif i.is_mouse_clicked(0):
            state.focused = False
        if state.dragging:
            if i.is_mouse_down(0):
                state.preview = {key: tuple(value[n]+f.mouse[n]-state.grab[n] for n in (0, 1)) for key, value in state.before.items()}
                if not io.key_alt:
                    state.preview = {key: tuple(snap(v, snap_step) for v in value) for key, value in state.preview.items()}
                if result['moved'].phase != 'begin' and (io.mouse_delta.x or io.mouse_delta.y):
                    result['moved'] = Edit('update', dict(state.preview))
            else:
                result['moved'], state.dragging = Edit('commit', dict(state.preview)), False
        if state.wire is not None and state.wire in pins:
            source = pins[state.wire]
            wire(source[0] if source[1] else tuple(io.mouse_pos), tuple(io.mouse_pos) if source[1] else source[0], p.accent)
            if not i.is_mouse_down(0):
                if hit_pin is not None and pins[hit_pin][1] != source[1] and pins[hit_pin][2] != source[2]:
                    pair = (state.wire, hit_pin) if source[1] else (hit_pin, state.wire)
                    if not can_connect or can_connect(*pair):
                        result['connect'] = pair
                state.wire = None
        if state.box:
            a, b = state.canvas.view.screen(state.box, f.top), tuple(io.mouse_pos)
            low, high = tuple(min(a[n], b[n]) for n in (0, 1)), tuple(max(a[n], b[n]) for n in (0, 1))
            d.add_rect_filled(low, high, p.selection)
            if not i.is_mouse_down(0):
                for key, (a, b) in cards.items():
                    if all(low[n] <= a[n] and b[n] <= high[n] for n in (0, 1)) and key not in state.selection:
                        state.selection.append(key)
                state.box = None
        if state.focused and f.focused and not io.want_text_input:
            if io.key_ctrl and i.is_key_pressed(i.Key.a):
                state.selection = [n.id for n in nodes]
            if not read_only and i.is_key_pressed(i.Key.delete):
                result['remove_nodes'] = list(state.selection)
            if i.is_key_pressed(i.Key.home) and nodes:
                state.canvas.view.fit(tuple(min(n.position[k] for n in nodes) for k in (0, 1)), tuple(max(n.position[k]+(c.dp(190) if k == 0 else c.dp(220)) for n in nodes) for k in (0, 1)), f.size, c.dp(24))
    return result
