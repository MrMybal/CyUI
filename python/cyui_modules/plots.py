from dataclasses import dataclass, field
from math import log, exp, isfinite, nan
from .common import Range, ui, zoom_range, take_wheel, wheel_taken
from .canvas import Canvas, CanvasState


@dataclass
class ColorRamp:
    style: str = 'themed'  # themed, intensity, frequency, monochrome
    colors: object = field(default_factory=list)  # optional 1..64 palette colors


def sample_ramp(ramp, fraction):
    _, c = ui()
    p = c.palette(); t = max(0., min(1., fraction)) if isfinite(fraction) else 0.
    colors = ramp.colors
    if not 1 <= len(colors) <= 64:
        if ramp.style == 'monochrome': return c.mix(p.bg0, p.text, t)
        if ramp.style == 'themed':
            return c.mix(p.bg0, p.info, t*2) if t < .5 else (c.mix(p.info, p.accent, (t-.5)/.3) if t < .8 else c.mix(p.accent, p.warning, (t-.8)/.2))
        colors = [p.error, p.warning, p.success, p.info, p.codeKeyword, p.codeType] if ramp.style == 'frequency' else [p.bg0, p.codeType, p.codeKeyword, p.info, p.success, p.warning, p.error]
    at = t*(len(colors)-1); a = int(at)
    return c.mix(colors[a], colors[min(a+1, len(colors)-1)], at-a)


def draw_color_scale(top, bottom, ramp, values, unit='', vertical=True, logarithmic=False):
    i, c = ui(); d = i.get_window_draw_list()
    if not values.valid or bottom[0] <= top[0] or bottom[1] <= top[1]: return
    for n in range(128):
        a, b = n/128, (n+1)/128
        lo = (top[0], bottom[1]-(bottom[1]-top[1])*b) if vertical else (top[0]+(bottom[0]-top[0])*a, top[1])
        hi = (bottom[0], bottom[1]-(bottom[1]-top[1])*a) if vertical else (top[0]+(bottom[0]-top[0])*b, bottom[1])
        d.add_rect_filled(lo, hi, sample_ramp(ramp, (a+b)/2))
    for n in range(3):
        t = n/2
        value = exp(log(values.minimum)+t*(log(values.maximum)-log(values.minimum))) if logarithmic and values.minimum > 0 else values.minimum+t*values.span
        text = f'{value:.3g} {unit}'
        at = (bottom[0]+c.dp(4), max(top[1], min(max(top[1], bottom[1]-i.get_font_size()), bottom[1]-t*(bottom[1]-top[1])-i.get_font_size()/2))) if vertical else (max(top[0], min(bottom[0]-i.calc_text_size(text).x, top[0]+t*(bottom[0]-top[0]))), bottom[1]+c.dp(3))
        d.add_text(at, c.palette().textDim, text)


def _quad(d, points, colors):
    i, _ = ui(); uv = i.get_font_tex_uv_white_pixel()
    d.prim_reserve(6, 4); index = d._vtx_current_idx
    for offset in (0, 1, 2, 0, 2, 3): d.prim_write_idx(index+offset)
    for point, color in zip(points, colors): d.prim_write_vtx(point, uv, color)


@dataclass
class Axis:
    view: Range = field(default_factory=Range)
    logarithmic: bool = False

    def fraction(self, value):
        if not self.view.valid or not isfinite(value):
            return nan
        if self.logarithmic:
            return log(value / self.view.minimum) / log(self.view.maximum / self.view.minimum) if value > 0 and self.view.minimum > 0 else nan
        return (value - self.view.minimum) / self.view.span

    def value(self, fraction):
        try:
            return self.view.minimum * (self.view.maximum / self.view.minimum) ** fraction if self.logarithmic and self.view.minimum > 0 else self.view.minimum + fraction * self.view.span
        except (OverflowError, ZeroDivisionError):
            return nan


def _navigate(axis, shift, anchor, factor):
    """Pan by shift (a fraction of the view) and zoom by factor around anchor, like C++.

    A log axis pans and zooms in log space only while its range is positive; otherwise it
    moves like a linear one. A result that is not a finite, non-empty range is rejected.
    """
    view = axis.view
    try:
        if axis.logarithmic and view.minimum > 0 and view.maximum > 0:
            low, high = log(view.minimum), log(view.maximum)
            delta = shift * (high - low)
            moved = Range(low + delta, high + delta)
            if anchor > 0 and isfinite(anchor):
                moved = zoom_range(moved, log(anchor), factor)
            if not (abs(moved.minimum) <= 700 and abs(moved.maximum) <= 700):
                return
            result = Range(exp(moved.minimum), exp(moved.maximum))
        else:
            delta = shift * view.span
            result = zoom_range(Range(view.minimum + delta, view.maximum + delta), anchor, factor)
    except (OverflowError, ValueError, ZeroDivisionError):
        return
    if result.valid and (not axis.logarithmic or view.minimum <= 0 or result.minimum > 0):
        axis.view = result


