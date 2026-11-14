"""Model and real ImGui frame tests; no application engine or desktop automation.

The frame tests need imgui-bundle and are skipped without it, unless CYUI_REQUIRE_UI=1
(set in CI), which turns a missing or broken imgui-bundle into a failure.
"""
import importlib
import math
import os
import random
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'python'))
from cyui_modules.common import Edit, Range, snap, zoom_range
from cyui_modules.canvas import CanvasView, CanvasState, Canvas
from cyui_modules.plots import Axis, Series, PlotState, ColorRamp, sample_ramp, plot, heatmap, reduce_plot, histogram, _navigate
from cyui_modules.audio import fader_position, fader_gain, meter_position
from cyui_modules.curves import CurvePoint, CurveState, evaluate_curve, curve_editor
from cyui_modules.nodes import Node, Pin, Link, NodeState, node_editor, arrange_nodes
from cyui_modules.timeline import Lane, Clip, TimelineState, timeline
from cyui_modules.audioviews import make_peaks, WaveformState, waveform, spectrum, oscilloscope
from cyui_modules.audioviews import (SpectrogramData, spectrogram, LoudnessData, loudness,
    TruePeakChannel, true_peak, correlation, ResponseState, ResponsePoint, filter_response, DynamicsData, dynamics)
from cyui_modules.audioviews import (SpectrogramHistory, SpectrogramState, SpectrumBand, band_spectrum,
    PersistenceOptions, StereoDensityState, stereo_density, SpatialSample, SpatialScopeState, spatial_scope)
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
from cyui_modules.filters import TextFilter, FilterMenuItem, filter_menu_matches, fold_filter_text


