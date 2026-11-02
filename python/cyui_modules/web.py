"""Web panel contract, strict origins and CSS theme messages. No browser on import."""
from dataclasses import dataclass
from urllib.parse import urlsplit
import json
import re
from .common import ui


def web_origin(url):
    if len(url) > 8192 or any(ord(c) < 33 or ord(c) == 127 or c == '\\' for c in url):
        return ''
    try:
        parts = urlsplit(url)
        if parts.scheme not in ('http', 'https') or not parts.netloc or parts.username is not None or parts.password is not None or '%' in parts.netloc or any(ord(c) >= 128 for c in parts.netloc):
            return ''
        host = parts.hostname or ''
        if ':' in host:
            if not re.fullmatch(r'[a-fA-F0-9:]+', host):
                return ''
            host = '['+host+']'
        elif not re.fullmatch(r'[a-zA-Z0-9.\-]+', host):
            return ''
        port = parts.port
        if port == 0 or parts.netloc.endswith(':'):
            return ''
        return parts.scheme+'://'+host.lower()+(f':{port}' if port and port != (443 if parts.scheme == 'https' else 80) else '')
    except ValueError:
        return ''


@dataclass
class WebPolicy:
    origins: tuple = ()
    max_message_bytes: int = 1024*1024
    max_queued_events: int = 256

    def allows(self, url):
        origin = web_origin(url)
        return bool(origin) and any(web_origin(entry) == origin for entry in self.origins)


def theme_message():
    _, c = ui()
    p = c.palette()
    colors = {}
    for name in ('bg0', 'bg1', 'bg2', 'bg3', 'text', 'textDim', 'textFaint', 'accent', 'selection', 'line', 'success', 'warning', 'error', 'info'):
        value = getattr(p, name)
        colors[name] = f'rgba({value & 255},{value >> 8 & 255},{value >> 16 & 255},{(value >> 24 & 255)/255:.4f})'
    return {'protocol': 'cyui.web', 'version': 1, 'type': 'theme', 'payload': {'scale': c.dp(1), 'colors': colors}}


def web_panel(identifier, backend, size=(0, 320), visible=True, focus=False, viewport_origin=(0, 0)):
    """backend implements place(bounds, clip, visible, focus), poll(), ready().

    Call CEFRuntime.begin_frame()/end_frame(), including frames with hidden tabs.
    """
    i, c = ui()
    available = i.get_content_region_avail()
    extent = tuple(max(c.dp(1), size[n] if size[n] > 0 else available[n]+size[n]) for n in (0, 1))
    low = tuple(i.get_cursor_screen_pos())
    high = tuple(low[n]+extent[n] for n in (0, 1))
    i.invisible_button(identifier, extent)
    draw = i.get_window_draw_list()
    clip_low, clip_high = tuple(draw.get_clip_rect_min()), tuple(draw.get_clip_rect_max())
    bounds = tuple(tuple(point[n]-viewport_origin[n] for n in (0, 1)) for point in (low, high))
    clip = tuple(tuple(operation(point[n], bound[n])-viewport_origin[n] for n in (0, 1)) for operation, point, bound in ((max, low, clip_low), (min, high, clip_high)))
    draw.add_rect_filled(low, high, c.palette().bg0)
    if not backend.ready():
        c.centred_text(draw, i.ImVec2(*low), i.ImVec2(*high), c.palette().textDim, 'Web view loading...')
    backend.place(bounds, clip, visible and i.is_item_visible() and all(clip[1][n] > clip[0][n] for n in (0, 1)), focus)
    if hasattr(backend, 'uses_texture') and backend.uses_texture():
        texture = backend.texture()
        if texture:
            draw.add_image(i.ImTextureRef(texture), low, high)
        if visible and i.is_item_visible():
            from .common import take_wheel, wheel_taken
            io, hovered = i.get_io(), i.is_item_hovered()
            modifiers = int(io.key_shift) | (int(io.key_ctrl) << 1) | (int(io.key_alt) << 2) | (int(io.key_super) << 3) | (int(i.is_mouse_down(0)) << 4) | (int(i.is_mouse_down(1)) << 5) | (int(i.is_mouse_down(2)) << 6)
            args = dict(x=io.mouse_pos.x-low[0], y=io.mouse_pos.y-low[1], modifiers=modifiers)
            if i.is_mouse_clicked(0):
                backend.input('focus', down=hovered)
            backend.input('move', down=hovered, **args)
            for button in range(3):
                if (hovered or backend.focused()) and (i.is_mouse_clicked(button) or i.is_mouse_released(button)):
                    backend.input('button', button=button, down=i.is_mouse_clicked(button), clicks=2 if i.is_mouse_double_clicked(button) else 1, **args)
            if hovered and (io.mouse_wheel or io.mouse_wheel_h) and not wheel_taken():
                backend.input('wheel', wheel_x=io.mouse_wheel_h, wheel_y=io.mouse_wheel, **args)
                take_wheel()
            if backend.focused():
                i.set_next_frame_want_capture_keyboard(True)
                keys = [(getattr(i.Key, chr(97+n)), 65+n) for n in range(26)]
                keys += [(getattr(i.Key, name), value) for name, value in (('backspace', 8), ('tab', 9), ('enter', 13), ('escape', 27), ('space', 32), ('page_up', 33), ('page_down', 34), ('end', 35), ('home', 36), ('left_arrow', 37), ('up_arrow', 38), ('right_arrow', 39), ('down_arrow', 40), ('insert', 45), ('delete', 46))]
                for key, value in keys:
                    if i.is_key_pressed(key) or i.is_key_released(key):
                        backend.input('key', key=value, down=i.is_key_down(key), modifiers=modifiers)
                for cp in io.input_queue_characters:
                    backend.input('text', codepoint=cp, modifiers=modifiers)
    events = []
    while len(events) < 256:
        event = backend.poll()
        if event is None:
            break
        events.append(event)
    return events
