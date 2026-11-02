"""Model and real ImGui frame tests; no application engine or desktop automation."""
import importlib
import math
import random
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'python'))
from cyui_modules.common import Range, snap, zoom_range
from cyui_modules.canvas import CanvasView, CanvasState, Canvas
from cyui_modules.plots import Axis, Series, PlotState, plot, heatmap, reduce_plot, histogram
from cyui_modules.curves import CurvePoint, CurveState, evaluate_curve, curve_editor
from cyui_modules.nodes import Node, Pin, Link, NodeState, node_editor, arrange_nodes
from cyui_modules.timeline import Lane, Clip, TimelineState, timeline
from cyui_modules.audioviews import make_peaks, WaveformState, waveform, spectrum, oscilloscope
from cyui_modules.images import ImageData, ImageState, GradientStop, GradientState, sample_gradient, image_viewport, gradient_editor
from cyui_modules.viewport import OrbitCamera, ViewportState, Transform2D, TransformState, camera_position, camera_view_matrix, viewport, transform_gizmo
from cyui_modules.code import CodeDocument, CodeViewState, SourceSpan, CodeEditor, code_viewer
from cyui_modules.diff import DiffDocument, DiffState, diff_viewer
from cyui_modules.inspector import Property, InspectorState, inspector
from cyui_modules.assets import Asset, AssetState, asset_browser
from cyui_modules.commands import Command, CommandState, rank_commands, command_palette
from cyui_modules.diagnostics import LogBuffer, LogState, Diagnostic, log_view, diagnostic_list
from cyui_modules.documents import Document, DocumentState, TranscriptState, TranscriptMessage, document_view, transcript
from cyui_modules.web import WebPolicy, web_origin, web_panel, theme_message


class Models(unittest.TestCase):
    def test_navigation_and_sampling(self):
        self.assertEqual(snap(-1.25, .5), -1.5)
        self.assertEqual(zoom_range(Range(0, 10), 3, .5), Range(1.5, 6.5))
        view = CanvasView((1e9, 2e9), 2)
        point = view.world((100, 200)); view.zoom_at((100, 200), 4)
        self.assertEqual(point, view.world((100, 200)))
        axis = Axis(Range(20, 20000), True)
        self.assertAlmostEqual(axis.value(axis.fraction(1000)), 1000)
        data = [(n, 100 if n == 501 else 0) for n in range(1000)]
        reduced = reduce_plot(data, 50)
        self.assertLessEqual(len(reduced), 50)
        self.assertIn((501, 100), reduced)
        data[300] = 300, math.nan
        self.assertTrue(any(math.isnan(p[1]) for p in reduce_plot(data, 50)))
        self.assertEqual(histogram([0, .5, 1, math.nan, -1], Range(0, 1), 2), [1, 2])

    def test_curves_and_color(self):
        keys = [CurvePoint(1, 0, 0), CurvePoint(2, 1, 1)]
        self.assertAlmostEqual(evaluate_curve(keys, .5), .5)
        keys[0].shape = 'step'
        self.assertEqual(evaluate_curve(keys, .5), 0)
        self.assertEqual(evaluate_curve(keys, 1), 1)
        peaks = make_peaks([0, 1, -1, .5], 2)
        self.assertEqual(peaks, [(0, 1), (-1, .5)])
        self.assertTrue(math.isnan(make_peaks([math.nan], 1)[0][0]))
        stops = [GradientStop(1, 0, (0, 0, 0, 1)), GradientStop(2, 1, (1, 1, 1, 1))]
        self.assertEqual(sample_gradient(stops, .5), (.5, .5, .5, 1))
        self.assertGreater(sample_gradient(stops, .5, True)[0], .7)
        camera = OrbitCamera(pitch=0)
        self.assertEqual(camera_position(camera), (0., 0., 5.))
        self.assertEqual(camera_view_matrix(camera)[14:], [-5., 1.])

    def test_code_and_diff(self):
        doc = CodeDocument('\tfloat 音楽 = 42;\r\n/* comment\ncontinued */\n', 'cpp', 7)
        self.assertTrue(doc.lines[0].text.startswith('    float'))
        self.assertIn('\r\n', doc.text)
        self.assertEqual(doc.lines[2].spans[0][2], 'comment')
        self.assertFalse(doc.set_overlays([], 6))
        self.assertTrue(doc.set_overlays([SourceSpan(0, 8, 9, 'type')], 7))
        self.assertEqual(doc.lines[0].overlays, [])  # offset cuts a UTF-8 character
        rng = random.Random(529)
        for _ in range(200):
            a = [rng.choice(['a', 'b', 'c', 'β', '']) for _ in range(rng.randrange(25))]
            b = [rng.choice(['a', 'b', 'c', 'β', '']) for _ in range(rng.randrange(25))]
            diff = DiffDocument(); diff.set_texts('\n'.join(a)+'\n' if a else '', '\n'.join(b)+'\n' if b else '', work_budget=rng.choice([0, 4000000]))
            self.assertEqual([r.code.text for r in diff.rows if r.old_line], a)
            self.assertEqual([r.code.text for r in diff.rows if r.new_line], b)
        diff.set_unified('--- a\n+++ b\n@@ -4,1 +9,1 @@\n-old\n+new\n')
        self.assertEqual((diff.rows[3].old_line, diff.rows[4].new_line), (4, 9))

    def test_bounded_logs_and_documents(self):
        from threading import Thread
        buffer = LogBuffer(3, 1024)
        writer = Thread(target=lambda: [buffer.append('info', str(n)) for n in range(10)])
        writer.start(); writer.join()
        revision, rows = buffer.snapshot()
        self.assertEqual([e.text for e in rows], ['7', '8', '9'])
        self.assertIsNone(buffer.snapshot(revision)[1])
        doc = Document('# Title\n```cpp\nint x=1;\n```\n> Note\n| A | B |\n|---|---|\n| 1 | 2 |\n')
        self.assertEqual([b.kind for b in doc.blocks], ['heading', 'code', 'quote', 'table'])
        self.assertEqual(rank_commands([Command(1, 'Save document'), Command(2, 'Open document')], 'document save'), [0])

    def test_web_policy(self):
        policy = WebPolicy(('http://127.0.0.1:4173', 'https://EXAMPLE.com:443'))
        for url in ('http://127.0.0.1:4173/chat', 'https://example.com/app?x=1'):
            self.assertTrue(policy.allows(url))
        for url in ('file:///secret', 'javascript:alert(1)', 'https://example.com.evil', 'https://example.com@evil', 'https://example.com\\@evil', 'http://127.0.0.1:4174', 'https://example.com:99999999', 'https://%65xample.com', 'https://example.com\n.evil'):
            self.assertFalse(policy.allows(url), url)
        self.assertEqual(web_origin('https://EXAMPLE.com:00443/a'), 'https://example.com')