class Models(unittest.TestCase):
    def test_streaming_spectrogram_history(self):
        history = SpectrogramHistory()
        self.assertTrue(history.reset(3, 2, .5, Range(20, 20000), True))
        for n in range(5): self.assertTrue(history.append([n, n]))
        snapshot = history.view()
        self.assertEqual(list(snapshot.values), [2, 2, 3, 3, 4, 4])
        self.assertEqual(snapshot.time, Range(1, 2.5))
        self.assertEqual((snapshot.frames, snapshot.bins), (3, 2))
        self.assertFalse(history.reset(10000000, 2, .5, Range(20, 20000)))
        self.assertFalse(history.append([])); self.assertEqual(history.view().frames, 3)
        history.clear(10); history.append([-30, -40])
        self.assertEqual(history.view().time, Range(10, 10.5))

    def test_filter_words_translations_and_nested_menus(self):
        from cyui_modules.locale import locale_scope
        first, second = TextFilter(), TextFilter('other')
        self.assertFalse(first.active())
        self.assertEqual(fold_filter_text('ÉTÉ Œuvre Straße e\u0301 音楽'), 'ete oeuvre strasse e 音楽')
        first.query = 'private-id'
        self.assertFalse(first.matches('Visible###private-id'))
        with locale_scope('fr'):
            first.query = 'rugosite'
            self.assertTrue(first.matches('Roughness'))
            first.query = 'roughness'
            self.assertTrue(first.matches('Roughness'))
            menu = [FilterMenuItem(label='File###private-id', children=[FilterMenuItem(label='Export', children=[
                FilterMenuItem(0x100000001, 'Export PNG'), FilterMenuItem(2, 'Export HDR', enabled=False)])])]
            first.query = 'fichier png'
            matches = filter_menu_matches(menu, first)
            self.assertEqual([(m.id, m.breadcrumb) for m in matches], [(0x100000001, 'Fichier / Exporter')])
            first.query = 'export'
            self.assertEqual([m.enabled for m in filter_menu_matches(menu, first)], [True, False])
            menu[0].enabled = False
            self.assertFalse(filter_menu_matches(menu, first)[0].enabled)
            first.query = 'private-id'
            self.assertEqual(filter_menu_matches(menu, first), [])
        self.assertEqual(second.query, 'other')

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
        # A recreated buffer never shares a revision with the old one: its rows are copied.
        recreated = LogBuffer(3, 1024)
        for n in range(10):
            recreated.append('info', 'new')
        self.assertNotEqual(recreated.snapshot()[0], revision)
        self.assertEqual([e.text for e in recreated.snapshot(revision)[1]], ['new']*3)
        doc = Document('# Title\n```cpp\nint x=1;\n```\n> Note\n| A | B |\n|---|---|\n| 1 | 2 |\n')
        self.assertEqual([b.kind for b in doc.blocks], ['heading', 'code', 'quote', 'table'])
        self.assertEqual(rank_commands([Command(1, 'Save document'), Command(2, 'Open document')], 'document save'), [0])

    def test_inline_markdown_tokens(self):
        from cyui_modules.documents import _INLINE
        kinds = [(m.group(), m.lastgroup) for m in _INLINE.finditer('**bold** *em* `code` [a](https://x) f(*args) g(**kw) `')]
        self.assertEqual(kinds, [('**bold**', 'strong'), ('*em*', 'emphasis'), ('`code`', 'mono'), ('[a](https://x)', 'link'),
                                 ('f(*args) ', 'word'), ('g(**kw) ', 'word'), ('`', 'word')])
        # As in C++: no emphasis without text right inside the markers.
        self.assertEqual([m.lastgroup for m in _INLINE.finditer('2 * 3 * 4 ** x **')], ['word']*8)

    def test_arrange_nodes_with_a_cycle(self):
        # 1 -> 2 <-> 3 -> 4: nodes in and after the cycle still advance by column.
        nodes = [Node(1, (0, 0), 'A', outputs=[Pin(11, 'o')]), Node(2, (0, 0), 'B', inputs=[Pin(21, 'i')], outputs=[Pin(22, 'o')]),
                 Node(3, (0, 0), 'C', inputs=[Pin(31, 'i')], outputs=[Pin(32, 'o'), Pin(33, 'o')]), Node(4, (0, 0), 'D', inputs=[Pin(41, 'i')])]
        layout = arrange_nodes(nodes, [Link(1, 11, 21), Link(2, 22, 31), Link(3, 32, 21), Link(4, 33, 41)])
        self.assertLess(layout[1][0], layout[2][0])
        self.assertLess(layout[2][0], layout[3][0])
        self.assertLess(layout[3][0], layout[4][0])

    def test_web_policy(self):
        policy = WebPolicy(('http://127.0.0.1:4173', 'https://EXAMPLE.com:443'))
        for url in ('http://127.0.0.1:4173/chat', 'https://example.com/app?x=1'):
            self.assertTrue(policy.allows(url))
        for url in ('file:///secret', 'javascript:alert(1)', 'https://example.com.evil', 'https://example.com@evil', 'https://example.com\\@evil', 'http://127.0.0.1:4174', 'https://example.com:99999999', 'https://%65xample.com', 'https://example.com\n.evil'):
            self.assertFalse(policy.allows(url), url)
        self.assertEqual(web_origin('https://EXAMPLE.com:00443/a'), 'https://example.com')

    def test_comment_syntax_follows_language(self):
        def roles(text, language):
            return [(text[a:a+n], role) for a, n, role in CodeDocument(text, language).lines[0].spans if role == 'comment']
        self.assertEqual(roles('a = b // 2  # half', 'python'), [('# half', 'comment')])
        self.assertEqual(roles('x = 1 /* not a comment */', 'python'), [])
        self.assertEqual(roles('key = a // b ; note', 'ini'), [('; note', 'comment')])
        self.assertEqual(roles('select 4 // 2 -- note', 'sql'), [('-- note', 'comment')])
        self.assertEqual(roles('x = 7 // 2 -- note', 'lua'), [('-- note', 'comment')])
        self.assertEqual(roles('int a; // note', 'cpp'), [('// note', 'comment')])
        self.assertEqual(roles('int a; /* note */', 'cpp'), [('/* note */', 'comment')])
        self.assertEqual(CodeDocument('ab\n\tlonger line\n', 'cpp').columns, len('    longer line'))

    def test_diff_line_endings(self):
        diff = DiffDocument(); diff.set_texts('a\r\nb\r\n', 'a\nb\n')
        self.assertEqual([r.kind for r in diff.rows], ['context', 'context', 'notice'])
        diff.set_texts('a\r\nb\r\n', 'a\r\nc\r\n')
        self.assertEqual([r.code.text for r in diff.rows if r.kind != 'context'], ['b', 'c'])
        notice = '\\ Line endings differ (CRLF / LF)'
        # CRLF on different lines in both texts, and a lone trailing CR: one notice each time.
        diff.set_texts('a\r\nb\n', 'a\nb\r\n')
        self.assertEqual([r.code.text for r in diff.rows if r.kind == 'notice'], [notice])
        diff.set_texts('a\nb\r', 'a\nb')
        self.assertEqual([r.kind for r in diff.rows[:2]], ['context', 'context'])
        self.assertEqual([r.code.text for r in diff.rows].count(notice), 1)
        diff.set_texts('a\r\nb\r\n', 'a\r\nb\r\n')
        self.assertEqual([r.kind for r in diff.rows], ['context', 'context'])
        # Form feed, U+2028 and U+0085 are content in a unified diff, not line breaks.
        diff.set_unified('@@ -1,1 +1,1 @@\r\n-a\x0cb c\x85d\r\n+e\r\n')
        self.assertEqual([(r.kind, r.code.text) for r in diff.rows], [('hunk', '@@ -1,1 +1,1 @@'), ('removed', 'a\x0cb c\x85d'), ('added', 'e')])

    def test_navigation_limits(self):
        deep = Range(1e18, 1e18+4096)
        for _ in range(200):
            deep = zoom_range(deep, 1e18+1, .5)
        self.assertTrue(deep.valid)
        self.assertGreater(deep.span, 0)
        for minimum in (0., -5.):
            axis = Axis(Range(minimum, 100), True)
            _navigate(axis, 0, 50, math.exp(-.15))   # wheel zoom
            _navigate(axis, .1, 50, 1)               # drag
            self.assertTrue(axis.view.valid)
        axis = Axis(Range(1, 10), True)
        _navigate(axis, 0, 5, 1e300)                 # would overflow exp(): keeps the range
        self.assertEqual(axis.view, Range(1, 10))
        _navigate(axis, 0, -1, .5)                   # no log of a negative anchor
        self.assertTrue(axis.view.valid)

    def test_fader_curve(self):
        self.assertEqual(fader_position(-70), 0)
        self.assertEqual(fader_position(6), 1)
        # Continuous at the bottom: a small drag from -70 dB is a small move, not a jump.
        self.assertLess(fader_position(-69.5), .01)
        self.assertGreater(fader_gain(.005), -70)
        for part in (.001, .1, .5, .9, 1):
            self.assertAlmostEqual(fader_position(fader_gain(part)), part)
        self.assertLess(fader_position(6, -70, 12), 1)
        self.assertEqual(fader_position(12, -70, 12), 1)
        self.assertEqual(fader_position(math.nan), 0)
        self.assertEqual(fader_gain(.5, 0, 0), 0)
        self.assertEqual(fader_position(-10, 0, 0), 0)
        # A tiny range stays finite and invertible.
        for part in (0, .25, 1):
            level = fader_gain(part, 0, 1e-7)
            self.assertTrue(math.isfinite(level))
            self.assertAlmostEqual(fader_position(level, 0, 1e-7), part, places=6)
        self.assertEqual(fader_position(math.inf), 1)
        self.assertEqual(fader_position(1e6), 1)
        self.assertEqual(meter_position(-70), 0)


