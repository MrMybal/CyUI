from dataclasses import dataclass, field
from copy import deepcopy
from bisect import bisect_right
from math import hypot, isfinite, exp
from .common import Edit, Range, select, snap, ui, zoom_range, take_wheel, wheel_taken
from .canvas import Canvas, CanvasState
from .plots import PlotState


@dataclass
class CurvePoint:
    id: int
    time: float
    value: float
    shape: str = 'linear'
    incoming: tuple = (-.1, 0.)
    outgoing: tuple = (.1, 0.)


def evaluate_curve(points, time, fallback=0.):
    if not points or not isfinite(time):
        return fallback
    low, high = 0, len(points)
    while low < high:
        middle = (low+high)//2
        if points[middle].time <= time:
            low = middle+1
        else:
            high = middle
    index = max(0, low-1)
    a = points[index]
    if time < points[0].time or index + 1 == len(points):
        return a.value
    b = points[index + 1]
    if b.time <= a.time:
        return b.value
    k = max(0., min(1., (time - a.time) / (b.time - a.time)))
    if a.shape == 'step':
        return a.value
    if a.shape == 'smooth':
        k = k * k * (3 - 2 * k)
    if a.shape == 'bezier':
        def bezier(a, b, c, d, t):
            u = 1 - t
            return u*u*u*a + 3*u*u*t*b + 3*u*t*t*c + t*t*t*d
        low, high = 0., 1.
        span = b.time - a.time
        for _ in range(40):
            k = (low + high) / 2
            if bezier(a.time, a.time + max(0., min(span, a.outgoing[0])), b.time + max(-span, min(0., b.incoming[0])), b.time, k) < time:
                low = k
            else:
                high = k
        return bezier(a.value, a.value + a.outgoing[1], b.value + b.incoming[1], b.value, k)
    return a.value + (b.value - a.value) * k


@dataclass
class CurveState:
    view: PlotState = field(default_factory=PlotState)
    selection: list = field(default_factory=list)
    before: list = field(default_factory=list)
    preview: list = field(default_factory=list)
    clipboard: list = field(default_factory=list)
    grab: tuple = (0, 0)
    box: object = None
    active: int = 0
    handle: int = 0
    focused: bool = False
    revision: int = 0


