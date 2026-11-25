from dataclasses import dataclass, field
from copy import deepcopy
from math import hypot, cos, sin, isfinite
from .common import Edit, ui
from .canvas import Canvas, CanvasState


@dataclass
class ImageData:
    texture: object = 0
    width: int = 0
    height: int = 0
    uv_min: tuple = (0, 0)
    uv_max: tuple = (1, 1)


@dataclass
class ImageState:
    canvas: CanvasState = field(default_factory=CanvasState)
    fit_pending: bool = True
    crop: object = None
    before: object = None
    start: object = None
    zoom_request: object = None


def _apply_view(state, extent, top, size, padding, fit=False):
    view = state.canvas.view
    if state.fit_pending or fit:
        view.fit((0, 0), extent, size, padding)
        state.fit_pending = False
    if state.zoom_request is not None:
        zoom, state.zoom_request = state.zoom_request, None
        if isfinite(zoom) and zoom > 0:
            view.zoom_at(tuple(top[n] + size[n] / 2 for n in (0, 1)), max(.05, min(64., zoom)), top)


def _draw_image(image, origin, clip, view, draw):
    i, c = ui()
    if image.width <= 0 or image.height <= 0:
        return
    a, b = view.screen((0, 0), origin), view.screen((image.width, image.height), origin)
    tile, p = c.dp(16), c.palette()
    low, high = tuple(max(a[n], clip[0][n]) for n in (0, 1)), tuple(min(b[n], clip[1][n]) for n in (0, 1))
    if any(high[n] <= low[n] for n in (0, 1)):
        return
    x0, y = (low[n] - (low[n] - a[n]) % tile for n in (0, 1))
    while y < high[1]:
        x = x0
        while x < high[0]:
            colour = p.bg2 if (round((x-a[0])/tile)+round((y-a[1])/tile)) % 2 else p.bg3
            draw.add_rect_filled((max(x, low[0]), max(y, low[1])), (min(x+tile, high[0]), min(y+tile, high[1])), colour)
            x += tile
        y += tile
    if image.texture:
        texture = image.texture if isinstance(image.texture, i.ImTextureRef) else i.ImTextureRef(image.texture)
        draw.add_image(texture, a, b, image.uv_min, image.uv_max)


def image_viewport(identifier, state, image, size=(0, 280), tool='inspect'):
    i, c = ui()
    result = {'pixel': None, 'clicked': False, 'crop': Edit()}
    left_pan = tool == 'inspect' and image.width > 0 and image.height > 0
    with Canvas(identifier, state.canvas, size, False, left_pan=left_pan) as f:
        if state.start is not None and (tool != 'crop' or state.canvas.panning or image.width <= 0 or image.height <= 0):
            state.crop, state.start = state.before, None
            result['crop'] = Edit('cancel', state.crop)
        if image.width <= 0 or image.height <= 0:
            return result
        _apply_view(state, (image.width, image.height), f.top, f.size, c.dp(12),
                    f.hovered and not state.canvas.panning and i.is_mouse_double_clicked(0))
        view, draw, p = state.canvas.view, f.draw, c.palette()
        _draw_image(image, f.top, (f.top, f.bottom), view, draw)
        mouse = view.world(tuple(i.get_io().mouse_pos), f.top)
        if f.hovered and 0 <= mouse[0] < image.width and 0 <= mouse[1] < image.height:
            result['pixel'] = int(mouse[0]), int(mouse[1])
            result['clicked'] = f.left_clicked if left_pan else not state.canvas.panning and i.is_mouse_clicked(0)
        if tool == 'crop' and not state.canvas.panning:
            if result['clicked']:
                state.start, state.before = mouse, state.crop
                result['crop'] = Edit('begin', state.crop)
            if state.start:
                state.crop = tuple(tuple(max(0, min((image.width, image.height)[n], op(state.start[n], mouse[n]))) for n in (0, 1)) for op in (min, max))
                if i.is_key_pressed(i.Key.escape):
                    state.crop, state.start = state.before, None
                    result['crop'] = Edit('cancel')
                elif not i.is_mouse_down(0):
                    state.start, result['crop'] = None, Edit('commit', state.crop)
                elif result['crop'].phase != 'begin' and (i.get_io().mouse_delta.x or i.get_io().mouse_delta.y):
                    result['crop'] = Edit('update', state.crop)
        if state.crop:
            a, b = (view.screen(point, f.top) for point in state.crop)
            draw.add_rect_filled(a, b, p.selection)
            draw.add_rect(a, b, p.accent, 0., 0, c.dp(2))
    return result


