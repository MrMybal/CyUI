"""Validate the exported manual and the real application help route contract."""
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'python'))
from cyui_modules.help import documentation_url, open_documentation


class Resources(HTMLParser):
    def __init__(self, html):
        super().__init__()
        self.refs = []
        self.feed(html)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'script' and 'src' in attrs:
            self.refs.append(attrs['src'])
            if attrs.get('type') == 'module':
                raise AssertionError('The offline bootstrap must use classic scripts')
        if tag == 'link' and attrs.get('rel') in ('stylesheet', 'icon'):
            self.refs.append(attrs['href'])


class Documentation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.export = ROOT / 'Documentation'
        cls.manifest = json.loads((cls.export / 'DocData/manifest.json').read_text('utf-8'))
        cls.pages = {(p['locale'], p['id']): p for p in cls.manifest['pages']}

    def test_complete_languages_stable_anchors_and_revisions(self):
        self.assertEqual(self.manifest['defaultLocale'], 'en')
        en = {id for locale, id in self.pages if locale == 'en'}
        fr = {id for locale, id in self.pages if locale == 'fr'}
        self.assertEqual(en, fr)
        self.assertGreater(len(en), 20)
        for id in en:
            a, b = self.pages['en', id], self.pages['fr', id]
            self.assertEqual([h['id'] for h in a['headings']], [h['id'] for h in b['headings']], id)
            self.assertEqual(a.get('parent'), b.get('parent'), id)
            self.assertTrue(a['text'].strip())
            self.assertTrue(b['text'].strip())
        report = json.loads((self.export / 'DocData/translation-report.json').read_text('utf-8'))
        self.assertEqual(report['missing'], [])
        self.assertEqual(report['outdated'], [])
        authored = {p.stem for p in (ROOT / '.CyDocBuild/manual/en').glob('*.md')}
        self.assertFalse(authored & {p['id'] for p in report['untracked']})

    def test_only_selected_modules_and_local_classic_resources(self):
        scripts = {p.name for p in (self.export / 'DocData').glob('media-*.js')}
        self.assertEqual(scripts, {'media-image.js', 'media-gallery.js'})
        bootstrap = (ROOT / '.CyDocBuild/reader.mjs').read_text('utf-8')
        self.assertNotRegex(bootstrap, r'\bfetch\s*\(|XMLHttpRequest|https?://')
        for name in Resources((self.export / 'Documentation.html').read_text('utf-8')).refs:
            ref = urlsplit(name)
            self.assertFalse(ref.scheme or ref.netloc)
            self.assertTrue((self.export / unquote(ref.path)).is_file(), name)
        self.assertTrue((self.export / 'DocData/CyDoc-LICENSE.txt').is_file())
        self.assertTrue((self.export / 'DocAI.md').is_file())
        self.assertFalse(any(p['id'].lower().startswith(('docai', 'audit', 'translation-report')) for p in self.manifest['pages']))
        self.assertFalse((ROOT / '.CyDocBuild/node_modules/three').exists())

    def test_public_api_index_does_not_publish_implementations(self):
        api = [p for p in self.manifest['pages'] if p.get('source')]
        self.assertTrue(api)
        self.assertTrue(all(p['source'].startswith('include/cyui/') for p in api))
        self.assertFalse(any(p.get('kind') == 'source' for p in self.manifest['pages']))
        self.assertNotIn('void prepare(CodeLanguage)', str(api))
        self.assertIn(('fr', 'api-diff-viewer'), self.pages)

    def test_all_distribution_copies_match_inventory(self):
        inventory = json.loads((self.export / '.cydoc-files.json').read_text('utf-8'))
        for base in (ROOT / 'site/Documentation', ROOT / 'python/cyui_documentation/Documentation'):
            for name in inventory:
                self.assertEqual((self.export / name).read_bytes(), (base / name).read_bytes(), str(base / name))
        self.assertFalse(any('node_modules' in name or '.CyDocBuild' in name for name in inventory))

    def test_help_uris_locale_fallback_aliases_and_media(self):
        uri = documentation_url('filtering', 'searchable-menus', locale='fr-CA')
        self.assertEqual(urlsplit(uri).scheme, 'file')
        self.assertEqual(urlsplit(uri).fragment, '/fr/filtering/searchable-menus')
        self.assertTrue(documentation_url('start', locale='de').endswith('#/en/integration'))
        self.assertTrue(documentation_url('overview', media='workspace').endswith('?media=workspace'))
        for options in ({'page': 'absent'}, {'section': 'absent'}, {'media': 'absent'}):
            with self.assertRaises(ValueError):
                documentation_url(**options)
        with patch('cyui_modules.help.webbrowser.open', return_value=True) as launch:
            self.assertTrue(open_documentation('diff', locale='fr'))
            launch.assert_called_once_with(documentation_url('diff', locale='fr'))

    def test_relocated_manual_in_unicode_path_with_url_punctuation(self):
        (ROOT / 'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='docs-', dir=ROOT / 'build') as scratch:
            destination = Path(scratch) / 'Manuel été #100% + exemples'
            shutil.copytree(self.export, destination)
            uri = documentation_url('integration', 'startup', manual=destination / 'Documentation.html')
            parts = urlsplit(uri)
            self.assertEqual(parts.fragment, '/en/integration/startup')
            self.assertIn('%23', parts.path)
            self.assertIn('%25', parts.path)
            self.assertIn('%C3%A9', parts.path)
            self.assertTrue(Path(unquote(parts.path).lstrip('/')).is_file() if sys.platform == 'win32'
                            else Path(unquote(parts.path)).is_file())


if __name__ == '__main__':
    unittest.main()
