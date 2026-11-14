"""Document model, archive interoperability and real native UI frames."""
from pathlib import Path
import sys
import unittest
from io import BytesIO
import zipfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'python'))
from cyui_modules.documents import Document, DocumentState, DocumentOptions, DocumentServices, document_view
from cyui_modules.documenteditor import RichText, DocumentEditor, DocumentEditorOptions
from cyui_modules._markdown import Inline, BOLD, ITALIC, UNDERLINE, SPOILER
from cyui_modules.documentio import (DocumentPackage, DocumentIOLimits, decode_markdown, encode_markdown,
    read_cymd, write_cymd, markdown_from_html, document_html, document_package_html)


class Models(unittest.TestCase):
    def test_gfm_and_native_extensions(self):
        doc = Document('# Title\n\n**Bold *nested*** <u>under</u> ||secret||\n\n3. Third\n   - Nested\n\n- [ ] Task\n\n| A | B |\n|:---|---:|\n| x\\|y | z |\n\n![Caption|320x180](assets/p.png)\n')
        runs = [r for b in doc.blocks for r in b.inlines]
        self.assertTrue(any(r.style & (BOLD | ITALIC) == BOLD | ITALIC for r in runs))
        self.assertTrue(any(r.style & UNDERLINE for r in runs))
        self.assertTrue(any(r.style & SPOILER for r in runs))
        task = next(b for b in doc.blocks if b.task)
        self.assertEqual(doc.text.encode('utf8')[task.task_offset:task.task_offset+1], b' ')
        table = next(b for b in doc.blocks if b.kind == 'table')
        self.assertEqual((table.rows[1][0], table.alignments), ('x|y', [1, 3]))
        image = next(r for r in runs if r.kind == 'image')
        self.assertEqual((image.width, image.height), (320, 180))

    def test_byte_offsets_and_rich_history(self):
        rich = RichText([Inline(text='héllo', style=BOLD)])
        self.assertFalse(rich.replace(2, 3, 'x'))
        self.assertTrue(rich.replace(3, 5, 'y'))
        self.assertEqual(rich.markdown(), '**héyo**')
        editor = DocumentEditor('Hello world\r\n')
        self.assertTrue(editor.select(6, 11)); editor.command('bold')
        self.assertEqual(editor.markdown(), 'Hello **world**\r\n')
        editor.command('undo'); self.assertEqual(editor.markdown(), 'Hello world\r\n')
        editor.command('redo'); self.assertEqual(editor.markdown(), 'Hello **world**\r\n')

    def test_markdown_encodings_and_archive(self):
        package = DocumentPackage('# Résumé\r\n😀\r\n', {'assets/sample.bin': b'example'}, manifest='{"format":"cymd","version":1,"main":"document.md","created":"2020-01-01"}')
        for encoding in ('utf-8', 'utf-16-le', 'utf-16-be'):
            package.encoding, package.bom = encoding, True
            decoded = decode_markdown(encode_markdown(package))
            self.assertEqual((decoded.markdown, decoded.encoding, decoded.bom), (package.markdown, encoding, True))
        decoded = read_cymd(write_cymd(package))
        self.assertEqual((decoded.markdown, decoded.assets, decoded.manifest), (package.markdown, package.assets, package.manifest))
        fixture = Path(__file__).parent/'fixtures/markdown-sample.cymd'
        self.assertIn('Résumé', read_cymd(fixture.read_bytes()).markdown)
        with self.assertRaises(ValueError): read_cymd(write_cymd(package), DocumentIOLimits(expanded_bytes=2))

    def test_zip_traversal_and_duplicate_paths(self):
        for names in (('../secret',), ('assets/a', 'assets/A')):
            out = BytesIO()
            with zipfile.ZipFile(out, 'w') as archive:
                archive.writestr('document.md', '# Synthetic')
                for name in names: archive.writestr(name, b'data')
            with self.assertRaises(ValueError): read_cymd(out.getvalue())

    def test_clipboard_and_self_contained_html(self):
        text = markdown_from_html('<p>Hello <strong>bold</strong></p><script>hidden()</script><img src="remote" alt="photo">', lambda source: 'assets/photo.png')
        self.assertIn('**bold**', text); self.assertNotIn('hidden', text); self.assertIn('assets/photo.png', text)
        package = DocumentPackage('![photo](assets/photo.png)', {'assets/photo.png': b'synthetic'})
        output = document_package_html(package, '<Title>')
        self.assertIn('data:image/png;base64,', output); self.assertIn('&lt;Title&gt;', output)
        self.assertNotIn('href="javascript:', document_html(Document('[link](javascript:alert)')))
        pasted_table = Document(markdown_from_html('<table><tr><th>A</th><th>B</th></tr><tr><td><p>x|y</p></td><td>*literal*</td></tr></table>'))
        self.assertEqual(pasted_table.blocks[0].rows[1], ['x|y', '*literal*'])


import test_python_modules as frame_support
i = frame_support.i