def curve_editor(identifier, state, points, size=(0, 280), snap_step=0., revision=0, read_only=False, evaluator=None, snap_time=None):
    i, c = ui()
    edit = Edit()
    with Canvas(identifier, CanvasState(), size, False, False) as f:
        io, p, draw = i.get_io(), c.palette(), f.draw
        mouse = (state.view.x.value((io.mouse_pos.x-f.top[0])/f.size[0]), state.view.y.value(1-(io.mouse_pos.y-f.top[1])/f.size[1]))
        def screen(time, value):
            return f.top[0]+state.view.x.fraction(time)*f.size[0], f.bottom[1]-state.view.y.fraction(value)*f.size[1]
        if state.active and (revision != state.revision or read_only or i.is_key_pressed(i.Key.escape)):
            state.active = 0
            edit = Edit('cancel')
        if f.hovered and io.mouse_wheel and not wheel_taken():
            state.view.x.view = zoom_range(state.view.x.view, mouse[0], exp(-io.mouse_wheel*.15))
            take_wheel()
        if f.hovered and i.is_mouse_dragging(2):
            delta = -io.mouse_delta.x/f.size[0]*state.view.x.view.span
            moved = Range(state.view.x.view.minimum+delta, state.view.x.view.maximum+delta)
            if moved.valid:
                state.view.x.view = moved
        shown = state.preview if state.active else points
        hit, handle, distance = 0, 0, c.dp(8)
        for point in shown:
            q = screen(point.time, point.value)
            d = hypot(q[0]-io.mouse_pos.x, q[1]-io.mouse_pos.y)
            if d < distance:
                hit, handle, distance = point.id, 0, d
            if point.id in state.selection and point.shape == 'bezier':
                for side, tangent in ((-1, point.incoming), (1, point.outgoing)):
                    at = screen(point.time+tangent[0], point.value+tangent[1])
                    draw.add_line(q, at, p.textFaint, c.dp(1))
                    draw.add_circle_filled(at, c.dp(3), p.info)
                    d = hypot(at[0]-io.mouse_pos.x, at[1]-io.mouse_pos.y)
                    if d < distance:
                        hit, handle, distance = point.id, side, d
        if f.hovered and i.is_mouse_double_clicked(0) and not read_only:
            value = [deepcopy(x) for x in points if x.id != hit] if hit else deepcopy(points) + [CurvePoint(max([x.id for x in points]+[0])+1, snap_time(mouse[0]) if snap_time else snap(mouse[0], snap_step), mouse[1])]
            state.active = 0
            edit = Edit('commit', sorted(value, key=lambda x: x.time))
        elif f.hovered and i.is_mouse_clicked(0):
            state.focused = True
            if hit:
                if hit not in state.selection or io.key_ctrl:
                    select(state.selection, hit, io.key_ctrl or io.key_shift)
                if not read_only and hit in state.selection:
                    state.active, state.handle, state.grab, state.revision = hit, handle, mouse, revision
                    state.before, state.preview = deepcopy(points), deepcopy(points)
                    edit = Edit('begin', deepcopy(state.preview))
            else:
                if not io.key_ctrl and not io.key_shift:
                    state.selection.clear()
                state.box = mouse
        elif i.is_mouse_clicked(0):
            state.focused = False
        if state.active:
            if i.is_mouse_down(0):
                state.preview = deepcopy(state.before)
                # Only the grabbed key snaps; the other selected keys keep their spacing.
                grabbed = next((x for x in state.before if x.id == state.active), None)
                shift = mouse[0]-state.grab[0]
                if grabbed and not io.key_alt:
                    shift = (snap_time(grabbed.time+shift) if snap_time else snap(grabbed.time+shift, snap_step))-grabbed.time
                for point in state.preview:
                    if point.id not in state.selection:
                        continue
                    if state.handle and point.id == state.active:
                        tangent = point.incoming if state.handle < 0 else point.outgoing
                        tx = tangent[0]+mouse[0]-state.grab[0]
                        tangent = (min(0., tx) if state.handle < 0 else max(0., tx), tangent[1]+mouse[1]-state.grab[1])
                        if state.handle < 0:
                            point.incoming = tangent
                        else:
                            point.outgoing = tangent
                    elif not state.handle:
                        point.time += shift
                        point.value += mouse[1]-state.grab[1]
                state.preview.sort(key=lambda x: x.time)
                if edit.phase != 'begin' and (io.mouse_delta.x or io.mouse_delta.y):
                    edit = Edit('update', deepcopy(state.preview))
            else:
                edit, state.active = Edit('commit', deepcopy(state.preview)), 0
        if state.box:
            a, b = screen(*state.box), screen(*mouse)
            low, high = (min(a[0], b[0]), min(a[1], b[1])), (max(a[0], b[0]), max(a[1], b[1]))
            draw.add_rect_filled(low, high, p.selection)
            if not i.is_mouse_down(0):
                for point in points:
                    at = screen(point.time, point.value)
                    if all(low[n] <= at[n] <= high[n] for n in (0, 1)) and point.id not in state.selection:
                        state.selection.append(point.id)
                state.box = None
        if state.focused and f.focused and not io.want_text_input:
            if io.key_ctrl and i.is_key_pressed(i.Key.a):
                state.selection = [p.id for p in points]
            if io.key_ctrl and i.is_key_pressed(i.Key.c):
                state.clipboard = deepcopy([p for p in points if p.id in state.selection])
            if not read_only and io.key_ctrl and i.is_key_pressed(i.Key.v) and state.clipboard:
                value = deepcopy(points)
                for point in deepcopy(state.clipboard):
                    point.id = max([p.id for p in value]+[0])+1
                    point.time += mouse[0]-state.clipboard[0].time
                    value.append(point)
                edit = Edit('commit', sorted(value, key=lambda p: p.time))
            if not read_only and i.is_key_pressed(i.Key.delete):
                edit = Edit('commit', [deepcopy(p) for p in points if p.id not in state.selection])
        shown = state.preview if state.active else edit.value if edit.committed else points
        previous = None
        # The evaluator may preserve an application's own interpolation.
        for pixel in range(min(2048, max(2, int(f.size[0])))+1):
            time = state.view.x.value(pixel/min(2048, max(2, int(f.size[0]))))
            value = (evaluator or evaluate_curve)(shown, time)
            at = screen(time, value)
            if all(map(isfinite, at)):
                if previous:
                    draw.add_line(previous, at, p.accent, c.dp(1.5))
                previous = at
            else:
                previous = None
        for point in shown:
            draw.add_circle_filled(screen(point.time, point.value), c.dp(4 if point.id in state.selection else 3), p.text if point.id in state.selection else p.accent)
    return edit
