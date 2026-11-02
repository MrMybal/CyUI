from dataclasses import dataclass, field
from copy import deepcopy
from math import exp, floor, log10
from .common import Edit, Range, select, snap, ui, zoom_range, take_wheel, wheel_taken
from .canvas import Canvas, CanvasState


@dataclass
class Lane:
    id: int
    label: str
    height: float = 56


@dataclass
class Clip:
    id: int
    lane: int
    start: float
    duration: float
    label: str = ''
    color_index: int = 0
    locked: bool = False


@dataclass
class Marker:
    id: int
    time: float
    label: str = ''


@dataclass
class TimelineState:
    view: Range = field(default_factory=lambda: Range(0, 10))
    selection: Range = field(default_factory=lambda: Range(0, 0))
    anchor: float = 0
    selected: list = field(default_factory=list)
    vertical_scroll: float = 0
    before: list = field(default_factory=list)
    preview: list = field(default_factory=list)
    active: int = 0
    mode: int = 0
    grab: float = 0
    revision: int = 0
    focused: bool = False
    panning: bool = False


def timeline(identifier, state, lanes, clips, markers=(), size=(0, 300), playhead=0., snap_step=0., revision=0, read_only=False, snap_time=None, format_time=None, draw_clip=None, min_duration=1e-6):
    i, c = ui()
    result = {'clips': Edit(), 'seek': None, 'range_changed': False, 'remove': []}
    with Canvas(identifier, CanvasState(), size, False, False) as f:
        io, p, d = i.get_io(), c.palette(), f.draw
        if not state.view.valid:
            state.view = Range(0, 10)
        header, ruler = min(c.dp(150), f.size[0]*.3), i.get_frame_height()
        left, width = f.top[0]+header, max(c.dp(1), f.size[0]-header)
        def x(time):
            return left+(time-state.view.minimum)/state.view.span*width
        mouse = state.view.minimum+(io.mouse_pos.x-left)/width*state.view.span
        def snapped(time):
            return time if io.key_alt else snap_time(time) if snap_time else snap(time, snap_step)
        if state.mode > 0 and (state.revision != revision or read_only or i.is_key_pressed(i.Key.escape)):
            state.mode = 0
            result['clips'] = Edit('cancel')
        if f.hovered and io.mouse_wheel and not wheel_taken():
            if io.key_ctrl:
                state.vertical_scroll = max(0, state.vertical_scroll-io.mouse_wheel*c.dp(40))
            elif io.key_shift:
                delta = -io.mouse_wheel*state.view.span*.1
                state.view = Range(state.view.minimum+delta, state.view.maximum+delta)
            else:
                state.view = zoom_range(state.view, mouse, exp(-io.mouse_wheel*.15))
            take_wheel()
        if f.hovered and i.is_mouse_clicked(2):
            state.panning = True
        if not i.is_mouse_down(2):
            state.panning = False
        if state.panning:
            delta = -io.mouse_delta.x/width*state.view.span
            state.view = Range(state.view.minimum+delta, state.view.maximum+delta)
        rows, y = {}, f.top[1]+ruler-state.vertical_scroll
        hovered_lane = None
        for lane in lanes:
            height = c.dp(max(24, lane.height))
            rows[lane.id] = y, y+height
            if y <= io.mouse_pos.y < y+height:
                hovered_lane = lane.id
            if y+height >= f.top[1]+ruler and y < f.bottom[1]:
                d.add_rect_filled((f.top[0], max(y, f.top[1]+ruler)), (left, y+height), p.bg2)
                d.add_text((f.top[0]+c.dp(8), max(y, f.top[1]+ruler)+c.dp(5)), p.textDim, lane.label)
            y += height
        state.vertical_scroll = max(0, min(state.vertical_scroll, y-f.top[1]-f.size[1]+state.vertical_scroll))
        desired = state.view.span*c.dp(90)/width
        power = 10**floor(log10(max(1e-12, desired)))
        step = (1 if desired/power < 2 else 2 if desired/power < 5 else 5)*power
        first = floor(state.view.minimum/step)
        # An integer index: t += step stops advancing once step is below the spacing of t.
        for n in range(1000):
            time = (first+n)*step
            if time > state.view.maximum:
                break
            at = x(time)
            d.add_line((at, f.top[1]), (at, f.bottom[1]), p.grid, c.dp(1))
            d.add_text((at+c.dp(3), f.top[1]+c.dp(3)), p.textDim, format_time(time) if format_time else f'{time:.5g}')
        hit, edge = None, 1
        shown = state.preview if state.mode > 0 else clips
        d.push_clip_rect((left, f.top[1]+ruler), f.bottom, True)
        for clip in shown:
            if clip.lane not in rows or clip.start+clip.duration < state.view.minimum or clip.start > state.view.maximum:
                continue
            top, bottom = rows[clip.lane]
            a, b = (x(clip.start), top+c.dp(3)), (x(clip.start+clip.duration), bottom-c.dp(3))
            d.add_rect_filled(a, b, c.mix(p.bg2, c.categorical(clip.color_index), .45), c.dp(3))
            d.add_rect(a, b, p.accent if clip.id in state.selected else p.line, c.dp(3))
            d.add_text((a[0]+c.dp(5), a[1]+c.dp(3)), p.text, clip.label)
            if draw_clip:
                draw_clip(clip, (a, b))
            if a[0] <= io.mouse_pos.x <= b[0] and a[1] <= io.mouse_pos.y <= b[1]:
                hit, edge = clip.id, 2 if abs(io.mouse_pos.x-a[0]) < c.dp(7) else 3 if abs(io.mouse_pos.x-b[0]) < c.dp(7) else 1
        if state.selection.minimum != state.selection.maximum:
            d.add_rect_filled((x(min(state.selection.minimum, state.selection.maximum)), f.top[1]+ruler), (x(max(state.selection.minimum, state.selection.maximum)), f.bottom[1]), p.selection)
        d.pop_clip_rect()
        for marker in markers:
            d.add_line((x(marker.time), f.top[1]), (x(marker.time), f.bottom[1]), p.warning, c.dp(1))
        d.add_line((x(playhead), f.top[1]), (x(playhead), f.bottom[1]), p.accent, c.dp(2))
        if f.hovered and i.is_mouse_clicked(0):
            state.focused = True
            if io.mouse_pos.x >= left and io.mouse_pos.y < f.top[1]+ruler:
                result['seek'] = snapped(mouse)
            elif hit is not None:
                if hit not in state.selected or io.key_ctrl:
                    select(state.selected, hit, io.key_ctrl or io.key_shift)
                clip = next(c for c in clips if c.id == hit)
                if not read_only and not clip.locked:
                    state.active, state.mode, state.grab, state.revision = hit, edge, mouse, revision
                    state.before, state.preview = deepcopy(clips), deepcopy(clips)
                    result['clips'] = Edit('begin', deepcopy(state.preview))
            elif io.mouse_pos.x >= left:
                state.selection, state.anchor, state.mode = Range(mouse, mouse), mouse, -1
                result['seek'] = mouse
                if not io.key_ctrl:
                    state.selected.clear()
        elif i.is_mouse_clicked(0):
            state.focused = False
        if state.mode > 0:
            if i.is_mouse_down(0):
                state.preview = deepcopy(state.before)
                # Only the grabbed clip snaps; the other selected clips keep their spacing.
                grabbed = next((c for c in state.before if c.id == state.active), None)
                shift = snapped(grabbed.start+mouse-state.grab)-grabbed.start if grabbed else mouse-state.grab
                for clip in state.preview:
                    if clip.locked or not ((state.mode == 1 and clip.id in state.selected) or clip.id == state.active):
                        continue
                    if state.mode == 1:
                        clip.start += shift
                        if clip.id == state.active and hovered_lane is not None:
                            clip.lane = hovered_lane
                    elif state.mode == 2:
                        end = clip.start+clip.duration
                        clip.start = min(end-min_duration, snapped(clip.start+mouse-state.grab))
                        clip.duration = end-clip.start
                    else:
                        clip.duration = max(min_duration, snapped(clip.start+clip.duration+mouse-state.grab)-clip.start)
                if result['clips'].phase != 'begin' and (io.mouse_delta.x or io.mouse_delta.y):
                    result['clips'] = Edit('update', deepcopy(state.preview))
            else:
                result['clips'], state.mode = Edit('commit', deepcopy(state.preview)), 0
        if state.mode == -1:
            state.selection = Range(min(state.anchor, mouse), max(state.anchor, mouse))
            result['range_changed'] = True
            if not i.is_mouse_down(0):
                state.mode = 0
        if state.focused and f.focused and not io.want_text_input and not read_only and i.is_key_pressed(i.Key.delete):
            result['remove'] = [c.id for c in clips if c.id in state.selected and not c.locked]
    return result