def image_view_toolbar(identifier, state):
    i, c = ui()
    i.push_id(identifier)
    try:
        if c.button(c.tr_label('Fit')):
            state.fit_pending, state.zoom_request = True, None
        i.same_line()
        if c.button('100%##image-actual'):
            state.zoom_request = 1.
        i.same_line()
        if c.button('-##image-out'):
            state.zoom_request = state.canvas.view.zoom / 1.25
        i.same_line()
        if c.button('+##image-in'):
            state.zoom_request = state.canvas.view.zoom * 1.25
        i.same_line()
        i.text_unformatted(f'{state.canvas.view.zoom:.0%}')
    finally:
        i.pop_id()


@dataclass
class ImageAnnotation:
    id: int
    region: tuple
    color_index: int = 0
    label: str = ''


@dataclass
class ImageCompareState:
    view: ImageState = field(default_factory=ImageState)
    mode: str = 'wipe'
    divider: float = .5
    dragging_divider: bool = False
    previous_size: tuple = (0., 0.)


def image_compare(identifier, state, first, second, size=(0, 280), *,
                  labels=('Before', 'After'), annotations=(), selected=None, blink_seconds=.6):
    """Aligned textures in side-by-side, wipe and blink modes. Returns requests by annotation ID."""
    i, c = ui()
    result = {'selected': None, 'divider_changed': False}
    i.push_id(identifier)
    try:
        modes = ('side_by_side', 'wipe', 'blink')
        if state.mode not in modes:
            state.mode = 'wipe'
        i.set_next_item_width(c.dp(144))
        changed, mode = i.combo(c.tr_label('Comparison##image-mode'), modes.index(state.mode),
                                [c.tr(label) for label in ('Side by side', 'Wipe', 'Blink')])
        if changed:
            state.mode = modes[mode]
        image_view_toolbar('##view', state.view)
        paired, wiping = state.mode == 'side_by_side', state.mode == 'wipe'
        top = tuple(i.get_cursor_screen_pos())
        available = i.get_content_region_avail()
        width, height = (max(c.dp(1), size[n] if size[n] > 0 else size[n] + available[n]) for n in (0, 1))
        room = (width * .5 if paired else width, height)
        state.divider = max(0., min(1., state.divider)) if isfinite(state.divider) else .5
        io = i.get_io()
        mouse = tuple(io.mouse_pos)
        handle = wiping and abs(mouse[0] - (top[0] + width * state.divider)) <= c.dp(9)
        right = paired and mouse[0] >= top[0] + room[0]
        view = state.view.canvas.view
        if state.previous_size[0] and not state.view.fit_pending and isfinite(view.zoom) and view.zoom > 0:
            view.origin = tuple(view.origin[n] + (state.previous_size[n] - room[n]) / (2 * view.zoom) for n in (0, 1))
        state.previous_size = room
        with Canvas('##compare', state.view.canvas, (width, height), False,
                    left_pan=not handle and not state.dragging_divider, zoom_origin_offset=(room[0] if right else 0., 0.)) as canvas:
            if canvas.hovered and handle and i.is_mouse_clicked(0) and not state.view.canvas.panning:
                state.dragging_divider = True
            if not wiping or not i.is_mouse_down(0) or getattr(io, 'app_focus_lost', False):
                state.dragging_divider = False
            if state.dragging_divider:
                split = max(0., min(1., (mouse[0] - top[0]) / width))
                result['divider_changed'], state.divider = split != state.divider, split
            extent = max(first.width, second.width), max(first.height, second.height)
            if min(extent) <= 0:
                return result
            _apply_view(state.view, extent, top, room, c.dp(12),
                        canvas.hovered and not handle and i.is_mouse_double_clicked(0))
            p, draw = c.palette(), canvas.draw
            hovered = []

            def pane(image, origin, lo, hi, label):
                if hi[0] <= lo[0]:
                    return
                draw.push_clip_rect(lo, hi, True)
                try:
                    _draw_image(image, origin, (lo, hi), view, draw)
                    for mark in annotations:
                        a, b = (view.screen(point, origin) for point in mark.region)
                        if a[0] >= b[0] or a[1] >= b[1]:
                            continue
                        ink = p.accent if selected == mark.id else c.categorical(mark.color_index)
                        draw.add_rect(a, b, ink, 0., 0, c.dp(2) if selected == mark.id else c.dp(1))
                        if canvas.hovered and all(max(lo[n], a[n]) <= mouse[n] <= min(hi[n], b[n]) for n in (0, 1)):
                            hovered.append(((b[0]-a[0])*(b[1]-a[1]), mark))
                    if label:
                        draw.add_text((lo[0] + c.dp(8), lo[1] + c.dp(8)), p.text, c.tr(label))
                finally:
                    draw.pop_clip_rect()

            if wiping or paired:
                split = top[0] + width * (.5 if paired else state.divider)
                pane(first, top, top, (split, canvas.bottom[1]), labels[0])
                pane(second, (split, top[1]) if paired else top, (split, top[1]), canvas.bottom, labels[1])
                draw.add_line((split, top[1]), (split, canvas.bottom[1]), p.accent if wiping else p.lineStrong, c.dp(2))
                if wiping:
                    draw.add_circle_filled((split, top[1] + height / 2), c.dp(5), p.accent)
            else:
                period = max(.1, min(10., blink_seconds)) if isfinite(blink_seconds) else .6
                index = int(i.get_time() / period) % 2
                pane((first, second)[index], top, top, canvas.bottom, labels[index])
            if hovered and not state.dragging_divider:
                mark = min(hovered, key=lambda hit: hit[0])[1]
                if mark.label:
                    i.set_tooltip(mark.label)
                if canvas.left_clicked:
                    result['selected'] = mark.id
    finally:
        i.pop_id()
    return result


