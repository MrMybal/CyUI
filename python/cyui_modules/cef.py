"""Optional CEF backend using the CyUI C ABI. No browser is loaded on import."""
from pathlib import Path
import json
import os
import threading
import time


class CEFRuntime:
    def __init__(self, library, helper, cache_directory, *, no_sandbox=False):
        import ctypes as t
        self._t, self._thread, self._views, self._errors = t, threading.get_ident(), [], []
        library, helper, cache = (Path(p).resolve() for p in (library, helper, cache_directory))
        self._search = os.add_dll_directory(str(library.parent)) if os.name == 'nt' else None
        self._dll = d = t.CDLL(str(library))
        d.cyui_cef_error.restype = t.c_char_p
        d.cyui_cef_initialize.argtypes = [t.c_char_p, t.c_char_p, t.c_int]
        self._upload_type = t.CFUNCTYPE(t.c_uint64, t.c_int, t.c_int, t.c_void_p, t.c_void_p)
        d.cyui_cef_create.argtypes = [t.c_char_p, t.c_char_p, self._upload_type, t.c_void_p]
        d.cyui_cef_create.restype = t.c_void_p
        for name in ('ready', 'focused', 'texture', 'poll', 'close', 'closed', 'destroy'):
            getattr(d, 'cyui_cef_'+name).argtypes = [t.c_void_p]
        d.cyui_cef_texture.restype = t.c_uint64
        d.cyui_cef_poll.restype = t.c_char_p
        d.cyui_cef_frame.argtypes = [t.c_void_p, t.c_int]
        d.cyui_cef_place.argtypes = [t.c_void_p]+[t.c_float]*8+[t.c_int]*2
        d.cyui_cef_input.argtypes = [t.c_void_p, t.c_int]+[t.c_float]*4+[t.c_int]*4+[t.c_uint]*2+[t.c_int]
        for name in ('navigate', 'post'):
            getattr(d, 'cyui_cef_'+name).argtypes = [t.c_void_p, t.c_char_p]
        self._active = bool(d.cyui_cef_initialize(str(cache).encode(), str(helper).encode(), no_sandbox))
        if not self._active:
            raise RuntimeError(d.cyui_cef_error().decode() or 'CEF initialization failed')

    def _check(self):
        if threading.get_ident() != self._thread or not self._active:
            raise RuntimeError('CEF must run on its owning UI thread before shutdown')

    def create(self, url, origins, upload):
        """upload(width, height, bgra_pointer) returns a host-owned ImTextureID.

        Copy/update the GPU texture during this callback; the CPU pointer expires
        when it returns. Native CEF popups are composited in the same texture.
        """
        self._check()
        view = CEFView(self, url, origins, upload)
        self._views.append(view)
        return view

    def pump(self):
        self._check()
        self._dll.cyui_cef_pump()
        if self._errors:
            raise RuntimeError('CEF texture upload failed') from self._errors.pop(0)

    def begin_frame(self):
        self.pump()
        for view in self._views:
            if view._handle:
                self._dll.cyui_cef_frame(view._handle, True)

    def end_frame(self):
        self._check()
        for view in self._views:
            if view._handle:
                self._dll.cyui_cef_frame(view._handle, False)

    def close(self):
        self._check()
        for view in self._views:
            view.close()
        end = time.monotonic()+5
        while any(not view.closed() for view in self._views):
            self.pump()
            if time.monotonic() > end:
                raise RuntimeError('CEF browsers have not closed; keep pumping before shutdown')
            time.sleep(.005)
        for view in self._views:
            if view._handle:
                self._dll.cyui_cef_destroy(view._handle)
                view._handle = None
        self._dll.cyui_cef_shutdown()
        self._active = False
        if self._search:
            self._search.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()


class CEFView:
    def __init__(self, runtime, url, origins, upload):
        self._runtime, self._dll = runtime, runtime._dll
        def callback(width, height, pointer, _):
            try:
                return upload(width, height, pointer)
            except BaseException as error:
                runtime._errors.append(error)
                return 0
        self._upload = runtime._upload_type(callback)
        self._handle = self._dll.cyui_cef_create(url.encode(), '\n'.join(origins).encode(), self._upload, None)
        if not self._handle:
            raise RuntimeError(self._dll.cyui_cef_error().decode() or 'CEF view creation failed')

    def ready(self):
        self._runtime._check()
        return bool(self._dll.cyui_cef_ready(self._handle))

    def focused(self):
        self._runtime._check()
        return bool(self._dll.cyui_cef_focused(self._handle))

    def uses_texture(self):
        return True

    def texture(self):
        self._runtime._check()
        return self._dll.cyui_cef_texture(self._handle)

    def navigate(self, url):
        self._runtime._check()
        return bool(self._dll.cyui_cef_navigate(self._handle, url.encode()))

    def post_json(self, message):
        self._runtime._check()
        text = message if isinstance(message, str) else json.dumps(message, ensure_ascii=False)
        return bool(self._dll.cyui_cef_post(self._handle, text.encode()))

    def poll(self):
        self._runtime._check()
        raw = self._dll.cyui_cef_poll(self._handle)
        return json.loads(raw) if raw else None

    def place(self, bounds, clip, visible=True, focus=False):
        self._runtime._check()
        args = []
        for low, high in (bounds, clip):
            args.extend((low[0], low[1], high[0]-low[0], high[1]-low[1]))
        self._dll.cyui_cef_place(self._handle, *args, visible, focus)

    def input(self, kind, x=0, y=0, wheel_x=0, wheel_y=0, button=0, clicks=1, key=0, native_key=0, modifiers=0, codepoint=0, down=False):
        self._runtime._check()
        self._dll.cyui_cef_input(self._handle, ('move', 'button', 'wheel', 'key', 'text', 'focus').index(kind), x, y, wheel_x, wheel_y, button, clicks, key, native_key, modifiers, codepoint, down)

    def close(self):
        self._runtime._check()
        if self._handle:
            self._dll.cyui_cef_close(self._handle)

    def closed(self):
        self._runtime._check()
        return not self._handle or bool(self._dll.cyui_cef_closed(self._handle))
