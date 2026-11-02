"""Invoked by run_webcef.py after the native browser fixture test."""
from pathlib import Path
import ctypes
import json
import shutil
import sys
import tempfile
import time

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'python'))
from cyui_modules.cef import CEFRuntime

directory, url = Path(sys.argv[1]), sys.argv[2]
if sys.platform == 'win32':
    library, helper = directory/'cyui_webcef_bridge.dll', directory/'cyui_webcef_helper.exe'
elif sys.platform == 'darwin':
    # Unvalidated here: macOS needs the helper app bundle CEF expects.
    library = directory/'libcyui_webcef_bridge.dylib'
    bundle = directory/'cyui_webcef_helper.app/Contents/MacOS/cyui_webcef_helper'
    helper = bundle if bundle.exists() else directory/'cyui_webcef_helper'
else:
    library, helper = directory/'libcyui_webcef_bridge.so', directory/'cyui_webcef_helper'
# run_webcef.py passes a profile it deletes after this process exits: libcef keeps files in it
# open until the process ends, so it cannot be removed from here on Windows.
owned = len(sys.argv) < 4
profile = tempfile.mkdtemp(prefix='cef-python-', dir=directory) if owned else sys.argv[3]
frames, released = [], []


def upload(width, height, pointer):
    frames.append((width, height, ctypes.string_at(pointer, min(4096, width*height*4))))
    return 42


try:
    # This local test explicitly opts out of the process sandbox. Release hosts own
    # sandbox initialization and packaging, as described in docs/WEB.md.
    with CEFRuntime(library, helper, profile, no_sandbox=True) as runtime:
        web = runtime.create(url, (url.rsplit('/examples/', 1)[0],), upload, released.append)
        web.place(((0, 0), (800, 600)), ((0, 0), (800, 600)))
        assert not web.navigate('file:///private.txt')
        assert not web.post_json('invalid JSON')
        ready = reply = typed = sent_input = focused = sent_typing = False
        input_position = (100, 100)
        start = time.monotonic()
        end = start+25
        while time.monotonic() < end and not (reply and frames and typed):
            runtime.pump()
            while (event := web.poll()) is not None:
                if event['kind'] == 'message':
                    message = json.loads(event['data'])
                    if message.get('type') == 'page.ready':
                        ready = True
                        input_position = message['payload']['input']['x'], message['payload']['input']['y']
                    typed |= message.get('type') == 'test.input' and message.get('payload', {}).get('text') == '音楽'
                    reply |= message.get('type') == 'test.reply'
                    focused |= message.get('type') == 'test.focus'
            if ready and web.post_json({'protocol': 'cyui.web', 'version': 1, 'type': 'host.reply', 'payload': {'python': True}}):
                ready = False
            if reply and not sent_input and time.monotonic()-start > 2:
                web.input('focus', down=True)
                web.input('move', *input_position, down=True)
                web.input('button', *input_position, down=True, modifiers=16)
                web.input('button', *input_position, down=False)
                sent_input = True
            if focused and sent_input and not sent_typing and time.monotonic()-start > 3:
                web.input('key', key=65, modifiers=2, down=True)
                web.input('key', key=65, modifiers=2, down=False)
                web.input('text', codepoint=0x97f3)
                web.input('text', codepoint=0x697d)
                sent_typing = True
            time.sleep(.005)
        assert reply and frames and typed, 'Python CEF page/paint/message round trip timed out'
        assert web.texture() == 42
        runtime.begin_frame(); runtime.end_frame()  # hidden panel lifetime
        web.close()
    assert released == [42], f'release callback after close: {released}'
finally:
    if owned:
        shutil.rmtree(profile, ignore_errors=True)  # Best effort, see above.
print(f'Python CEF real page, texture upload, messages and release passed ({len(frames)} frames)')
