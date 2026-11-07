"""Offline checks for the generated showcase, its navigation and capture assets."""
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import subprocess
import sys
import unittest
from urllib.parse import parse_qs, unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "site"
PAGES = ("index.html", "guide.html", "fr/index.html", "fr/guide.html")


class Document(HTMLParser):
    def __init__(self, path):
        super().__init__()
        self.ids = []
        self.references = []
        self.language = None
        self.images = []
        self.tags = []
        self.feed(path.read_text(encoding="utf-8"))

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        self.tags.append(tag)
        if tag == "html":
            self.language = attrs.get("lang")
        if "id" in attrs:
            self.ids.append(attrs["id"])
        if tag == "img":
            self.images.append(attrs)
        for name in ("href", "src"):
            if attrs.get(name):
                self.references.append((tag, attrs[name]))


class Showcase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.documents = {name: Document(SITE / name) for name in PAGES}

    def test_generated_pages_and_cache_versions_are_current(self):
        subprocess.run([sys.executable, "-B", str(ROOT / "tools/build_showcase.py"), "--check"],
                       cwd=ROOT, check=True, capture_output=True)
        for name, document in self.documents.items():
            for _, reference in document.references:
                url = urlsplit(reference)
                if url.path.endswith(("site.css", "site.js")):
                    asset = (SITE / name).parent / url.path
                    expected = hashlib.sha256(asset.read_bytes()).hexdigest()[:12]
                    self.assertEqual(parse_qs(url.query), {"v": [expected]})

    def test_all_local_links_resources_and_fragments_resolve(self):
        for name, document in self.documents.items():
            self.assertEqual(len(document.ids), len(set(document.ids)), name)
            for tag, reference in document.references:
                with self.subTest(page=name, reference=reference):
                    url = urlsplit(reference)
                    if url.scheme or url.netloc:
                        self.assertEqual(url.scheme, "https")
                        self.assertEqual(url.netloc, "github.com")
                        self.assertEqual(tag, "a", "Assets must stay local/offline")
                        continue
                    self.assertFalse(url.path.startswith("/"), "Pages must work under a GitHub project subpath")
                    target = ((SITE / name).parent / unquote(url.path)).resolve() if url.path else (SITE / name).resolve()
                    self.assertTrue(target.is_relative_to(SITE.resolve()))
                    self.assertTrue(target.is_file(), target)
                    if url.fragment and target.suffix == ".html":
                        self.assertIn(unquote(url.fragment), Document(target).ids)

    def test_language_pages_are_complete_static_documents(self):
        for name, document in self.documents.items():
            self.assertEqual(document.language, "fr" if name.startswith("fr/") else "en")
            self.assertIn("main", document.ids)
            self.assertIn("image-dialog", document.ids)
            self.assertEqual(document.tags.count("h1"), 1)
            for image in document.images:
                self.assertIn("alt", image)
                if image.get("src", "").endswith(".webp"):
                    self.assertTrue(image["alt"])
                    self.assertIn("width", image)
                    self.assertIn("height", image)
        for en, fr in (("index.html", "fr/index.html"), ("guide.html", "fr/guide.html")):
            self.assertEqual(self.documents[en].ids, self.documents[fr].ids)
            self.assertEqual(self.documents[en].tags, self.documents[fr].tags)
            for image_en, image_fr in zip(self.documents[en].images, self.documents[fr].images):
                if image_en.get("src", "").endswith(".webp"):
                    self.assertNotEqual(image_en["alt"], image_fr["alt"])

    def test_capture_hashes_and_image_metadata(self):
        folder = SITE / "assets/screenshots"
        manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
        self.assertRegex(manifest["source_revision"], r"^[a-f0-9]{40}$")
        self.assertEqual({item["file"] for item in manifest["images"]}, {p.name for p in folder.glob("*.webp")})
        for item in manifest["images"]:
            with self.subTest(image=item["file"]):
                raw = (folder / item["file"]).read_bytes()
                self.assertEqual(hashlib.sha256(raw).hexdigest(), item["sha256"])
                self.assertEqual(raw[:4], b"RIFF")
                self.assertEqual(raw[8:12], b"WEBP")
                cursor = 12
                while cursor < len(raw):
                    chunk = raw[cursor:cursor + 4]
                    size = int.from_bytes(raw[cursor + 4:cursor + 8], "little")
                    self.assertNotIn(chunk, (b"EXIF", b"XMP ", b"ICCP"))
                    cursor += 8 + size + (size % 2)
                self.assertEqual(cursor, len(raw))

    def test_site_does_not_embed_local_machine_paths(self):
        pattern = re.compile(r"(?i)[A-Z]:[\\/](?:Users|_Project|_Plugins|_UnrealSource)[\\/]|/(?:Users|home)/")
        for path in SITE.rglob("*"):
            if path.is_file() and path.suffix in (".html", ".js", ".css", ".json", ".md", ".svg"):
                self.assertIsNone(pattern.search(path.read_text(encoding="utf-8")), path)


if __name__ == "__main__":
    unittest.main()
