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


# imgui Key name -> Windows virtual-key code (DOM keyCode). Left/right modifiers keep their side;
# the CEF adapter maps them to the generic code and adds the scan code on Windows.
_KEYS = [(chr(97+n), 0x41+n) for n in range(26)] + [(f'_{n}', 0x30+n) for n in range(10)] \
    + [(f'keypad{n}', 0x60+n) for n in range(10)] + [(f'f{n+1}', 0x70+n) for n in range(24)] + [
    ('backspace', 0x08), ('tab', 0x09), ('enter', 0x0d), ('keypad_enter', 0x0d), ('pause', 0x13),
    ('caps_lock', 0x14), ('escape', 0x1b), ('space', 0x20), ('page_up', 0x21), ('page_down', 0x22),
    ('end', 0x23), ('home', 0x24), ('left_arrow', 0x25), ('up_arrow', 0x26), ('right_arrow', 0x27),
    ('down_arrow', 0x28), ('print_screen', 0x2c), ('insert', 0x2d), ('delete', 0x2e),
    ('left_super', 0x5b), ('right_super', 0x5c), ('menu', 0x5d), ('keypad_multiply', 0x6a),
    ('keypad_add', 0x6b), ('keypad_subtract', 0x6d), ('keypad_decimal', 0x6e), ('keypad_divide', 0x6f),
    ('num_lock', 0x90), ('scroll_lock', 0x91), ('left_shift', 0xa0), ('right_shift', 0xa1),
    ('left_ctrl', 0xa2), ('right_ctrl', 0xa3), ('left_alt', 0xa4), ('right_alt', 0xa5),
    ('app_back', 0xa6), ('app_forward', 0xa7), ('semicolon', 0xba), ('equal', 0xbb), ('comma', 0xbc),
    ('minus', 0xbd), ('period', 0xbe), ('slash', 0xbf), ('grave_accent', 0xc0), ('left_bracket', 0xdb),
    ('backslash', 0xdc), ('right_bracket', 0xdd), ('apostrophe', 0xde), ('oem102', 0xe2)]


def _forward_keys(i, backend, owner, modifiers):
    """Forwards keys and claims them (key ownership) so ImGui nav/widgets do not also act."""
    internal = getattr(i, 'internal', None)
    owned = internal is not None and hasattr(internal, 'set_key_owner')
    for name, code in _KEYS:
        key = getattr(i.Key, name, None)
        if key is None:
            continue
        if owned:
            pressed = internal.is_key_pressed(key, int(i.InputFlags_.repeat), owner)
            released = internal.is_key_released(key, owner)
            internal.set_key_owner(key, owner, int(internal.InputFlagsPrivate_.lock_until_release))
        else:  # Older bindings: forward only (ImGui nav may also see the key).
            pressed, released = i.is_key_pressed(key), i.is_key_released(key)
        if pressed:
            backend.input('key', key=code, down=True, modifiers=modifiers)
        if released:
            backend.input('key', key=code, down=False, modifiers=modifiers)


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

    Call CEFRuntime.pump() before new_frame, then begin_frame()/end_frame() every frame,
    including frames with hidden tabs. focus is one-shot (taken on a False -> True change).
    """
    i, c = ui()
    available = i.get_content_region_avail()
    extent = tuple(max(c.dp(1), size[n] if size[n] > 0 else available[n]+size[n]) for n in (0, 1))
    low = tuple(i.get_cursor_screen_pos())
    high = tuple(low[n]+extent[n] for n in (0, 1))
    i.invisible_button(identifier, extent)
    owner = i.get_item_id()
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
            storage = i.get_state_storage()
            for button in range(3):
                if (hovered or backend.focused()) and (i.is_mouse_clicked(button) or i.is_mouse_released(button)):
                    # The release repeats the press's click count, or the page never sees dblclick.
                    down, slot = i.is_mouse_clicked(button), i.get_id(f'{identifier}##clicks{button}')
                    if down:
                        storage.set_int(slot, max(1, i.get_mouse_clicked_count(button)))
                    backend.input('button', button=button, down=down, clicks=storage.get_int(slot, 1), **args)
            if hovered and (io.mouse_wheel or io.mouse_wheel_h) and not wheel_taken():
                backend.input('wheel', wheel_x=io.mouse_wheel_h, wheel_y=io.mouse_wheel, **args)
                take_wheel()
            if backend.focused():
                i.set_next_frame_want_capture_keyboard(True)
                _forward_keys(i, backend, owner, modifiers)
                for cp in io.input_queue_characters:
                    backend.input('text', codepoint=cp, modifiers=modifiers)
    events = []
    while len(events) < 256:
        event = backend.poll()
        if event is None:
            break
        events.append(event)
    return events
