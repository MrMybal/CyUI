from dataclasses import dataclass, field
from copy import deepcopy
from math import exp, floor, log10, isfinite
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
    marker_edit: object = None
    lane_drag: object = None
    marker_before: list = field(default_factory=list)
    marker_preview: list = field(default_factory=list)
    context_marker: object = None
    marker_label: str = ''
    error: str = ''


def _clip_operation(clips, selection, value, allocate_id, split=False, minimum=1e-6):
    if not allocate_id or not isfinite(value) or not isfinite(minimum) or minimum <= 0: raise ValueError('Invalid timeline operation')
    used = {clip.id for clip in clips}
    if len(used) != len(clips) or any(not isfinite(c.start) or not isfinite(c.duration) or c.duration <= 0 or not isfinite(c.start+c.duration) for c in clips): raise ValueError('Invalid clips')
    result = []
    for clip in deepcopy(clips):
        active = clip.id in selection and not clip.locked
        if split and active and value-clip.start >= minimum and clip.start+clip.duration-value >= minimum:
            copy = deepcopy(clip); copy.id = allocate_id()
            if not copy.id or copy.id in used: raise ValueError('Allocator returned duplicate or zero ID')
            used.add(copy.id); copy.start = value; copy.duration = clip.start+clip.duration-value; clip.duration = value-clip.start; result += [clip, copy]
        else:
            result.append(clip)
            if not split and active:
                copy = deepcopy(clip); copy.id = allocate_id(); copy.start += value
                if not copy.id or copy.id in used or not isfinite(copy.start+copy.duration): raise ValueError('Invalid duplicate ID or position')
                used.add(copy.id); result.append(copy)
    return result


def split_timeline_clips(clips, selection, time, allocate_id, min_duration=1e-6): return _clip_operation(clips, selection, time, allocate_id, True, min_duration)
def duplicate_timeline_clips(clips, selection, offset, allocate_id): return _clip_operation(clips, selection, offset, allocate_id)


def reorder_timeline_lane(lanes, identifier, before=None):
    moved = next((lane for lane in lanes if lane.id == identifier), None)
    if moved is None or before == identifier or (before is not None and not any(lane.id == before for lane in lanes)): return list(lanes)
    result = [lane for lane in lanes if lane.id != identifier]
    result.insert(next((n for n, lane in enumerate(result) if lane.id == before), len(result)), moved)
    return result


def add_timeline_marker(markers, time, allocate_id, label='Marker'):
    if not allocate_id or not isfinite(time): raise ValueError('Invalid marker position or allocator')
    key = allocate_id()
    if not key or any(marker.id == key for marker in markers): raise ValueError('Allocator returned duplicate or zero ID')
    return [deepcopy(marker) for marker in markers]+[Marker(key, time, label)]


def timeline(identifier, state, lanes, clips, markers=(), size=(0, 300), playhead=0., snap_step=0., revision=0, read_only=False, snap_time=None, format_time=None, draw_clip=None, min_duration=1e-6, allocate_id=None):
    i, c = ui()
    result = {'clips': Edit(), 'seek': None, 'range_changed': False, 'remove': [], 'markers': Edit(), 'lanes': Edit()}
    with Canvas(identifier, CanvasState(), size, False, False) as f:
        io, p, d = i.get_io(), c.palette(), f.draw
        if (state.marker_edit is not None or state.lane_drag is not None) and (revision != state.revision or read_only or i.is_key_pressed(i.Key.escape)):
            result['markers'] = Edit('cancel'); state.marker_edit = state.lane_drag = None
        if state.marker_edit is not None: markers = state.marker_preview
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
        hit_marker = None
        for marker in markers:
            d.add_line((x(marker.time), f.top[1]), (x(marker.time), f.bottom[1]), p.warning, c.dp(1))
            if io.mouse_pos.y < f.top[1]+ruler and abs(io.mouse_pos.x-x(marker.time)) < c.dp(7): hit_marker = marker
        d.add_line((x(playhead), f.top[1]), (x(playhead), f.bottom[1]), p.accent, c.dp(2))
        if f.hovered and i.is_mouse_clicked(0):
            state.focused = True
            if hit_marker is not None and not read_only:
                state.marker_edit, state.grab, state.revision = hit_marker.id, mouse, revision
                state.marker_before = deepcopy(markers); state.marker_preview = deepcopy(markers); result['markers'] = Edit('begin', deepcopy(markers))
            elif io.mouse_pos.x < left and hovered_lane is not None and not read_only:
                state.lane_drag, state.revision = hovered_lane, revision
            elif io.mouse_pos.x >= left and io.mouse_pos.y < f.top[1]+ruler:
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
        if state.marker_edit is not None:
            state.marker_preview = deepcopy(state.marker_before)
            for marker in state.marker_preview:
                if marker.id == state.marker_edit: marker.time = snapped(marker.time+mouse-state.grab)
            if result['markers'].phase != 'begin': result['markers'] = Edit('update' if i.is_mouse_down(0) else 'commit', deepcopy(state.marker_preview))
            if not i.is_mouse_down(0): state.marker_edit = None
        if state.lane_drag is not None and not i.is_mouse_down(0):
            if hovered_lane is not None: result['lanes'] = Edit('commit', reorder_timeline_lane(lanes, state.lane_drag, hovered_lane))
            state.lane_drag = None
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
        if f.hovered and i.is_mouse_clicked(1):
            state.context_marker = hit_marker.id if hit_marker else None
            state.marker_label = hit_marker.label if hit_marker else ''
            state.anchor = snapped(mouse)
            if hit is not None and hit not in state.selected: state.selected = [hit]
            i.open_popup('##timelineMenu')
        split = duplicate = False
        if i.begin_popup('##timelineMenu'):
            if state.context_marker is not None:
                accepted, state.marker_label = i.input_text('##label', state.marker_label, i.InputTextFlags_.enter_returns_true)
                if accepted and not read_only:
                    values = deepcopy(markers)
                    for marker in values:
                        if marker.id == state.context_marker: marker.label = state.marker_label
                    result['markers'] = Edit('commit', values); i.close_current_popup()
                if i.menu_item(c.tr_label('Remove marker'), '', False, not read_only)[0]: result['markers'] = Edit('commit', [deepcopy(m) for m in markers if m.id != state.context_marker])
            else:
                split = i.menu_item(c.tr_label('Split at playhead'), 'S', False, not read_only and allocate_id is not None)[0]
                duplicate = i.menu_item(c.tr_label('Duplicate'), 'Ctrl+D', False, not read_only and allocate_id is not None)[0]
                if i.menu_item(c.tr_label('Add marker'), '', False, not read_only and allocate_id is not None)[0]:
                    try: result['markers'] = Edit('commit', add_timeline_marker(markers, state.anchor, allocate_id, c.tr('Marker')))
                    except ValueError as error: state.error = str(error)
            i.end_popup()
        if state.focused and f.focused and not io.want_text_input and not read_only and allocate_id:
            split |= not io.key_ctrl and i.is_key_pressed(i.Key.s)
            duplicate |= io.key_ctrl and i.is_key_pressed(i.Key.d)
        try:
            if split: result['clips'] = Edit('commit', split_timeline_clips(clips, state.selected, playhead, allocate_id, min_duration))
            if duplicate:
                picked = [clip for clip in clips if clip.id in state.selected and not clip.locked]
                if picked: result['clips'] = Edit('commit', duplicate_timeline_clips(clips, state.selected, max(clip.start+clip.duration for clip in picked)-min(clip.start for clip in picked), allocate_id))
        except ValueError as error: state.error = str(error)
    return result
