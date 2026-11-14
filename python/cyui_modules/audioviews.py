from dataclasses import dataclass, field
from math import isfinite, isnan, exp, nan, ceil, inf, hypot
from copy import deepcopy
from .common import Range, Edit, ui, zoom_range, take_wheel, wheel_taken
from .canvas import Canvas, CanvasState
from .plots import plot, Axis, PlotState, _navigate


def make_peaks(samples, columns):
    if not len(samples) or columns <= 0:
        return []
    columns = min(columns, len(samples))
    result = []
    for x in range(columns):
        values = [v for v in samples[x*len(samples)//columns:(x+1)*len(samples)//columns] if isfinite(v)]
        result.append((min(values), max(values)) if values else (nan, nan))
    return result


@dataclass
class WaveformState:
    view: Range = field(default_factory=Range)
    selection: Range = field(default_factory=lambda: Range(0, 0))
    anchor: float = 0
    selecting: bool = False
    panning: bool = False


def waveform(identifier, state, channels, coverage, playhead=0., size=(0, 180)):
    i, c = ui()
    result = {'seek': None, 'selection_changed': False}
    with Canvas(identifier, CanvasState(), size, False, False) as f:
        io, p, d = i.get_io(), c.palette(), f.draw
        mouse = state.view.minimum+(io.mouse_pos.x-f.top[0])/f.size[0]*state.view.span
        def x(t):
            return f.top[0]+(t-state.view.minimum)/state.view.span*f.size[0]
        if f.hovered and io.mouse_wheel and not wheel_taken():
            if io.key_shift:
                delta = -io.mouse_wheel*state.view.span*.1
                moved = Range(state.view.minimum+delta, state.view.maximum+delta)
                state.view = moved if moved.valid else state.view
            else:
                state.view = zoom_range(state.view, mouse, exp(-io.mouse_wheel*.15))
            take_wheel()
        if f.hovered and i.is_mouse_clicked(2):
            state.panning = True
        if not i.is_mouse_down(2):
            state.panning = False
        if state.panning:
            delta = -io.mouse_delta.x/f.size[0]*state.view.span
            moved = Range(state.view.minimum+delta, state.view.maximum+delta)
            state.view = moved if moved.valid else state.view
        if f.hovered and i.is_mouse_clicked(0):
            state.selection, state.anchor, state.selecting, result['seek'] = Range(mouse, mouse), mouse, True, mouse
        if state.selecting:
            state.selection = Range(min(state.anchor, mouse), max(state.anchor, mouse))
            result['selection_changed'] = True
            if not i.is_mouse_down(0):
                state.selecting = False
        if f.hovered and i.is_mouse_double_clicked(0):
            state.selection, state.selecting = Range(0, 0), False
        if coverage.valid:
            for channel, peaks in enumerate(channels):
                if not len(peaks):
                    continue
                height = f.size[1]/len(channels)
                mid = f.top[1]+height*(channel+.5)
                for pixel in range(min(8192, max(1, int(f.size[0])))):
                    t = state.view.minimum+(pixel+.5)/f.size[0]*state.view.span
                    if coverage.contains(t):
                        low, high = peaks[min(len(peaks)-1, int((t-coverage.minimum)/coverage.span*len(peaks)))]
                        if isfinite(low) and isfinite(high):
                            d.add_line((f.top[0]+pixel, mid-max(-1, min(1, high))*height*.45), (f.top[0]+pixel, mid-max(-1, min(1, low))*height*.45), p.info, c.dp(1))
        if state.selection.minimum != state.selection.maximum:
            d.add_rect_filled((x(min(state.selection.minimum, state.selection.maximum)), f.top[1]), (x(max(state.selection.minimum, state.selection.maximum)), f.bottom[1]), p.selection)
        d.add_line((x(playhead), f.top[1]), (x(playhead), f.bottom[1]), p.accent, c.dp(2))
    return result


def spectrum(identifier, state, series, size=(0, 240)):
    state.x.logarithmic = True
    if not state.x.view.valid or state.x.view.minimum <= 0:
        state.x.view = Range(20, 20000)
    return plot(identifier, state, series, size)


def oscilloscope(identifier, state, series, size=(0, 200)):
    state.x.logarithmic = state.y.logarithmic = False
    return plot(identifier, state, series, size)


def stereo_scope(identifier, state, series, size=(0, 200)):
    # Square, with one symmetric extent on both axes: wheel zoom scales it, the centre stays at 0.
    i, c = ui()
    state.x.logarithmic = state.y.logarithmic = False
    avail = i.get_content_region_avail()
    side = max(c.dp(1), min(size[0] if size[0] > 0 else avail.x+size[0], size[1] if size[1] > 0 else avail.y+size[1]))
    extent = state.x.view.span*.5 if state.x.view.valid else 1.
    state.x.view, state.y.view = Range(-extent, extent), Range(-extent, extent)
    result = plot(identifier, state, series, (side, side))
    zoomed = state.x.view.span*.5 if state.x.view.valid else extent
    state.x.view, state.y.view = Range(-zoomed, zoomed), Range(-zoomed, zoomed)
    return result


@dataclass
class SpectrogramData:
    """Frame-major values[frame*bins+bin], increasing time and frequency."""
    values: object = field(default_factory=list)
    frames: int = 0
    bins: int = 0
    time: Range = field(default_factory=Range)
    frequency: Range = field(default_factory=lambda: Range(20, 20000))
    logarithmic_bins: bool = False


def _number(value, unit=''):
    if isnan(value) or value == inf:
        return '--'
    return f'-inf {unit}' if value == -inf else f'{value:.1f} {unit}'


def _hz(value):
    return f'{value/1000:.2g} kHz' if value >= 1000 else f'{value:.3g} Hz'


def _label(d, at, text, color):
    i, c = ui()
    extent = i.calc_text_size(text)
    d.add_rect_filled((at[0]-c.dp(2), at[1]), (at[0]+extent.x+c.dp(2), at[1]+extent.y), c.alpha(c.palette().bg0, .9))
    d.add_text(at, color, text)


def _fraction(value, view):
    return max(0., min(1., (value-view.minimum)/view.span)) if view.valid and not isnan(value) else 0.


def _bar(d, top, bottom, value, view, color, marker=nan):
    _, c = ui()
    d.add_rect_filled(top, bottom, c.palette().bg3, c.dp(2))
    if not isnan(value):
        d.add_rect_filled(top, (top[0]+(bottom[0]-top[0])*_fraction(value, view), bottom[1]), color, c.dp(2))
    if isfinite(marker) and view.contains(marker):
        x = top[0]+(bottom[0]-top[0])*_fraction(marker, view)
        d.add_line((x, top[1]-c.dp(2)), (x, bottom[1]+c.dp(2)), c.palette().text, c.dp(1))


def _height(size):
    i, c = ui()
    return max(c.dp(1), size[1] if size[1] > 0 else i.get_content_region_avail().y+size[1])


def spectrogram(identifier, state, data, size=(0, 240), *, intensity=None, unit='dB', navigation=True, axes=True, max_cells=65536):
    i, c = ui()
    intensity = intensity or Range(-100, 0)
    result = dict(valid=False, hovered=False, clicked=False, view_changed=False, frame=0, bin=0, mouse=(0., 0.), intensity=nan)
    with Canvas(identifier, CanvasState(), size, False, False) as f:
        result['valid'] = (data.frames > 0 and data.bins > 0 and data.frames <= len(data.values)//data.bins
                           and data.time.valid and data.frequency.valid and intensity.valid
                           and (not data.logarithmic_bins or data.frequency.minimum > 0))
        if not result['valid']:
            return result
        if not state.x.view.valid:
            state.x.view = deepcopy(data.time)
        if not state.y.view.valid or (state.y.logarithmic and state.y.view.minimum <= 0):
            state.y.view = deepcopy(data.frequency)
        state.x.logarithmic = False
        io, p, d = i.get_io(), c.palette(), f.draw
        def mouse():
            return state.x.value((io.mouse_pos.x-f.top[0])/f.size[0]), state.y.value(1-(io.mouse_pos.y-f.top[1])/f.size[1])
        mx, my = mouse()
        if navigation:
            if f.hovered and i.is_mouse_clicked(2): state.dragging = True
            if not i.is_mouse_down(2): state.dragging = False
            before = deepcopy((state.x.view, state.y.view))
            if state.dragging:
                _navigate(state.x, -io.mouse_delta.x/f.size[0], mx, 1)
                _navigate(state.y, io.mouse_delta.y/f.size[1], my, 1)
            if f.hovered and io.mouse_wheel and not wheel_taken():
                _navigate(state.x, 0, mx, exp(-io.mouse_wheel*.15))
                if io.key_ctrl: _navigate(state.y, 0, my, exp(-io.mouse_wheel*.15))
                take_wheel()
            result['view_changed'] = before != (state.x.view, state.y.view)
        else:
            state.dragging = False
        result['mouse'] = mouse()
        time, frequency = Axis(data.time), Axis(data.frequency, data.logarithmic_bins)
        def index(fraction, count):
            return min(count-1, int(max(0., min(1., fraction))*count))
        budget = max(1, min(262144, int(max_cells)))
        nx = max(1, min(data.frames, budget, 4096, ceil(f.size[0])))
        ny = max(1, min(data.bins, budget//nx, 4096, ceil(f.size[1])))
        for y in range(ny):
            fy = frequency.fraction(state.y.value(1-(y+.5)/ny))
            if not isfinite(fy) or not 0 <= fy <= 1: continue
            row = index(fy, data.bins)
            for x in range(nx):
                fx = time.fraction(state.x.value((x+.5)/nx))
                if not isfinite(fx) or not 0 <= fx <= 1: continue
                value = data.values[index(fx, data.frames)*data.bins+row]
                if not isfinite(value): continue
                t = _fraction(value, intensity)
                color = c.mix(p.bg0, p.info, t*2) if t < .5 else (c.mix(p.info, p.accent, (t-.5)/.3) if t < .8 else c.mix(p.accent, p.warning, (t-.8)/.2))
                d.add_rect_filled((f.top[0]+f.size[0]*x/nx, f.top[1]+f.size[1]*y/ny), (f.top[0]+f.size[0]*(x+1)/nx, f.top[1]+f.size[1]*(y+1)/ny), color)
        if axes:
            for n in range(5):
                _label(d, (f.top[0]+c.dp(4), max(f.top[1]+c.dp(3), f.bottom[1]-f.size[1]*n/4-i.get_font_size()-c.dp(3))), _hz(state.y.value(n/4)), p.textDim)
            _label(d, (f.top[0]+c.dp(65), f.bottom[1]-i.get_font_size()-c.dp(3)), _number(state.x.view.minimum, 's')+' / '+_number(state.x.view.maximum, 's'), p.textDim)
            _label(d, (f.top[0]+c.dp(65), f.top[1]+c.dp(3)), _number(intensity.minimum, unit)+' / '+_number(intensity.maximum, unit), p.textDim)
        fx, fy = time.fraction(result['mouse'][0]), frequency.fraction(result['mouse'][1])
        result['hovered'] = f.hovered and isfinite(fx) and isfinite(fy) and 0 <= fx <= 1 and 0 <= fy <= 1
        if result['hovered']:
            result.update(frame=index(fx, data.frames), bin=index(fy, data.bins), clicked=i.is_mouse_clicked(0))
            result['intensity'] = data.values[result['frame']*data.bins+result['bin']]
            d.add_line((io.mouse_pos.x, f.top[1]), (io.mouse_pos.x, f.bottom[1]), c.alpha(p.text, .4), c.dp(1))
            d.add_line((f.top[0], io.mouse_pos.y), (f.bottom[0], io.mouse_pos.y), c.alpha(p.text, .4), c.dp(1))
            i.set_tooltip(f"{_number(result['mouse'][0], 's')} | {_hz(result['mouse'][1])} | {_number(result['intensity'], unit)}")
    return result


@dataclass
class LoudnessData:
    momentary: float = nan
    short_term: float = nan
    integrated: float = nan
    range_low: float = nan
    range_high: float = nan
    history: object = field(default_factory=list)


def loudness(identifier, state, data, size=(0, 240), *, levels=None, target=-23., reset_button=True):
    i, c = ui()
    levels = levels if levels and levels.valid else Range(-60, 0)
    height = _height(size)
    i.push_id(identifier); i.begin_group()
    reset = reset_button and i.small_button(c.tr_label('Reset measurements###reset'))
    values = [data.momentary, data.short_term, data.integrated,
              data.range_high-data.range_low if isfinite(data.range_low) and isfinite(data.range_high) and data.range_high >= data.range_low else nan]
    with Canvas('readings', CanvasState(), (size[0], c.dp(66)), False, False) as f:
        p, d = c.palette(), f.draw
        for n, label in enumerate(('Momentary', 'Short-term', 'Integrated', 'Loudness range')):
            left, right = f.top[0]+f.size[0]*n/4+c.dp(6), f.top[0]+f.size[0]*(n+1)/4-c.dp(6)
            d.push_clip_rect((left, f.top[1]), (max(left, right), f.bottom[1]), True)
            _label(d, (left, f.top[1]+c.dp(5)), c.tr(label), p.textDim)
            _label(d, (left, f.top[1]+c.dp(23)), _number(values[n], 'LU' if n == 3 else 'LUFS'), p.text)
            top, bottom = (left, f.top[1]+c.dp(48)), (max(left, right), f.top[1]+c.dp(57))
            _bar(d, top, bottom, values[n] if n != 3 else nan, levels, p.info, target if n != 3 else nan)
            if n == 3 and isfinite(values[n]):
                a, b = left+(right-left)*_fraction(data.range_low, levels), left+(right-left)*_fraction(data.range_high, levels)
                d.add_rect_filled((a, top[1]), (b, bottom[1]), p.accent)
            d.pop_clip_rect()
    state.y.view = levels; state.x.logarithmic = state.y.logarithmic = False
    header = i.get_frame_height_with_spacing() if reset_button else 0
    plot('history', state, data.history, (size[0], max(c.dp(40), height-header-c.dp(66)-i.get_style().item_spacing.y)), format_x=lambda v: _number(v, 's'), format_y=lambda v: _number(v, 'LUFS'))
    i.end_group(); i.pop_id()
    return dict(reset_requested=bool(reset), clipped_channels=0)


@dataclass
class TruePeakChannel:
    label: str = ''
    level: float = nan
    held: float = nan
    overs: int = 0


def true_peak(identifier, channels, size=(0, 160), *, levels=None, warning=-6., ceiling=0., reset_button=True):
    i, c = ui()
    levels = levels if levels and levels.valid else Range(-60, 6)
    height = _height(size)
    i.push_id(identifier); i.begin_group()
    reset = reset_button and i.small_button(c.tr_label('Reset measurements###reset'))
    if i.begin_child('channels', (size[0], max(c.dp(24), height-(i.get_frame_height_with_spacing() if reset_button else 0)))):
        count = min(len(channels), 2**31-1)
        row = max(c.dp(38), i.get_content_region_avail().y/max(1, count))
        spacing = i.get_style().item_spacing
        i.push_style_var(i.StyleVar_.item_spacing, (spacing.x, 0))
        clipper = i.ListClipper(); clipper.begin(count, row)
        while clipper.step():
            for n in range(clipper.display_start, clipper.display_end):
                i.push_id(str(n))
                with Canvas('level', CanvasState(), (0, row), False, False) as f:
                    p, d, v = c.palette(), f.draw, channels[n]
                    color = p.error if v.level > ceiling or v.overs else (p.warning if v.level > warning else p.success)
                    _label(d, (f.top[0]+c.dp(6), f.top[1]+c.dp(3)), f'{v.label}  {_number(v.level, "dBTP")}   {c.tr("Held")} {_number(v.held, "dBTP")}   {c.tr("Overs")} {v.overs}', color)
                    a, b = (f.top[0]+c.dp(6), f.top[1]+c.dp(23)), (max(f.top[0]+c.dp(6), f.bottom[0]-c.dp(6)), f.top[1]+c.dp(32))
                    _bar(d, a, b, v.level, levels, color, ceiling)
                    if isfinite(v.held) and levels.contains(v.held):
                        x = a[0]+(b[0]-a[0])*_fraction(v.held, levels)
                        d.add_line((x, a[1]-c.dp(2)), (x, b[1]+c.dp(2)), p.accent, c.dp(2))
                i.pop_id()
        i.pop_style_var()
        if not channels: i.text_disabled(c.tr('No channels'))
    i.end_child()
    i.end_group(); i.pop_id()
    return dict(reset_requested=bool(reset), clipped_channels=sum(v.level > ceiling or bool(v.overs) for v in channels))


def correlation(identifier, state, value, history=(), size=(0, 160)):
    i, c = ui()
    height = _height(size)
    i.push_id(identifier); i.begin_group()
    with Canvas('gauge', CanvasState(), (size[0], c.dp(48)), False, False) as f:
        p, d = c.palette(), f.draw
        _label(d, (f.top[0]+c.dp(6), f.top[1]+c.dp(3)), c.tr('Correlation')+'  '+_number(value), p.text)
        a, b = (f.top[0]+c.dp(6), f.top[1]+c.dp(26)), (f.bottom[0]-c.dp(6), f.top[1]+c.dp(35))
        centre = (a[0]+b[0])/2
        d.add_rect_filled(a, (centre, b[1]), c.alpha(p.error, .3))
        d.add_rect_filled((centre, a[1]), b, c.alpha(p.success, .3))
        d.add_line((centre, a[1]), (centre, b[1]), p.textDim, c.dp(1))
        if isfinite(value): d.add_circle_filled((a[0]+(b[0]-a[0])*_fraction(value, Range(-1, 1)), (a[1]+b[1])/2), c.dp(5), p.error if value < 0 else p.success)
        _label(d, (a[0], b[1]), '-1', p.textDim); _label(d, (b[0]-c.dp(16), b[1]), '+1', p.textDim)
    result = dict(hovered=False, mouse=(0, 0), skipped=[])
    if history:
        state.y = Axis(Range(-1, 1)); state.x.logarithmic = False
        result = plot('history', state, history, (size[0], max(c.dp(40), height-c.dp(48)-i.get_style().item_spacing.y)), format_x=lambda v: _number(v, 's'))
    i.end_group(); i.pop_id()
    return result


@dataclass
class ResponsePoint:
    id: int
    frequency: float = 1000.
    gain: float = 0.
    enabled: bool = True


@dataclass
class ResponseState:
    view: PlotState = field(default_factory=lambda: PlotState(Axis(Range(20, 20000), True), Axis(Range(-24, 24))))
    before: list = field(default_factory=list)
    preview: list = field(default_factory=list)
    active: int = 0
    revision: int = 0
    dragging: bool = False


def filter_response(identifier, state, gain, phase, points, size=(0, 240), *, frequency=None, gains=None, phase_range=None, read_only=False, revision=0, **plot_options):
    i, c = ui()
    frequency = frequency if frequency and frequency.valid and frequency.minimum > 0 else Range(20, 20000)
    gains = gains if gains and gains.valid else Range(-24, 24)
    phase_range = phase_range or Range(-180, 180)
    if not state.view.x.view.valid or state.view.x.view.minimum <= 0: state.view.x.view = deepcopy(frequency)
    if not state.view.y.view.valid: state.view.y.view = deepcopy(gains)
    state.view.x.logarithmic, state.view.y.logarithmic = True, False
    plot_options.setdefault('format_x', _hz); plot_options.setdefault('format_y', lambda v: _number(v, 'dB'))
    result = dict(plot=plot(identifier, state.view, gain, size, **plot_options), points=Edit())
    top, bottom = tuple(i.get_item_rect_min()), tuple(i.get_item_rect_max())
    width, height = bottom[0]-top[0], bottom[1]-top[1]
    d, io, p = i.get_window_draw_list(), i.get_io(), c.palette()
    d.push_clip_rect(top, bottom, True)
    def screen(hz, db): return top[0]+state.view.x.fraction(hz)*width, bottom[1]-state.view.y.fraction(db)*height
    valid = len(points) <= 1024 and len({p.id for p in points}) == len(points) and all(p.id and isfinite(p.frequency) and p.frequency > 0 and isfinite(p.gain) for p in points)
    if state.dragging and (read_only or not valid or points != state.before or revision != state.revision or not i.is_window_focused(i.FocusedFlags_.root_and_child_windows) or i.is_key_pressed(i.Key.escape)):
        result['points'] = Edit('cancel', deepcopy(state.before)); state.dragging = False; state.active = 0
    if phase_range.valid:
        axis = Axis(phase_range)
        for line in phase:
            if len(line.points) > plot_options.get('max_points', 65536): result['plot']['skipped'].append(line.id); continue
            previous = None
            for hz, value in line.points:
                x, y = state.view.x.fraction(hz), axis.fraction(value)
                if not isfinite(x) or not isfinite(y): previous = None; continue
                at = top[0]+x*width, bottom[1]-y*height
                if previous: d.add_line(previous, at, c.alpha(c.categorical(line.color_index), .65), c.dp(1))
                previous = at
        if phase and plot_options.get('legend', True):
            x, phase_label = top[0]+c.dp(70), c.tr('Phase')
            for line in phase:
                text = phase_label+(': '+line.label if line.label and line.label != phase_label else '')
                width_label = i.calc_text_size(text).x+c.dp(18)
                if x+width_label > bottom[0]-c.dp(70): break
                _label(d, (x, top[1]+i.get_font_size()+c.dp(7)), text, c.categorical(line.color_index))
                x += width_label
        if phase and plot_options.get('axes', True):
            for n in range(3):
                text = _number(axis.value(n/2), 'deg')
                _label(d, (bottom[0]-i.calc_text_size(text).x-c.dp(4), max(top[1]+c.dp(3), min(bottom[1]-i.get_font_size()-c.dp(3), bottom[1]-height*n/2))), text, p.info)
    visible = state.preview if state.dragging else points
    hit, closest = 0, c.dp(10)
    if valid:
        for point in visible:
            at = screen(point.frequency, point.gain)
            if not (top[0] <= at[0] <= bottom[0] and top[1] <= at[1] <= bottom[1]): continue
            d.add_circle_filled(at, c.dp(5), p.accent if point.enabled else p.bg5)
            d.add_circle(at, c.dp(5), p.text, 0, c.dp(1))
            distance = hypot(at[0]-io.mouse_pos.x, at[1]-io.mouse_pos.y)
            if distance < closest: hit, closest = point.id, distance
    if result['points'].phase == 'none' and not read_only and not state.dragging and result['plot']['hovered'] and hit and i.is_mouse_clicked(0):
        state.before, state.preview = deepcopy(points), deepcopy(points)
        state.active, state.revision, state.dragging = hit, revision, True
        result['points'] = Edit('begin', deepcopy(state.preview))
    elif state.dragging:
        if i.is_mouse_down(0):
            for point in state.preview:
                if point.id == state.active:
                    point.frequency = max(frequency.minimum, min(frequency.maximum, state.view.x.value(max(0., min(1., (io.mouse_pos.x-top[0])/width)))))
                    point.gain = max(gains.minimum, min(gains.maximum, state.view.y.value(max(0., min(1., 1-(io.mouse_pos.y-top[1])/height)))))
            result['points'] = Edit('update', deepcopy(state.preview))
        else:
            result['points'] = Edit('commit', deepcopy(state.preview)); state.dragging = False; state.active = 0
    if hit and result['plot']['hovered']:
        point = next(p for p in visible if p.id == hit)
        i.set_tooltip(f'{_hz(point.frequency)} | {_number(point.gain, "dB")}')
    d.pop_clip_rect()
    return result


@dataclass
class DynamicsData:
    transfer: object = field(default_factory=list)
    input: float = nan
    output: float = nan
    gain_reduction: float = nan


def dynamics(identifier, state, data, size=(0, 240), *, reduction=None, **plot_options):
    i, c = ui()
    reduction = reduction if reduction and reduction.valid else Range(0, 36)
    height = _height(size)
    i.push_id(identifier); i.begin_group()
    with Canvas('reduction', CanvasState(), (size[0], c.dp(43)), False, False) as f:
        _label(f.draw, (f.top[0]+c.dp(6), f.top[1]+c.dp(3)), c.tr('Gain reduction')+'  '+_number(data.gain_reduction, 'dB'), c.palette().text)
        _bar(f.draw, (f.top[0]+c.dp(6), f.top[1]+c.dp(26)), (f.bottom[0]-c.dp(6), f.top[1]+c.dp(35)), data.gain_reduction, reduction, c.palette().warning)
    state.x.logarithmic = state.y.logarithmic = False
    plot_options.setdefault('format_x', lambda v: _number(v, c.tr('dB in'))); plot_options.setdefault('format_y', lambda v: _number(v, c.tr('dB out')))
    result = plot('transfer', state, data.transfer, (size[0], max(c.dp(40), height-c.dp(43)-i.get_style().item_spacing.y)), **plot_options)
    top, bottom = tuple(i.get_item_rect_min()), tuple(i.get_item_rect_max())
    d = i.get_window_draw_list(); d.push_clip_rect(top, bottom, True)
    if state.x.view.valid and state.y.view.valid:
        def at(x, y): return top[0]+state.x.fraction(x)*(bottom[0]-top[0]), bottom[1]-state.y.fraction(y)*(bottom[1]-top[1])
        lo, hi = max(state.x.view.minimum, state.y.view.minimum), min(state.x.view.maximum, state.y.view.maximum)
        if lo <= hi: d.add_line(at(lo, lo), at(hi, hi), c.palette().lineStrong, c.dp(1))
        if isfinite(data.input) and isfinite(data.output) and state.x.view.contains(data.input) and state.y.view.contains(data.output):
            d.add_circle_filled(at(data.input, data.output), c.dp(5), c.palette().accent)
    d.pop_clip_rect(); i.end_group(); i.pop_id()
    return result