_ui_error = None
try:
    from imgui_bundle import imgui as i
    import cyui as c
except ImportError as error:
    i, _ui_error = None, error


class Environment(unittest.TestCase):
    def test_locale_catalogs_and_contexts(self):
        from cyui_modules.locale import locale, tr, tr_label, set_locale, register_translations, locale_scope
        self.assertEqual(locale(), 'en')
        with locale_scope('fr-CA'):
            self.assertEqual(tr('Save'), 'Enregistrer')
            self.assertEqual(tr_label('Reset measurements###reset'), 'Réinitialiser les mesures###reset')
            self.assertEqual(tr('unknown', 'English fallback'), 'English fallback')
            self.assertTrue(tr_label('Save###document').endswith('###document'))
            self.assertFalse(set_locale('unknown'))
            self.assertEqual(locale(), 'fr')
            self.assertTrue(register_translations('fr', {'app.export': 'Exporter'}))
            self.assertEqual(tr('app.export', 'Export'), 'Exporter')
        self.assertEqual(locale(), 'en')
        self.assertTrue(register_translations('de', {'Save': 'Speichern'}, 'Deutsch'))
        with locale_scope('de-DE'):
            self.assertEqual(tr('Save'), 'Speichern')
            self.assertEqual(tr('Missing'), 'Missing')

    def test_ui_tests_can_run_when_required(self):
        if os.environ.get('CYUI_REQUIRE_UI') == '1' and i is None:
            self.fail(f'CYUI_REQUIRE_UI=1 but imgui-bundle could not be imported: {_ui_error}')