@dataclass
class GradientStop:
    id: int
    position: float
    color: tuple


def sample_gradient(stops, position, linear_light=False):
    if not stops:
        return 0., 0., 0., 0.
    ordered = sorted(stops, key=lambda x: x.position)
    left = ordered[0]
    for right in ordered[1:]:
        if position <= right.position and position >= left.position:
            k = (position-left.position)/(right.position-left.position) if right.position > left.position else 1.
            def blend(a, b):
                if not linear_light:
                    return a+(b-a)*k
                def decode(x):
                    return x/12.92 if x <= .04045 else ((x+.055)/1.055)**2.4
                value = decode(a)+(decode(b)-decode(a))*k
                return value*12.92 if value <= .0031308 else 1.055*value**(1/2.4)-.055
            return tuple(blend(left.color[n], right.color[n]) for n in range(3))+(left.color[3]+(right.color[3]-left.color[3])*k,)
        if position < left.position:
            return left.color
        left = right
    return left.color


@dataclass
class GradientState:
    selected: int = 0
    dragging: int = 0
    before: list = field(default_factory=list)
    preview: list = field(default_factory=list)
    revision: int = 0


def gradient_editor(identifier, state, stops, size=(0, 48), linear_light=False, revision=0):
    i, c = ui()
    edit = Edit()
    with Canvas(identifier, CanvasState(), size, False, False) as f:
        io, p, d = i.get_io(), c.palette(), f.draw
        if state.dragging and (state.revision != revision or i.is_key_pressed(i.Key.escape)):
            state.dragging, edit = 0, Edit('cancel')
        shown = state.preview if state.dragging else stops
        bottom, count = f.bottom[1]-c.dp(14), max(1, min(4096, int(f.size[0]/c.dp(2))))
        for n in range(count):
            color = i.color_convert_float4_to_u32(sample_gradient(shown, (n+.5)/count, linear_light))
            d.add_rect_filled((f.top[0]+f.size[0]*n/count, f.top[1]), (f.top[0]+f.size[0]*(n+1)/count, bottom), color)
        hit = 0
        for stop in shown:
            x = f.top[0]+f.size[0]*stop.position
            a, b, e = (x, bottom), (x-c.dp(6), bottom+c.dp(10)), (x+c.dp(6), bottom+c.dp(10))
            d.add_triangle_filled(a, b, e, i.color_convert_float4_to_u32(stop.color))
            d.add_triangle(a, b, e, p.accent if state.selected == stop.id else p.text, c.dp(1.5))
            if abs(io.mouse_pos.x-x) < c.dp(8):
                hit = stop.id
        mouse = max(0., min(1., (io.mouse_pos.x-f.top[0])/f.size[0]))
        if f.hovered and i.is_mouse_double_clicked(0) and not hit:
            new_id = max([s.id for s in stops]+[0])+1
            edit = Edit('commit', deepcopy(stops)+[GradientStop(new_id, mouse, sample_gradient(stops, mouse, linear_light))])
            state.selected = new_id
        elif f.hovered and i.is_mouse_clicked(0) and hit:
            state.selected, state.dragging, state.revision = hit, hit, revision
            state.before, state.preview = deepcopy(stops), deepcopy(stops)
            edit = Edit('begin', deepcopy(state.preview))
        if state.dragging:
            if i.is_mouse_down(0):
                for stop in state.preview:
                    if stop.id == state.dragging:
                        stop.position = mouse
                if edit.phase != 'begin' and io.mouse_delta.x:
                    edit = Edit('update', deepcopy(state.preview))
            else:
                state.dragging, edit = 0, Edit('commit', deepcopy(state.preview))
        if f.hovered and hit and i.is_mouse_clicked(1) and len(stops) > 2:
            edit = Edit('commit', [deepcopy(s) for s in stops if s.id != hit])
    return edit