@unittest.skipIf(i is None, 'imgui-bundle unavailable')
class NativeFrames(unittest.TestCase):
    setUpClass = classmethod(frame_support.Frames.setUpClass.__func__)
    tearDownClass = classmethod(frame_support.Frames.tearDownClass.__func__)
    frame = frame_support.Frames.frame

    def test_live_typing_paragraphs_reload_and_undo(self):
        editor = DocumentEditor('**First.**\n\nLast.\n')
        options = DocumentEditorOptions(toolbar=False, mode_selector=False)
        draw = lambda: editor.draw('typing', (800, 600), options)
        def key(value):
            i.get_io().add_key_event(value, True); self.frame(draw)
            i.get_io().add_key_event(value, False); self.frame(draw)
        self.frame(draw)
        self.frame(draw, (65, 45), (0, True)); self.frame(draw, (65, 45), (0, False))
        key(i.Key.end); key(i.Key.enter)
        after_enter = editor.markdown()
        for _ in range(4): self.frame(draw)
        self.assertEqual(editor.markdown(), after_enter)
        i.get_io().add_input_characters_utf8('New'); self.frame(draw)
        key(i.Key.enter); key(i.Key.enter)
        i.get_io().add_input_characters_utf8('Next'); self.frame(draw)
        after_paragraph = editor.markdown()
        for _ in range(4): self.frame(draw)
        self.assertEqual(editor.markdown(), after_paragraph)
        self.assertEqual(after_paragraph.count('Last.'), 1)
        self.assertIn('**Next**', after_paragraph)
        editor.command('undo'); self.frame(draw)
        self.assertNotIn('Next', editor.markdown())
        self.frame(draw, (950, 750), (0, True)); self.frame(draw, (950, 750), (0, False))
        editor.set_markdown('- **First**\r\n\r\nLast.\r\n'); self.frame(draw)
        self.frame(draw, (90, 45), (0, True)); self.frame(draw, (90, 45), (0, False))
        key(i.Key.end); key(i.Key.enter)
        i.get_io().add_input_characters_utf8('Second'); self.frame(draw)
        self.assertIn('- **Second**', editor.markdown())
        self.assertNotIn('\n', editor.markdown().replace('\r\n', ''))
        editor.set_markdown('Loaded while focused.\n')
        for _ in range(3): self.frame(draw)
        self.assertEqual(editor.markdown(), 'Loaded while focused.\n')
        editor.set_markdown(''); self.frame(draw)
        self.frame(draw, (65, 45), (0, True)); self.frame(draw, (65, 45), (0, False))
        i.get_io().add_input_characters_utf8('New document'); self.frame(draw)
        self.assertEqual(editor.markdown(), 'New document')

    def test_host_rich_paste_and_read_only(self):
        editor = DocumentEditor('First.')
        calls = []
        options = DocumentEditorOptions(toolbar=False, mode_selector=False,
            paste_rich=lambda: calls.append(True) or '**Imported**')
        draw = lambda: editor.draw('paste', (800, 600), options)
        self.frame(draw); self.frame(draw, (65, 45), (0, True)); self.frame(draw, (65, 45), (0, False))
        i.get_io().add_key_event(i.Key.mod_ctrl, True); i.get_io().add_key_event(i.Key.v, True)
        self.frame(draw)
        i.get_io().add_key_event(i.Key.v, False); i.get_io().add_key_event(i.Key.mod_ctrl, False); self.frame(draw)
        self.assertEqual(len(calls), 1); self.assertIn('**Imported**', editor.markdown())
        saved = editor.markdown(); options.read_only = True; self.frame(draw)
        i.get_io().add_key_event(i.Key.mod_ctrl, True); i.get_io().add_key_event(i.Key.v, True); self.frame(draw)
        i.get_io().add_key_event(i.Key.v, False); i.get_io().add_key_event(i.Key.mod_ctrl, False); self.frame(draw)
        self.assertEqual(editor.markdown(), saved); self.assertEqual(len(calls), 1)

    def test_modes_images_and_missing_engines(self):
        from cyui_modules.images import ImageData
        from cyui_modules.media import MediaSnapshot, MediaPlayerState, media_player
        from cyui_modules.documentweb import DocumentWebAdapter
        self.assertFalse(DocumentWebAdapter().draw(type('Media', (), {'id': 1})(), (300, 200)))
        doc = Document('# Notes\n\n**Bold** and *italic* with ||secret||.\n\n![Image|320](image.png)\n\n- [ ] Task\n')
        calls = []
        services = DocumentServices(image=lambda source: calls.append(source) or ImageData(2, 640, 360))
        options = DocumentOptions(services)
        view = DocumentState()
        self.frame(lambda: document_view('doc', view, doc, (600, 500), options))
        self.assertTrue(calls)
        editor = DocumentEditor(doc.text)
        for mode in ('live', 'source', 'split', 'reading'):
            editor.mode = mode
            self.frame(lambda: editor.draw('editor', (900, 700), DocumentEditorOptions(options)))
        self.assertEqual(editor.markdown(), doc.text)
        class Backend:
            def snapshot(self): return MediaSnapshot(duration=120, ready=True, seekable=True)
            def play(self, value): pass
            def seek(self, value): pass
            def volume(self, value): pass
            def mute(self, value): pass
        self.frame(lambda: media_player('player', MediaPlayerState(), Backend(), (600, 300)))


if __name__ == '__main__': unittest.main(verbosity=2)
