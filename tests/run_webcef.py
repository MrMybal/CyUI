"""Run the optional browser integration test using only local fixture routes."""
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import Thread
import argparse
import subprocess
import os
import tempfile
import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf8')
root = Path(__file__).resolve().parents[1]


class Fixture(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(root), **kwargs)

    def do_GET(self):
        if not self.path.startswith(('/examples/web/', '/web/')) or '..' in self.path:
            self.send_error(404)
            return
        super().do_GET()

    def log_message(self, *_):
        pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('executable', type=Path)
    args = parser.parse_args(); executable = args.executable.resolve()
    server = ThreadingHTTPServer(('127.0.0.1', 0), Fixture)
    server.handle_error = lambda *_: None
    Thread(target=server.serve_forever, daemon=True).start()
    try:
        profile = Path(tempfile.mkdtemp(prefix='cef-test-', dir=executable.parent))
        completed = subprocess.run([str(executable), f'http://127.0.0.1:{server.server_port}/examples/web/index.html', str(profile)], check=False, timeout=40, capture_output=True, text=True, encoding='utf8', errors='replace')
        print(completed.stdout)
        if completed.returncode:
            print(completed.stderr)
            completed.check_returncode()
        if not (profile/'cyui-result.txt').is_file():
            print(completed.stderr)
            raise RuntimeError('CEF process exited without completing the page/paint/message checks')
        print((profile/'cyui-result.txt').read_text())
        subprocess.run([sys.executable, '-B', str(root/'tests/test_cef_bridge.py'), str(executable.parent), f'http://127.0.0.1:{server.server_port}/examples/web/index.html'], check=True, timeout=40)
    finally:
        server.shutdown(); server.server_close()
