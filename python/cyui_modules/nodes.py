from dataclasses import dataclass, field
from copy import deepcopy
from math import hypot
from .common import Edit, select, snap, ui
from .canvas import Canvas, CanvasState
from .node_data import (NodeGroup, NodeReroute, NodeFragment, copy_node_selection, nodes_to_json, nodes_from_json, clone_nodes)


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
    overlay: object = None
    overlay_before: list = field(default_factory=list)
    overlay_preview: list = field(default_factory=list)
    context: tuple = (None, None, (0, 0))
    context_label: str = ''
    error: str = ''


def arrange_nodes(nodes, links, spacing=(240, 130)):
    owners = {p.id: n.id for n in nodes for p in n.inputs+n.outputs}
    indegree, rank, successors = {n.id: 0 for n in nodes}, {}, {}
    for link in links:
        a, b = owners.get(link.source), owners.get(link.target)
        if a is not None and b is not None and a != b:
            successors.setdefault(a, []).append(b)
            indegree[b] += 1
    queue, placed, cursor = [], set(), 0
    def place(node):
        if node not in placed:
            placed.add(node)
            queue.append(node)
    def drain():
        nonlocal cursor
        while cursor < len(queue):
            a = queue[cursor]; cursor += 1
            for b in successors.get(a, []):
                if b in placed:
                    continue  # a back edge into a broken cycle
                rank[b] = max(rank.get(b, 0), rank.get(a, 0)+1)
                indegree[b] -= 1
                if not indegree[b]:
                    place(b)
    for n in nodes:
        if not indegree[n.id]:
            place(n.id)
    drain()
    # Nodes left over sit in or after a cycle: place the first, ignoring its remaining inputs.
    for n in nodes:
        if n.id not in placed:
            place(n.id)
            drain()
    rows, result = {}, {}
    for node in nodes:
        col = rank.get(node.id, 0)
        result[node.id] = col*spacing[0], rows.get(col, 0)*spacing[1]
        rows[col] = rows.get(col, 0)+1
    return result