@unittest.skipIf(i is None, 'imgui-bundle is required for real UI frame tests')
class Frames(unittest.TestCase):
    def test_spectral_rendering_navigation_and_persistence(self):
        ramp = ColorRamp(colors=[c.palette().info, c.palette().error])
        self.assertEqual(sample_ramp(ramp, -1), c.palette().info)
        self.assertEqual(sample_ramp(ramp, .5), c.mix(c.palette().info, c.palette().error, .5))
        data = SpectrogramData(list(range(16)), 4, 4, Range(0, 4), Range(20, 20000), True)
        view = PlotState(Axis(Range(0, 4)), Axis(Range(20, 20000), True))
        for orientation in ('time_horizontal', 'time_vertical'):
            for reverse in (False, True):
                draw = lambda: spectrogram('orientation', view, data, (400, 200), orientation=orientation, reverse_time=reverse)
                self.frame(draw); result = self.frame(draw, (120, 70))
                expected_frame = (3 if reverse else 1) if orientation == 'time_horizontal' else (1 if reverse else 3)
                self.assertEqual((result['frame'], result['bin']), (expected_frame, 3 if orientation == 'time_horizontal' else 1))
        result = self.frame(lambda: spectrogram('legend', view, data, (400, 200), legend=True), (400, 80))
        self.assertFalse(result['hovered']); self.assertLess(result['bounds'][1][0], 400)
        color_data = SpectrogramData([-20, -20, -60, -60], 2, 2, Range(0, 4), Range(20, 20000), True)
        def frequency_draw():
            before = len(i.get_window_draw_list().vtx_buffer)
            spectrogram('frequency-mode', view, color_data, (400, 200), axes=False, color_by_frequency=True, ramp=ColorRamp('frequency'))
            vertices = i.get_window_draw_list().vtx_buffer
            return vertices[before+4].col, vertices[before+8].col
        first_color, second_color = self.frame(frequency_draw)
        self.assertEqual(first_color & 0xffffff, second_color & 0xffffff)
        self.assertGreater(first_color >> 24, second_color >> 24)
        follow = SpectrogramState(window=2)
        draw = lambda: spectrogram('following', follow, data, (400, 200))
        self.frame(draw); self.frame(draw, (220, 120)); i.get_io().add_mouse_wheel_event(0, 1)
        self.frame(draw, (220, 120)); self.assertFalse(follow.follow_latest)
        follow.follow_latest = True; self.frame(draw)
        self.assertEqual(follow.view.x.view, Range(2, 4))
        follow.follow_latest = False; i.get_io().delta_time = .4; self.frame(draw); i.get_io().delta_time = 1/60
        self.frame(draw, (220, 120)); self.frame(draw, (220, 120), (0, True))
        self.frame(draw, (220, 120), (0, False)); self.frame(draw, (220, 120), (0, True))
        self.assertTrue(follow.follow_latest)
        self.frame(draw, (220, 120), (0, False))
        area_view = PlotState(Axis(Range(20, 20000), True), Axis(Range(-90, 0)))
        area = Series(1, [(20, -60), (200, -20)], style='area', baseline=-90, gradient=True)
        def area_draw():
            plot('area', area_view, [area], (400, 200), axes=False, legend=False)
            return any(abs(v.pos.y-220) < .1 and v.col >> 24 == 0 for v in i.get_window_draw_list().vtx_buffer)
        self.assertTrue(self.frame(area_draw))
        band = SpectrumBand(Range(20, 200), -45)
        self.assertEqual(self.frame(lambda: band_spectrum('bands', area_view, [band], size=(400, 200), max_points=0))['skipped'], [1])
        density, options = StereoDensityState(), PersistenceOptions(resolution=32, diffusion=0, axes=False, navigation=False)
        revision = [1]
        draw = lambda: stereo_density('density', density, [(.6, .6), (math.nan, 1)], revision[0], (200, 200), persistence=options)
        self.frame(draw); first = sum(density.density.energy)
        self.frame(draw); self.assertLess(sum(density.density.energy), first)
        revision[0] += 1; self.frame(draw); self.assertGreater(sum(density.density.energy), first)
        options.half_life = 0; self.frame(draw); self.frame(draw); self.assertGreater(sum(density.density.energy), 0)
        spatial = SpatialScopeState()
        points = [SpatialSample(1000, .6, -10), SpatialSample(0), SpatialSample(200, math.nan, -10)]
        self.frame(lambda: spatial_scope('spatial', spatial, points, 1, (400, 200), persistence=options))
        index = max(range(len(spatial.density.energy)), key=lambda n: spatial.density.energy[n])
        self.assertGreaterEqual(index % 32, 24)
        spatial.view.x.view = Range(-2, 2)
        self.frame(lambda: spatial_scope('spatial', spatial, [], 1, (400, 200), persistence=options))
        self.assertEqual(sum(spatial.density.energy), 0)

    def test_audio_analysis_gestures_and_missing_data(self):
        data = SpectrogramData(list(range(16)), 4, 4, Range(0, 4), Range(20, 20000), True)
        state = PlotState(Axis(Range(0, 4)), Axis(Range(20, 20000), True))
        draw = lambda: spectrogram('heat', state, data, (400, 200))
        self.frame(draw); self.frame(draw, (220, 120))
        hit = self.frame(draw, (220, 120), (0, True))
        self.assertTrue(hit['valid'] and hit['hovered'] and hit['clicked'])
        self.assertEqual((hit['frame'], hit['bin'], hit['intensity']), (2, 2, 10))
        self.frame(draw, (220, 120), (0, False))
        i.get_io().add_mouse_wheel_event(0, 1)
        zoomed = self.frame(draw, (220, 120))
        self.assertTrue(zoomed['view_changed'])
        self.assertLess(state.x.view.span, 4)
        self.assertAlmostEqual((state.x.view.minimum+state.x.view.maximum)/2, 2)
        data.frames = 10**30
        self.assertFalse(self.frame(draw)['valid'])
        data.frames = data.bins = 1000; data.values = [-30]*1000000
        self.frame(lambda: spectrogram('bounded', state, data, (400, 200), max_cells=8, axes=False))
        self.assertLess(i.get_draw_data().total_vtx_count, 200)

        response = ResponseState(); points = [ResponsePoint(0x100000001, math.sqrt(20*20000), 0)]
        options = dict(revision=0)
        draw = lambda: filter_response('response', response, [], [], points, (400, 200), **options)['points']
        self.frame(draw); self.frame(draw)
        self.assertEqual(self.frame(draw, (220, 120), (0, True)).phase, 'begin')
        moved = self.frame(draw, (320, 80))
        self.assertEqual(moved.phase, 'update')
        self.assertGreater(moved.value[0].frequency, 3000)
        self.assertGreater(moved.value[0].gain, 9)
        self.assertEqual(points[0].gain, 0)
        self.assertEqual(self.frame(draw, (320, 80), escape=True).phase, 'cancel')
        self.frame(draw, (220, 120), (0, False)); self.frame(draw, (220, 120))
        self.assertEqual(self.frame(draw, (220, 120), (0, True)).phase, 'begin')
        self.frame(draw, (320, 80))
        self.assertTrue(self.frame(draw, (320, 80), (0, False)).committed)
        self.frame(draw, (220, 120), (0, True)); options['revision'] += 1
        self.assertEqual(self.frame(draw, (250, 100)).phase, 'cancel')
        self.frame(draw, (220, 120), (0, False))
        options['read_only'] = True
        self.assertEqual(self.frame(draw, (220, 120), (0, True)).phase, 'none')
        self.frame(draw, (220, 120), (0, False)); options['read_only'] = False
        points.append(ResponsePoint(points[0].id, points[0].frequency, 2))
        self.assertEqual(self.frame(draw, (220, 120), (0, True)).phase, 'none')
        self.frame(draw, (220, 120), (0, False))

        channels = [TruePeakChannel('L', -1, .4, 3), TruePeakChannel('R', -math.inf), TruePeakChannel('Missing')]
        draw = lambda: true_peak('peaks', channels, (600, 180))
        self.assertEqual(self.frame(draw)['clipped_channels'], 1)
        self.frame(draw, (35, 27), (0, True))
        self.assertTrue(self.frame(draw, (35, 27), (0, False))['reset_requested'])
        self.assertEqual(channels[0].overs, 3)
        many = [TruePeakChannel('Channel', -12, -6) for _ in range(10000)]
        many[-1].overs = 1
        result = self.frame(lambda: true_peak('many', many, (600, 180)))
        self.assertEqual(result['clipped_channels'], 1)
        self.assertLess(i.get_draw_data().total_vtx_count, 5000)
        self.frame(lambda: None)  # let the removed child leave ImGui's previous-frame hover state
        self.frame(lambda: None)

    def test_live_language_switch_keeps_widget_ids(self):
        from cyui_modules.locale import locale_scope
        english = self.frame(lambda: i.get_id(c.tr_label('Save')))
        with locale_scope('fr'):
            french = self.frame(lambda: i.get_id(c.tr_label('Save')))
            self.assertEqual(english, french)
            from cyui_modules.diff import DiffDocument, DiffState, diff_viewer
            doc = DiffDocument(); doc.set_texts('a\n', 'b\n')
            self.frame(lambda: diff_viewer('localized-diff', DiffState(), doc))

    @classmethod
    def setUpClass(cls):
        i.create_context()
        io = i.get_io(); io.display_size = (1000, 800); io.delta_time = 1/60; io.set_ini_filename('')
        c.apply()
        # Modern ImGui font texture protocol, handled without a graphics backend.
        io.backend_flags |= i.BackendFlags_.renderer_has_textures
        io.backend_flags |= i.BackendFlags_.renderer_has_vtx_offset

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
            'spectrogram': lambda: spectrogram('spectrogram', PlotState(), SpectrogramData([0, math.nan, -50, -20], 2, 2)),
            'loudness': lambda: loudness('loudness', PlotState(), LoudnessData()),
            'true_peak': lambda: true_peak('true_peak', [TruePeakChannel('L', -1, .4, 3), TruePeakChannel('R', -math.inf)]),
            'correlation': lambda: correlation('correlation', PlotState(), math.nan),
            'response': lambda: filter_response('response', ResponseState(), [line], [line], [ResponsePoint(1)]),
            'dynamics': lambda: dynamics('dynamics', PlotState(), DynamicsData()),
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
        for name in ('Canvas', 'Plots', 'Curves', 'Nodes', 'Timeline', 'Audio', 'Audio views', 'Audio analysis', 'Spectral displays', 'Code', 'Diff', 'Inspector', 'Assets', 'Images', 'Viewport', 'Commands', 'Diagnostics', 'Documents', 'Web', 'Filtering'):
            self.frame(lambda: gallery.draw_page(name))

    def test_example_gallery_f1_opens_local_contextual_help(self):
        import importlib.util
        from unittest.mock import patch
        from cyui_modules.locale import locale_scope
        path = Path(__file__).resolve().parents[1]/'examples/python_modules.py'
        spec = importlib.util.spec_from_file_location('cyui_help_example', path)
        example = importlib.util.module_from_spec(spec); spec.loader.exec_module(example)
        gallery = example.ModuleGallery()
        with locale_scope('fr'), patch('cyui_modules.help.open_documentation', return_value=True) as opened:
            self.frame(gallery.draw)
            i.get_io().add_key_event(i.Key.f1, True)
            self.frame(gallery.draw)
            opened.assert_called_once_with('module-recipes', 'canvas-and-numeric-plots', locale='fr')
            i.get_io().add_key_event(i.Key.f1, False)
            self.frame(gallery.draw)

    def test_filter_text_entry_and_section_preferences(self):
        state = TextFilter()
        draw = lambda: c.filter_box('filter', state)
        self.frame(draw); state.focus(); self.frame(draw); self.frame(draw)
        pasted = 'a'*3000+' été'
        i.get_io().add_input_characters_utf8(pasted)
        self.frame(draw)
        self.assertEqual(state.query, pasted)
        self.frame(draw, escape=True)
        self.assertEqual(state.query, '')
        self.frame(draw)
        i.internal.clear_active_id()
        state.query = 'roughness'
        def reveal():
            key = i.get_id(c.tr_label('Surface'))
            i.get_state_storage().set_int(key, 0)
            self.assertTrue(c.begin_filtered_section('Surface', state, True, False))
            c.end_section()
            self.assertEqual(i.get_state_storage().get_int(key, -1), 0)
        self.frame(reveal)
        state.clear()
        self.frame(lambda: self.assertFalse(c.begin_filtered_section('Surface', state, True, False)))

    def test_filtered_inspector_cancels_hidden_edits(self):
        state = InspectorState(drafts={2: .8}, active={2}, before={2: .2})
        fields = [Property(1, 'Name', 'Host value'), Property(2, 'Roughness', .8)]
        query = TextFilter('name')
        events = self.frame(lambda: inspector('filtered', state, fields, filter_state=query))
        self.assertEqual([(key, edit.phase, edit.value) for key, edit in events], [(2, 'cancel', .2)])
        self.assertFalse(state.active)
        self.assertEqual(fields[1].value, .8)

    def test_filtered_menu_executes_only_enabled_host_actions(self):
        query = TextFilter('png')
        entries = [FilterMenuItem(label='Export', children=[
            FilterMenuItem(0x100000001, 'Export PNG'), FilterMenuItem(2, 'Export HDR', enabled=False)])]
        position = [0, 0]
        def draw():
            request = c.filter_menu('filtered-menu', query, entries, reset_on_open=False, focus_on_open=False)
            low, high = i.get_item_rect_min(), i.get_item_rect_max()
            position[:] = [(low.x+high.x)/2, (low.y+high.y)/2]
            return request
        self.frame(draw); self.frame(draw)
        self.frame(draw, position, (0, True))
        self.assertEqual(self.frame(draw, position, (0, False)), 0x100000001)
        query.query = 'hdr'
        self.frame(draw); self.frame(draw)
        self.frame(draw, position, (0, True))
        self.assertEqual(self.frame(draw, position, (0, False)), 0)

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

    def key_frame(self, draw, key):
        i.get_io().add_key_event(key, True)
        result = self.frame(draw)
        i.get_io().add_key_event(key, False)
        return result

    def click(self, draw, position):
        """Mouse down then up at position; the results of both frames."""
        return self.frame(draw, position, (0, True)), self.frame(draw, position, (0, False))

    def test_modal_buttons_close_the_host_flag(self):
        host = {'open': True}
        def draw(answer=False):
            shown, host['open'] = c.begin_modal('Delete layer?', host['open'])
            result = 0
            if shown:
                result = c.modal_buttons('Delete', 'Cancel', True)
                c.end_modal()
            return shown, result
        self.assertEqual(self.frame(draw), (True, 0))
        self.assertTrue(host['open'])
        self.assertEqual(self.key_frame(draw, i.Key.enter), (True, 1))
        # modal_buttons ran after begin_modal returned: the flag follows on the next call...
        self.assertEqual(self.frame(draw), (False, 0))
        self.assertFalse(host['open'])
        # ...and the modal stays closed instead of reopening from a stale True.
        self.assertEqual(self.frame(draw), (False, 0))
        self.assertFalse(self.frame(lambda: i.is_popup_open('Delete layer?')))
        # A one-item list is written at once, like the C++ bool*.
        flag = [True]
        def listed():
            shown, _ = c.begin_modal('Rename', flag)
            if shown:
                c.modal_buttons('Rename')
                c.end_modal()
            return shown
        self.assertTrue(self.frame(listed))
        self.key_frame(listed, i.Key.escape)
        self.assertEqual(flag, [False])
        self.assertFalse(self.frame(listed))
        # Reopening later works: the closed marker does not outlive the next call.
        host['open'] = True
        for _ in range(3):
            self.frame(draw)
        self.assertEqual(self.frame(draw), (True, 0))
        self.key_frame(draw, i.Key.escape)
        self.frame(draw)
        self.assertFalse(host['open'])

    def test_notifications_reuse_slot_windows_per_context(self):
        for n in range(10):
            c.notify(f'Saved {n}', 'success')
        self.assertEqual([t['text'] for t in c._toasts()], [f'Saved {n}' for n in range(4, 10)])
        self.frame(c.render_notifications)
        self.assertIsNotNone(i.internal.find_window_by_name('##cyuiToast5'))
        self.assertIsNone(i.internal.find_window_by_name('##cyuiToast6'))
        here = i.get_current_context()
        other = i.create_context()
        try:
            i.set_current_context(other)
            self.assertEqual(c._toasts(), [])
            c.notify('Elsewhere')
        finally:
            i.set_current_context(here)
            i.destroy_context(other)
        self.assertNotIn('Elsewhere', [t['text'] for t in c._toasts()])
        for toast in c._toasts():
            toast['until'] = 0
        self.frame(c.render_notifications)
        self.assertEqual(c._toasts(), [])

    def test_core_value_controls(self):
        state = {'on': False, 'mode': 0, 'level': 0.}
        def switch():
            changed, state['on'] = c.toggle_switch('Enabled', state['on'])
            return changed
        self.assertEqual(self.click(switch, (30, 28)), (False, True))
        self.assertTrue(state['on'])
        def modes():
            changed, state['mode'] = c.segmented('Mode', ['One', 'Two', 'Three'], state['mode'], 300)
            return changed
        self.assertEqual(self.click(modes, (170, 28)), (False, True))
        self.assertEqual(state['mode'], 1)
        def level():
            changed, state['level'] = c.slider_bar('Level', state['level'], 0., 1., width=200)
            return changed
        self.assertTrue(self.frame(level, (120, 28), (0, True)))
        self.assertAlmostEqual(state['level'], .5)
        self.frame(level, (120, 28), (0, False))
        i.get_io().add_mouse_wheel_event(0, 1)
        self.assertTrue(self.frame(level, (120, 28)))
        self.assertAlmostEqual(state['level'], .51)

    def test_core_edits_wheel_and_disabled(self):
        state = {'level': .25, 'on': False}
        def level():
            changed, state['level'] = c.slider_bar('Restore', state['level'], 0., 1., width=200)
            return changed, i.is_item_deactivated_after_edit()
        # Click (jumps to .5), then the second click of a double click restores .25.
        self.click(level, (120, 28))
        self.assertAlmostEqual(state['level'], .5)
        self.click(level, (120, 28))
        self.assertAlmostEqual(state['level'], .25)
        self.frame(level, (120, 28))  # hovered: the wheel is owned before it turns
        self.assertNotEqual(i.internal.get_key_owner(i.Key.mouse_wheel_y), 0)
        def switch():
            changed, state['on'] = c.toggle_switch('Edited', state['on'])
            return changed, i.is_item_deactivated_after_edit()
        self.assertEqual(self.click(switch, (30, 28))[1], (True, True))
        def faded():
            normal = c._faded(c.palette().accent)
            i.begin_disabled()
            dim = c._faded(c.palette().accent)
            i.end_disabled()
            return normal >> 24, dim >> 24
        normal, dim = self.frame(faded)
        self.assertLess(dim, normal)
        colours = {c.categorical(n) for n in range(12)}
        self.assertEqual(len(colours), 12)
        self.assertEqual(c.categorical(12), c.categorical(0))
        p = c.palette()
        self.assertIn(c.contrast_text(p.accent), (p.text, p.bg0, p.bg5))

    def test_nested_modals_close_only_themselves(self):
        flags = {'outer': True, 'inner': True}
        def draw():
            shown, flags['outer'] = c.begin_modal('Outer', flags['outer'])
            inner_result = 0
            if shown:
                inner, flags['inner'] = c.begin_modal('Inner', flags['inner'])
                if inner:
                    inner_result = c.modal_buttons('OK')
                    c.end_modal()
                c.end_modal()
            return shown, inner_result
        self.frame(draw); self.frame(draw)
        self.assertEqual(self.key_frame(draw, i.Key.escape), (True, 2))
        self.assertEqual(self.frame(draw), (True, 0))
        self.assertEqual(flags, {'outer': True, 'inner': False})
        self.assertEqual(self.frame(draw), (True, 0))

    def test_audio_fader_slow_drag_and_meter_hold(self):
        from cyui_modules import audio
        state = {'gain': -70.}
        def fader():
            changed, state['gain'] = audio.fader('Gain', state['gain'])
            return changed
        self.frame(fader, (35, 150), (0, True))
        for step in range(1, 11):
            self.frame(fader, (35, 150-step))
        self.frame(fader, (35, 140), (0, False))
        self.assertGreater(state['gain'], -70)
        held = []
        def meter(peaks):
            audio.meter('Meter', peaks, [])
            storage = i.get_state_storage()
            held.append(storage.get_float(i.get_id('Meter##hold0'), -120.))
        self.frame(lambda: meter([1.]))
        self.frame(lambda: meter([0.]))
        self.assertGreater(held[-1], -1)       # the 0 dB peak is still held
        self.frame(lambda: meter([]))          # no channel: drawn empty, no IndexError
        knob = audio.knob
        self.frame(lambda: knob('Flat', 1., 1., 1., 1., logarithmic=True))
        self.frame(lambda: knob('Nan', math.nan, 20., 20000., 1000., logarithmic=True))

    def test_ordered_selections_and_square_scope(self):
        from cyui_modules.audioviews import stereo_scope
        state = TimelineState()
        draw = lambda: timeline('Ordered', state, [], [], size=(800, 300))
        self.frame(draw); self.frame(draw)
        self.frame(draw, (600, 250), (0, True))
        self.frame(draw, (400, 250))
        self.frame(draw, (400, 250), (0, False))
        self.assertLess(state.selection.minimum, state.selection.maximum)
        self.assertAlmostEqual(state.anchor, state.selection.maximum)
        scope = PlotState(); scope.x.view = Range(0, 4)
        self.frame(lambda: stereo_scope('Scope', scope, [], (300, 200)))
        self.assertEqual((scope.x.view, scope.y.view), (Range(-2, 2), Range(-2, 2)))
        commands, palette = [Command(1, 'One'), Command(2, 'Two')], CommandState()
        draw = lambda: command_palette('Keys', True, palette, commands)
        self.frame(draw); self.frame(draw)
        self.assertEqual(self.key_frame(draw, i.Key.down_arrow), (True, 0))
        self.assertEqual(palette.selected, 1)
        self.assertEqual(self.key_frame(draw, i.Key.enter), (False, 2))

    def test_inspector_edit_cycle(self):
        state, choice, flag, name = InspectorState(), Property(1, 'Choice', 'a', choices=('a', 'b')), Property(2, 'Flag', True), Property(3, 'Name', 'old')
        def draw():
            return inspector('Edit cycle', state, [choice, flag, name])
        line, spacing, frame = self.frame(lambda: (i.get_text_line_height(), i.get_style().item_spacing.y, i.get_frame_height()))
        row = line + spacing + frame + spacing
        combo_y = 20 + line + spacing + frame/2
        # Choice: open the combo, then pick 'b' in its popup; the pick commits at once.
        self.click(draw, (100, combo_y))
        self.frame(draw)   # the popup takes its size and place
        popup = i.internal.find_window_by_name('##Combo_00')
        self.assertIsNotNone(popup)
        padding = i.get_style().window_padding
        events = []
        for result in self.click(draw, (popup.pos.x+padding.x+10, popup.pos.y+padding.y+line+spacing+line/2)):
            events += result
        self.assertIn((1, Edit('commit', 'b')), events)
        choice.value = 'b'
        # Flag: one click commits the new value.
        events = sum(self.click(draw, (26, combo_y+row)), [])
        self.assertIn((2, Edit('commit', False)), events)
        flag.value = False
        # Name: focused and left without an edit is cancelled, so outside changes show again.
        events = sum(self.click(draw, (100, combo_y+2*row)), [])
        self.assertEqual([e.phase for k, e in events if k == 3], ['begin'])
        events = sum(self.click(draw, (900, 700)), [])
        self.assertEqual([e.phase for k, e in events if k == 3], ['cancel'])
        self.assertEqual(state.active, set())
        name.value = 'from host'
        self.frame(draw)
        self.assertEqual(state.drafts[3], 'from host')
        # Escape with no field being edited cancels nothing.
        state.active.add(3); state.before[3] = 'from host'; state.errors[3] = 'kept with its error'
        events = self.key_frame(draw, i.Key.escape)
        self.assertEqual(events, [])
        self.assertIn(3, state.active)


if __name__ == '__main__':
    unittest.main(verbosity=2)