try:
    from imgui_bundle import imgui as i
    import cyui as c
except ImportError:
    i = None


@unittest.skipIf(i is None, 'imgui-bundle is required for real UI frame tests')
class Frames(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        i.create_context()
        io = i.get_io(); io.display_size = (1000, 800); io.delta_time = 1/60; io.set_ini_filename('')
        c.apply()
        # Modern ImGui font texture protocol, handled without a graphics backend.
        io.backend_flags |= i.BackendFlags_.renderer_has_textures

    @classmethod
    def tearDownClass(cls):
        i.destroy_context()

    def frame(self, draw, position=(-100, -100), button=None, escape=False):
        io = i.get_io(); io.add_mouse_pos_event(*position)
        if button:
            io.add_mouse_button_event(*button)
        if escape:
            io.add_key_event(i.Key.escape, True)
        i.new_frame(); i.set_next_window_pos((0, 0)); i.set_next_window_size((1000, 800))
        i.begin('Test', None, i.WindowFlags_.no_title_bar | i.WindowFlags_.no_saved_settings | i.WindowFlags_.no_move)
        i.set_cursor_screen_pos((20, 20))
        result = draw()
        i.dummy((0, 0)); i.end(); i.render()
        if escape:
            io.add_key_event(i.Key.escape, False)
        return result

    def test_all_modules_render(self):
        from cyui_modules import audio
        nodes = [Node(1, (0, 0), 'Source', outputs=[Pin(11, 'out')]), Node(2, (300, 0), 'Target', inputs=[Pin(21, 'in')])]
        line = Series(1, [(0, 0), (1, 1)], label='Signal')
        doc = CodeDocument('float value = 42; // 音楽\n', 'cpp')
        diff = DiffDocument(); diff.set_texts('a\nb\n', 'a\nβ\n')
        document = Document('# Title\nHello **bold** and `code` with [a link](https://example.com).\n| A | B |\n|---|---|\n| 1 | 2 |\n```cpp\nint x;\n```')
        log = LogBuffer(); log.append('warning', 'content ## literal')
        class Backend:
            def ready(self): return False
            def place(self, *args): self.placement = args
            def poll(self): return None
        backend = Backend()
        draws = {
            'canvas': lambda: Canvas('canvas', CanvasState()).__exit__(),
            'plots': lambda: plot('plot', PlotState(), [line]),
            'heatmap': lambda: heatmap('heat', [0, 1, 2, 3], 2, 2, Range(0, 3)),
            'curves': lambda: curve_editor('curve', CurveState(), [CurvePoint(1, .2, .2), CurvePoint(2, .8, .8)]),
            'nodes': lambda: node_editor('nodes', NodeState(), nodes, [Link(1, 11, 21)]),
            'timeline': lambda: timeline('timeline', TimelineState(), [Lane(1, 'Track')], [Clip(1, 1, 1, 2, 'Clip')]),
            'waveform': lambda: waveform('wave', WaveformState(), [make_peaks([0, 1, -1, .5], 2)], Range(0, 1)),
            'spectrum': lambda: spectrum('spectrum', PlotState(), [line]),
            'scope': lambda: oscilloscope('scope', PlotState(), [line]),
            'code': lambda: code_viewer('code', CodeViewState(), doc),
            'editor': lambda: CodeEditor('a\r\nb\r\n', 'cpp').draw('editor'),
            'diff': lambda: diff_viewer('diff', DiffState(), diff),
            'inspector': lambda: inspector('inspector', InspectorState(), [Property(1, 'Integer', 9007199254740993), Property(2, 'Real', .5), Property(3, 'Choice', 'a', choices=('a', 'b')), Property(4, 'Flag', True)]),
            'assets': lambda: asset_browser('assets', AssetState(), [Asset(1, 'Folder', folder=True), Asset(2, 'Image ## literal')]),
            'image': lambda: image_viewport('image', ImageState(), ImageData(0, 256, 256)),
            'gradient': lambda: gradient_editor('gradient', GradientState(), [GradientStop(1, 0, (0, 0, 0, 1)), GradientStop(2, 1, (1, 1, 1, 1))]),
            'viewport': lambda: viewport('viewport', ViewportState(), ImageData(0, 256, 256)),
            'gizmo': lambda: transform_gizmo('gizmo', TransformState(), Transform2D()),
            'commands': lambda: command_palette('Commands', True, CommandState(), [Command(1, 'Example')]),
            'log': lambda: log_view('log', LogState(), log),
            'diagnostics': lambda: diagnostic_list('diagnostics', [Diagnostic(1, 'Message', 'file.cpp', 1, 2)]),
            'document': lambda: document_view('document', DocumentState(), document),
            'transcript': lambda: transcript('transcript', TranscriptState(), [TranscriptMessage(1, 'User', 'Hello')]),
            'web': lambda: web_panel('web', backend),
            'knob': lambda: audio.knob('knob', .5, 0, 1, .5),
            'fader': lambda: audio.fader('fader', -12),
            'meter': lambda: audio.meter('meter', [-12, -6], [-18, -10]),
        }
        for name, draw in draws.items():
            print('Render', name, flush=True)
            self.frame(draw)
            self.frame(draw)
        self.assertEqual(theme_message()['protocol'], 'cyui.web')

    def test_editor_keeps_line_endings(self):
        editor = CodeEditor('a\r\nb\r\n', 'cpp')
        self.frame(lambda: editor.draw('CRLF editor'))
        self.assertEqual(editor.text, 'a\r\nb\r\n')

    def test_command_modal_host_close(self):
        state = CommandState()
        commands = [Command(1, 'Example')]
        self.frame(lambda: command_palette('Lifecycle', True, state, commands))
        def close():
            self.assertEqual(command_palette('Lifecycle', False, state, commands), (False, 0))
            self.assertFalse(i.is_popup_open('Lifecycle'))
        self.frame(close)

    def test_web_panel_forwards_unicode_and_keys(self):
        class Backend:
            def __init__(self): self.events = []
            def ready(self): return True
            def place(self, *args): pass
            def poll(self): return None
            def uses_texture(self): return True
            def texture(self): return 0
            def focused(self): return True
            def input(self, kind, **values): self.events.append((kind, values))
        backend = Backend()
        self.frame(lambda: web_panel('Input web', backend))
        io = i.get_io()
        io.add_input_characters_utf8('音楽')
        io.add_key_event(i.Key.enter, True)
        self.frame(lambda: web_panel('Input web', backend), (40, 40))
        self.assertEqual([event['codepoint'] for kind, event in backend.events if kind == 'text'], [ord(c) for c in '音楽'])
        self.assertTrue(any(kind == 'key' and event['key'] == 13 and event['down'] for kind, event in backend.events))
        io.add_key_event(i.Key.enter, False)
        self.frame(lambda: web_panel('Input web', backend))

    def test_example_gallery_pages(self):
        import importlib.util
        path = Path(__file__).resolve().parents[1]/'examples/python_modules.py'
        spec = importlib.util.spec_from_file_location('cyui_example', path)
        example = importlib.util.module_from_spec(spec); spec.loader.exec_module(example)
        gallery = example.ModuleGallery()
        for name in ('Canvas', 'Plots', 'Curves', 'Nodes', 'Timeline', 'Audio', 'Audio views', 'Code', 'Diff', 'Inspector', 'Assets', 'Images', 'Viewport', 'Commands', 'Diagnostics', 'Documents', 'Web'):
            self.frame(lambda: gallery.draw_page(name))

    def test_curve_gesture_and_revision_cancel(self):
        state = CurveState(); state.view.x.view = Range(0, 10); state.view.y.view = Range(0, 1)
        points = [CurvePoint(1, 2, .2)]
        draw = lambda: curve_editor('Gesture', state, points, (400, 200))
        self.frame(draw); self.frame(draw)
        event = self.frame(draw, (100, 180), (0, True))
        self.assertEqual(event.phase, 'begin')
        event = self.frame(draw, (140, 140))
        self.assertEqual(event.phase, 'update')
        self.assertAlmostEqual(event.value[0].time, 3)
        event = self.frame(lambda: curve_editor('Gesture', state, points, (400, 200), revision=1), (140, 140))
        self.assertEqual(event.phase, 'cancel')
        self.frame(draw, (140, 140), (0, False))
        self.assertEqual(points[0].time, 2)


if __name__ == '__main__':
    unittest.main(verbosity=2)