def node_editor(identifier, state, nodes, links, size=(0, 320), revision=0, snap_step=0., read_only=False, can_connect=None, draw_body=None, groups=(), reroutes=(), allocate_id=None):
    i, c = ui()
    result = {'moved': Edit(), 'connect': None, 'remove_nodes': [], 'remove_link': None, 'created': NodeFragment(), 'groups': Edit(), 'reroutes': Edit(), 'remove_groups': [], 'remove_reroutes': []}
    with Canvas(identifier, state.canvas, size) as f:
        io, p, d = i.get_io(), c.palette(), f.draw
        if state.overlay and (revision != state.revision or read_only or i.is_key_pressed(i.Key.escape)):
            result['reroutes' if state.overlay[0] == 'route' else 'groups'] = Edit('cancel'); state.overlay = None
        shown_groups = state.overlay_preview if state.overlay and state.overlay[0] != 'route' else groups
        shown_routes = state.overlay_preview if state.overlay and state.overlay[0] == 'route' else reroutes
        hit_group, hit_route, resize = None, None, False
        for group in shown_groups:
            a = state.canvas.view.screen(group.position, f.top); b = state.canvas.view.screen(tuple(group.position[k]+group.size[k] for k in (0, 1)), f.top)
            d.add_rect_filled(a, b, c.alpha(c.categorical(group.color_index), .09), c.dp(4)); d.add_rect(a, b, p.accent if group.id in state.selection else p.lineStrong, c.dp(4))
            d.add_text((a[0]+c.dp(8), a[1]+c.dp(4)), p.textDim, group.label)
            corner = b[0]-c.dp(12) <= io.mouse_pos.x <= b[0] and b[1]-c.dp(12) <= io.mouse_pos.y <= b[1]
            if (a[0] <= io.mouse_pos.x <= b[0] and a[1] <= io.mouse_pos.y <= a[1]+i.get_frame_height()) or corner: hit_group, resize = group, corner
        routing = {}
        for route in shown_routes:
            routing.setdefault(route.link, []).append(state.canvas.view.screen(route.position, f.top))
            at = state.canvas.view.screen(route.position, f.top)
            if hypot(at[0]-io.mouse_pos.x, at[1]-io.mouse_pos.y) < c.dp(9): hit_route = route
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
                if link.id in routing:
                    points = [a]+routing[link.id]+[b]
                    for previous, point in zip(points, points[1:]):
                        d.add_line(previous, point, c.categorical(link.color_index), c.dp(2))
                        dx, dy = point[0]-previous[0], point[1]-previous[1]
                        k = max(0, min(1, ((io.mouse_pos.x-previous[0])*dx+(io.mouse_pos.y-previous[1])*dy)/max(1e-6, dx*dx+dy*dy)))
                        distance = hypot(io.mouse_pos.x-previous[0]-k*dx, io.mouse_pos.y-previous[1]-k*dy)
                        if distance < link_distance: hit_link, link_distance = link.id, distance
                    continue
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
        for route in shown_routes:
            at = state.canvas.view.screen(route.position, f.top); d.add_circle_filled(at, c.dp(5), p.bg3); d.add_circle(at, c.dp(5), p.accent)
        if f.hovered and hit_link is not None and hit_node is None and hit_pin is None and not read_only and not allocate_id and i.is_mouse_clicked(1):
            result['remove_link'] = hit_link
        elif f.hovered and i.is_mouse_clicked(1):
            state.context = ('route' if hit_route else 'group' if hit_group else 'link' if hit_link is not None else None, hit_route.id if hit_route else hit_group.id if hit_group else hit_link, f.mouse)
            state.context_label = hit_group.label if hit_group else ''; i.open_popup('##nodeMenu')
        if i.begin_popup('##nodeMenu'):
            kind, key, position = state.context
            if i.menu_item(c.tr_label('Add comment group'), '', False, not read_only and allocate_id is not None)[0]: result['created'].groups.append(NodeGroup(allocate_id(), position, label=c.tr('Comment')))
            if kind == 'link':
                if i.menu_item(c.tr_label('Add reroute'), '', False, not read_only and allocate_id is not None)[0]: result['created'].reroutes.append(NodeReroute(allocate_id(), key, position))
                if i.menu_item(c.tr_label('Remove link'), '', False, not read_only)[0]: result['remove_link'] = key
            if kind == 'route' and i.menu_item(c.tr_label('Remove reroute'), '', False, not read_only)[0]: result['remove_reroutes'].append(key)
            if kind == 'group':
                accepted, state.context_label = i.input_text('##label', state.context_label, i.InputTextFlags_.enter_returns_true)
                if accepted and not read_only:
                    changed = deepcopy(groups)
                    for group in changed:
                        if group.id == key: group.label = state.context_label
                    result['groups'] = Edit('commit', changed); i.close_current_popup()
                if i.menu_item(c.tr_label('Remove group'), '', False, not read_only)[0]: result['remove_groups'].append(key)
            i.end_popup()
        overlay_hit = hit_node is None and hit_pin is None and (hit_group is not None or hit_route is not None)
        if f.hovered and i.is_mouse_clicked(0) and overlay_hit and not read_only and not state.canvas.panning:
            state.focused, state.grab, state.revision = True, f.mouse, revision
            state.overlay = ('route', hit_route.id) if hit_route else ('resize' if resize else 'group', hit_group.id)
            state.overlay_before = deepcopy(reroutes if hit_route else groups); state.overlay_preview = deepcopy(state.overlay_before)
            result['reroutes' if hit_route else 'groups'] = Edit('begin', deepcopy(state.overlay_preview))
            if hit_group: select(state.selection, hit_group.id, io.key_ctrl)
        if state.overlay:
            kind, key = state.overlay; state.overlay_preview = deepcopy(state.overlay_before)
            for overlay in state.overlay_preview:
                if overlay.id == key:
                    if kind == 'resize': overlay.size = tuple(max(80 if k == 0 else 50, overlay.size[k]+f.mouse[k]-state.grab[k]) for k in (0, 1))
                    else: overlay.position = tuple(overlay.position[k]+f.mouse[k]-state.grab[k] for k in (0, 1))
            name = 'reroutes' if kind == 'route' else 'groups'
            if result[name].phase != 'begin': result[name] = Edit('update' if i.is_mouse_down(0) else 'commit', deepcopy(state.overlay_preview))
            if not i.is_mouse_down(0): state.overlay = None
        if f.hovered and i.is_mouse_clicked(0) and not state.canvas.panning and not overlay_hit and not state.overlay:
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
            if io.key_ctrl and i.is_key_pressed(i.Key.c): i.set_clipboard_text(nodes_to_json(copy_node_selection(state.selection, nodes, links, groups, reroutes)))
            if not read_only and allocate_id and io.key_ctrl and (i.is_key_pressed(i.Key.v) or i.is_key_pressed(i.Key.d)):
                try:
                    source = copy_node_selection(state.selection, nodes, links, groups, reroutes) if i.is_key_pressed(i.Key.d) else nodes_from_json(i.get_clipboard_text())
                    result['created'] = clone_nodes(source, allocate_id); state.selection = [n.id for n in result['created'].nodes]+[g.id for g in result['created'].groups]
                except (ValueError, KeyError, TypeError) as error: state.error = str(error)
            if io.key_ctrl and i.is_key_pressed(i.Key.a):
                state.selection = [n.id for n in nodes]
            if not read_only and i.is_key_pressed(i.Key.delete):
                result['remove_nodes'] = list(state.selection)
                result['remove_groups'] = [g.id for g in groups if g.id in state.selection]
            if i.is_key_pressed(i.Key.home) and nodes:
                state.canvas.view.fit(tuple(min(n.position[k] for n in nodes) for k in (0, 1)), tuple(max(n.position[k]+(c.dp(190) if k == 0 else c.dp(220)) for n in nodes) for k in (0, 1)), f.size, c.dp(24))
    return result
