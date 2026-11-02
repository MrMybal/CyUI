"""Audio controls use the same normalized ranges, modifiers and dB meter mapping as C++."""
from dataclasses import dataclass, field
from math import log, log10, exp, sin, cos, pi, isfinite
from .common import ui, snap, take_wheel


def meter_position(db):
    return 0. if db <= -70 else max(0., min(1., exp((db-6)/26)))


def _arc(draw, center, radius, a, b, color, thickness):
    if abs(b-a) > .001:
        draw.path_arc_to(center, radius, min(a, b), max(a, b), 32)
        draw.path_stroke(color, thickness=thickness)


def knob(identifier, value, minimum, maximum, default, diameter=34., label='', logarithmic=False, bipolar=False, integer=False, steps=0, modulation=None):
    i, c = ui()
    p, io = c.palette(), i.get_io()
    i.push_id(identifier)
    origin = i.get_cursor_screen_pos()
    width, height = c.dp(diameter+12), c.dp(diameter)
    i.invisible_button('##knob', (width, height+i.get_font_size()))
    hovered, active = i.is_item_hovered(), i.is_item_active()
    logarithmic = logarithmic and minimum > 0
    def fraction(v):
        return max(0., min(1., log(max(v, minimum)/minimum)/log(maximum/minimum) if logarithmic else (v-minimum)/max(1e-12, maximum-minimum)))
    t, changed = fraction(value), False
    speed = .15 if io.key_ctrl else 4. if io.key_shift else 1.
    if active and io.mouse_delta.y:
        t = max(0., min(1., t-io.mouse_delta.y*.0055*speed/max(.5, i.get_style().font_scale_dpi)))
        changed = True
    if hovered and io.mouse_wheel:
        t = max(0., min(1., t+io.mouse_wheel*(1/steps if steps else .02*speed)))
        take_wheel()
        changed = True
    if hovered and i.is_mouse_double_clicked(0):
        t, changed = fraction(default), True
    if changed:
        if steps:
            t = snap(t, 1/steps)
        value = minimum*(maximum/minimum)**t if logarithmic else minimum+(maximum-minimum)*t
        if integer:
            value = snap(value, 1)
        value = max(minimum, min(maximum, value))
        t = fraction(value)
    d, center, radius = i.get_window_draw_list(), (origin.x+width/2, origin.y+height/2), height/2-c.dp(2)
    start, sweep, thickness = pi*.75, pi*1.5, max(c.dp(2), height*.095)
    _arc(d, center, radius, start, start+sweep, p.bg5, thickness)
    _arc(d, center, radius, start+sweep*.5 if bipolar else start, start+sweep*t, p.accent, thickness)
    angle = start+sweep*t
    d.add_line(center, (center[0]+cos(angle)*radius*.75, center[1]+sin(angle)*radius*.75), p.text, c.dp(1.5))
    if modulation is not None:
        angle = start+sweep*max(0, min(1, modulation))
        d.add_circle_filled((center[0]+cos(angle)*(radius+c.dp(3)), center[1]+sin(angle)*(radius+c.dp(3))), c.dp(2), p.info)
    text = f'{value:.3g}' if hovered or active else label
    d.add_text((origin.x, origin.y+height), p.text if hovered else p.textDim, text)
    i.pop_id()
    return changed, value


def fader(identifier, value, minimum=-70., maximum=6., size=(30, 150)):
    i, c = ui()
    p, io = c.palette(), i.get_io()
    size = tuple(c.dp(x) for x in size)
    origin = i.get_cursor_screen_pos()
    i.invisible_button(identifier, size)
    changed, travel = False, max(c.dp(1), size[1]-c.dp(14))
    speed = .15 if io.key_ctrl else 4. if io.key_shift else 1.
    if i.is_item_active() and io.mouse_delta.y:
        part = max(0., min(1., meter_position(value)-io.mouse_delta.y/travel*speed))
        value, changed = minimum if part <= 0 else max(minimum, min(maximum, 6+26*log(part))), True
    if i.is_item_hovered() and io.mouse_wheel:
        value, changed = max(minimum, min(maximum, value+io.mouse_wheel*speed)), True
        take_wheel()
    if i.is_item_hovered() and i.is_mouse_double_clicked(0):
        value, changed = max(minimum, min(maximum, 0)), True
    d, center = i.get_window_draw_list(), origin.x+size[0]/2
    d.add_rect_filled((center-c.dp(2), origin.y), (center+c.dp(2), origin.y+size[1]), p.bg0)
    y = origin.y+c.dp(7)+travel*(1-meter_position(value))
    d.add_rect_filled((origin.x, y-c.dp(4)), (origin.x+size[0], y+c.dp(4)), p.accent, c.dp(2))
    return changed, value


@dataclass
class MeterState:
    hold: list = field(default_factory=lambda: [-120., -120.])
    time: float = 0.


def meter(identifier, peaks, rms, size=(16, 150), vertical=True, state=None):
    i, c = ui()
    size = tuple(c.dp(x) for x in size)
    origin = i.get_cursor_screen_pos()
    i.invisible_button(identifier, size)
    d, p = i.get_window_draw_list(), c.palette()
    channels = max(1, min(2, len(peaks)))
    if state is None:
        state = MeterState()
    elapsed = max(0, i.get_time()-state.time)
    state.time = i.get_time()
    d.add_rect_filled(origin, (origin.x+size[0], origin.y+size[1]), p.bg0)
    for channel in range(channels):
        gain = peaks[channel]
        db = max(-120., 20*log10(gain)) if gain > 0 else -120.
        state.hold[channel] = max(db, state.hold[channel]-elapsed*18)
        for value, alpha in ((gain, .45), (rms[channel] if channel < len(rms) else gain, 1.)):
            part = meter_position(20*log10(value)) if value > 0 else 0.
            color = c.alpha(p.error if db >= 0 else p.warning if db > -3 else p.success, alpha)
            if vertical:
                left, right = origin.x+size[0]*channel/channels, origin.x+size[0]*(channel+1)/channels-c.dp(1)
                d.add_rect_filled((left, origin.y+size[1]*(1-part)), (right, origin.y+size[1]), color)
            else:
                top, bottom = origin.y+size[1]*channel/channels, origin.y+size[1]*(channel+1)/channels
                d.add_rect_filled((origin.x, top), (origin.x+size[0]*part, bottom), color)