@dataclass
class PlotState:
    x: Axis = field(default_factory=Axis)
    y: Axis = field(default_factory=Axis)
    dragging: bool = False


@dataclass
class Series:
    id: int
    points: object
    label: str = ''
    style: str = 'line'
    color_index: int = 0
    baseline: float = 0.
    fill_alpha: float = 1.
    gradient: bool = False
    color_by_x: bool = False
    ramp: ColorRamp = field(default_factory=ColorRamp)
    bands: object = field(default_factory=list)


def reduce_plot(points, budget):
    if budget < 4 or not len(points):
        return []
    if len(points) <= budget:
        return list(points)
    buckets = max(1, (budget - 2) // 3)
    out = [points[0]]
    for b in range(buckets):
        a, end = 1 + b * (len(points) - 2) // buckets, 1 + (b + 1) * (len(points) - 2) // buckets
        if any(not all(map(isfinite, p)) for p in points[a:end]):
            out.append((points[a][0], nan))
            continue
        lo = min(range(a, end), key=lambda k: points[k][1])
        hi = max(range(a, end), key=lambda k: points[k][1])
        out.append(points[min(lo, hi)])
        if lo != hi:
            out.append(points[max(lo, hi)])
    return out + [points[-1]]


def histogram(values, view, bins):
    if not view.valid or not 0 < bins <= 1000000:
        return []
    out = [0] * bins
    for value in values:
        if isfinite(value) and view.contains(value):
            out[min(bins - 1, int((value - view.minimum) / view.span * bins))] += 1
    return out


def plot(identifier, state, series, size=(0, 240), navigation=True, max_points=65536, axes=True, legend=True, format_x=None, format_y=None, draw_background=None):
    i, c = ui()
    with Canvas(identifier, CanvasState(), size, grid=False, navigation=False) as frame:
        p, draw, io = c.palette(), frame.draw, i.get_io()
        mouse = (state.x.value((io.mouse_pos.x - frame.top[0]) / frame.size[0]), state.y.value(1 - (io.mouse_pos.y - frame.top[1]) / frame.size[1]))
        if navigation:
            if frame.hovered and i.is_mouse_clicked(2):
                state.dragging = True
            if not i.is_mouse_down(2):
                state.dragging = False
            if state.dragging:
                _navigate(state.x, -io.mouse_delta.x / frame.size[0], mouse[0], 1)
                _navigate(state.y, io.mouse_delta.y / frame.size[1], mouse[1], 1)
            if frame.hovered and io.mouse_wheel and not wheel_taken():
                for axis, anchor in ((state.x, mouse[0]), (state.y, mouse[1])) if io.key_ctrl else ((state.x, mouse[0]),):
                    _navigate(axis, 0, anchor, exp(-io.mouse_wheel * .15))
                take_wheel()
        if draw_background: draw_background(frame, state)
        for n in range(1, 4):
            x, y = frame.top[0] + frame.size[0] * n / 4, frame.top[1] + frame.size[1] * n / 4
            draw.add_line((x, frame.top[1]), (x, frame.bottom[1]), p.grid, c.dp(1))
            draw.add_line((frame.top[0], y), (frame.bottom[0], y), p.grid, c.dp(1))
        skipped = []
        for line in series:
            if len(line.points) > max_points:
                skipped.append(line.id)
                continue
            previous, previous_fraction = None, 0
            color = c.categorical(line.color_index)
            for index, (x, y) in enumerate(line.points):
                fx, fy = state.x.fraction(x), state.y.fraction(y)
                if not isfinite(fx) or not isfinite(fy):
                    previous = None
                    continue
                q = (frame.top[0] + fx * frame.size[0], frame.bottom[1] - fy * frame.size[1])
                if line.style == 'points':
                    draw.add_circle_filled(q, c.dp(2.5), color)
                elif line.style == 'bars':
                    width = max(c.dp(1), frame.size[0] / max(1, len(line.points)) * .8)
                    fraction = state.y.fraction(line.baseline)
                    base = frame.bottom[1]-max(0., min(1., fraction))*frame.size[1] if isfinite(fraction) else frame.bottom[1]
                    left, right = q[0]-width/2, q[0]+width/2
                    if len(line.bands):
                        if len(line.bands) != len(line.points) or not line.bands[index].valid: continue
                        a, b = state.x.fraction(line.bands[index].minimum), state.x.fraction(line.bands[index].maximum)
                        if not isfinite(a) or not isfinite(b): continue
                        left, right = frame.top[0]+a*frame.size[0], frame.top[0]+b*frame.size[0]
                    fill = sample_ramp(line.ramp, fx) if line.color_by_x else color
                    high = c.alpha(fill, max(0., min(1., line.fill_alpha))); low = c.alpha(fill, 0) if line.gradient else high
                    upper, lower = (high, low) if q[1] <= base else (low, high)
                    draw.add_rect_filled_multi_color((left, min(base, q[1])), (right, max(base, q[1])), upper, upper, lower, lower)
                elif previous:
                    if line.style == 'step':
                        draw.add_line(previous, (q[0], previous[1]), color, c.dp(1.5))
                        draw.add_line((q[0], previous[1]), q, color, c.dp(1.5))
                    else:
                        first = sample_ramp(line.ramp, previous_fraction) if line.color_by_x else color
                        last = sample_ramp(line.ramp, fx) if line.color_by_x else color
                        if line.style == 'area':
                            fraction = state.y.fraction(line.baseline)
                            base = frame.bottom[1]-max(0., min(1., fraction))*frame.size[1] if isfinite(fraction) else frame.bottom[1]
                            opacity = max(0., min(1., line.fill_alpha))
                            _quad(draw, (previous, q, (q[0], base), (previous[0], base)), (c.alpha(first, opacity), c.alpha(last, opacity), c.alpha(last, 0 if line.gradient else opacity), c.alpha(first, 0 if line.gradient else opacity)))
                        draw.add_line(previous, q, last, c.dp(1.5))
                previous = q
                previous_fraction = fx
        def axis_label(position, text):
            extent = i.calc_text_size(text)
            draw.add_rect_filled((position[0]-c.dp(2), position[1]), (position[0]+extent.x+c.dp(2), position[1]+extent.y), c.alpha(p.bg0, .85))
            draw.add_text(position, p.textDim, text)
        if axes:
            for tick in range(5):
                part = tick/4
                text = format_x(state.x.value(part)) if format_x else f'{state.x.value(part):.4g}'
                width = i.calc_text_size(text).x
                axis_label((max(frame.top[0]+c.dp(3), min(frame.bottom[0]-width-c.dp(3), frame.top[0]+frame.size[0]*part-width*.5)), frame.bottom[1]-i.get_font_size()-c.dp(3)), text)
                if tick:
                    text = format_y(state.y.value(part)) if format_y else f'{state.y.value(part):.4g}'
                    axis_label((frame.top[0]+c.dp(4), max(frame.top[1]+c.dp(3), min(frame.bottom[1]-i.get_font_size()-c.dp(3), frame.bottom[1]-frame.size[1]*part-i.get_font_size()*.5))), text)
        if legend:
            x = frame.top[0]+c.dp(70)
            for line in series:
                if not line.label:
                    continue
                width = i.calc_text_size(line.label).x+c.dp(24)
                if x+width > frame.bottom[0]:
                    break
                y = frame.top[1]+c.dp(5)
                draw.add_rect_filled((x, y), (x+width, y+i.get_font_size()), c.alpha(p.bg0, .85))
                draw.add_circle_filled((x+c.dp(4), y+i.get_font_size()*.5), c.dp(3), c.categorical(line.color_index))
                draw.add_text((x+c.dp(12), y), p.text, line.label)
                x += width
        if frame.hovered:
            draw.add_line((io.mouse_pos.x, frame.top[1]), (io.mouse_pos.x, frame.bottom[1]), p.textFaint, c.dp(1))
            i.set_tooltip(f'{mouse[0]:.6g}, {mouse[1]:.6g}')
        return {'hovered': frame.hovered, 'mouse': mouse, 'skipped': skipped}


def heatmap(identifier, values, columns, rows, view, size=(0, 180)):
    i, c = ui()
    with Canvas(identifier, CanvasState(), size, grid=False, navigation=False) as frame:
        if columns <= 0 or rows <= 0 or len(values) < columns * rows or not view.valid:
            return None
        for y in range(0, rows, max(1, (rows+255) // 256)):
            for x in range(0, columns, max(1, (columns+255) // 256)):
                value = values[y * columns + x]
                color = c.mix(c.palette().bg0, c.palette().accent, max(0, min(1, (value - view.minimum) / view.span))) if isfinite(value) else c.palette().bg2
                frame.draw.add_rect_filled((frame.top[0] + x / columns * frame.size[0], frame.top[1] + y / rows * frame.size[1]), (frame.top[0] + min(columns, x + max(1, (columns+255) // 256)) / columns * frame.size[0], frame.top[1] + min(rows, y + max(1, (rows+255) // 256)) / rows * frame.size[1]), color)
        if frame.hovered:
            mouse = i.get_io().mouse_pos
            x = max(0, min(columns - 1, int((mouse.x - frame.top[0]) / frame.size[0] * columns)))
            y = max(0, min(rows - 1, int((mouse.y - frame.top[1]) / frame.size[1] * rows)))
            i.set_tooltip(f'[{x}, {y}] {values[y * columns + x]}')
            return x, y, i.is_mouse_clicked(0)
