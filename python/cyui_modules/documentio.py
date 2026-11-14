"""In-memory Markdown/CyMD import/export. Never extracts an archive to disk."""
from dataclasses import dataclass, field
from html.parser import HTMLParser
from html import escape
from io import BytesIO
import json
from pathlib import PurePosixPath
import zipfile
from ._markdown import BOLD, ITALIC, UNDERLINE, STRIKE, HIGHLIGHT, SPOILER, KEYBOARD, MONO


@dataclass
class DocumentPackage:
    markdown: str = ''
    assets: dict = field(default_factory=dict)
    previews: str = '{}'
    encoding: str = 'utf-8'
    bom: bool = False
    manifest: str = ''


@dataclass
class DocumentIOLimits:
    archive_bytes: int = 64*1024*1024
    expanded_bytes: int = 128*1024*1024
    entry_bytes: int = 32*1024*1024
    entries: int = 4096


def asset_path(path):
    return (isinstance(path, str) and bool(path) and len(path) <= 4096 and not path.startswith('/')
            and not any(ch in path for ch in ('\\', ':', '\0'))
            and all(part and part not in ('.', '..') and not part.endswith(('.', ' '))
                    and all(ord(c) >= 32 and ord(c) != 127 for c in part) for part in path.split('/')))


def decode_markdown(data):
    if len(data) > 32*1024*1024: raise ValueError('Document exceeds 32 MiB')
    encoding, bom, skip = 'utf-8', False, 0
    for marker, name in ((b'\xef\xbb\xbf', 'utf-8'), (b'\xff\xfe', 'utf-16-le'), (b'\xfe\xff', 'utf-16-be')):
        if data.startswith(marker): encoding, bom, skip = name, True, len(marker); break
    return DocumentPackage(data[skip:].decode(encoding, errors='strict'), encoding=encoding, bom=bom)


def encode_markdown(package):
    if len(package.markdown.encode('utf8')) > 32*1024*1024: raise ValueError('Document exceeds 32 MiB')
    markers = {'utf-8': b'\xef\xbb\xbf', 'utf-16-le': b'\xff\xfe', 'utf-16-be': b'\xfe\xff'}
    if package.encoding not in markers: raise ValueError('Unsupported Markdown encoding')
    return (markers[package.encoding] if package.bom else b'')+package.markdown.encode(package.encoding)


def read_cymd(data, limits=None):
    limits = limits or DocumentIOLimits()
    if len(data) > limits.archive_bytes: raise ValueError('Archive exceeds byte budget')
    with zipfile.ZipFile(BytesIO(data)) as archive:
        entries = archive.infolist()
        if len(entries) > limits.entries: raise ValueError('Archive exceeds entry budget')
        files, names, total = {}, set(), 0
        for entry in entries:
            name = entry.filename
            if not asset_path(name.rstrip('/')): raise ValueError('Unsafe asset path')
            if entry.is_dir(): continue
            if name.casefold() in names: raise ValueError('Duplicate asset path')
            names.add(name.casefold())
            if (entry.flag_bits & 1 or entry.extract_version >= 45 or entry.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED)
                    or (entry.external_attr >> 16) & 0o170000 == 0o120000): raise ValueError('Unsupported archive entry')
            total += entry.file_size
            if entry.file_size > limits.entry_bytes or total > limits.expanded_bytes: raise ValueError('Archive exceeds expansion budget')
            with archive.open(entry) as stream:
                value = stream.read(min(entry.file_size, limits.entry_bytes)+1)
            if len(value) != entry.file_size: raise ValueError('Archive entry size mismatch')
            files[name] = value
    if files.get('mimetype', b'application/x-cymd') != b'application/x-cymd': raise ValueError('Unsupported document type')
    package = decode_markdown(files.pop('document.md'))
    package.previews = files.pop('previews.json', b'{}').decode('utf8')
    package.manifest = files.pop('manifest.json', b'').decode('utf8'); files.pop('mimetype', None)
    package.assets = files
    return package


def write_cymd(package, limits=None):
    limits = limits or DocumentIOLimits()
    files = {'mimetype': b'application/x-cymd', 'manifest.json': b'{"format":"cymd","version":1,"main":"document.md","generator":"CyUI"}',
             'document.md': package.markdown.encode('utf8'), 'previews.json': package.previews.encode('utf8')}
    if package.manifest: files['manifest.json'] = package.manifest.encode('utf8')
    for name, value in package.assets.items():
        if name.casefold() in {n.casefold() for n in files}: raise ValueError('Duplicate/reserved asset path')
        files[name] = value
    if len(files) > limits.entries: raise ValueError('Archive exceeds entry budget')
    if any(not asset_path(name) or len(data) > limits.entry_bytes for name, data in files.items()): raise ValueError('Invalid asset or byte budget')
    if sum(map(len, files.values())) > limits.expanded_bytes: raise ValueError('Archive exceeds expansion budget')
    out = BytesIO()
    with zipfile.ZipFile(out, 'w', compression=zipfile.ZIP_STORED, allowZip64=False) as archive:
        for name, value in files.items(): archive.writestr(name, value)
    if out.tell() > limits.archive_bytes: raise ValueError('Archive exceeds byte budget')
    return out.getvalue()


class _ClipboardHTML(HTMLParser):
    def __init__(self, importer):
        super().__init__(convert_charrefs=True)
        self.out, self.links, self.skip, self.lists, self.pre, self.importer = [], [], [], 0, False, importer
        self.table_header, self.table_cell, self.table_columns = False, False, 0

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if self.skip or tag in ('script', 'style', 'head', 'iframe', 'object'):
            self.skip.append(tag); return
        wrappers = {'b': '**', 'strong': '**', 'em': '*', 'i': '*', 's': '~~', 'del': '~~', 'u': '<u>', 'mark': '<mark>', 'kbd': '<kbd>'}
        if tag in wrappers: self.out.append(wrappers[tag])
        elif tag in ('p', 'div', 'section'): self.out.append(' ' if self.table_cell else '\n\n')
        elif tag == 'br': self.out.append('\n')
        elif re_heading(tag): self.out.append('\n\n'+'#'*int(tag[1])+' ')
        elif tag in ('ul', 'ol'): self.lists += 1; self.out.append('\n')
        elif tag == 'li': self.out.append('\n'+'  '*max(0, self.lists-1)+'- ')
        elif tag == 'blockquote': self.out.append('\n> ')
        elif tag == 'pre': self.pre = True; self.out.append('\n```\n')
        elif tag == 'code' and not self.pre: self.out.append('`')
        elif tag == 'table': self.out.append('\n\n'); self.table_header = False
        elif tag == 'tr': self.out.append('\n|'); self.table_columns = 0
        elif tag in ('td', 'th'): self.table_cell = True; self.table_columns += 1
        elif tag == 'a': self.links.append(attrs.get('href', '')); self.out.append('[')
        elif tag == 'img' and self.importer:
            path = self.importer(attrs.get('src', ''))
            if path: self.out.append('!['+attrs.get('alt', '')+'](<'+path+'>)')

    def handle_endtag(self, tag):
        if self.skip:
            if tag in self.skip:
                self.skip = self.skip[:self.skip.index(tag)]
            return
        wrappers = {'b': '**', 'strong': '**', 'em': '*', 'i': '*', 's': '~~', 'del': '~~', 'u': '</u>', 'mark': '</mark>', 'kbd': '</kbd>'}
        if tag in wrappers: self.out.append(wrappers[tag])
        elif tag == 'pre': self.pre = False; self.out.append('\n```\n')
        elif tag == 'code' and not self.pre: self.out.append('`')
        elif tag in ('ul', 'ol'): self.lists = max(0, self.lists-1); self.out.append('\n')
        elif tag == 'a' and self.links: self.out.append('](<'+self.links.pop()+'>)')
        elif tag in ('td', 'th'): self.table_cell = False; self.out.append(' |')
        elif tag == 'tr':
            if not self.table_header and self.table_columns:
                self.out.append('\n|'+' --- |'*self.table_columns); self.table_header = True
        elif tag == 'table': self.out.append('\n\n')
        elif tag in ('p', 'div', 'section') or re_heading(tag): self.out.append(' ' if self.table_cell else '\n\n')

    def handle_data(self, data):
        if not self.skip:
            if not self.pre:
                from .documenteditor import _escape
                data = _escape(data)
            if self.table_cell: data = data.replace('\r', ' ').replace('\n', ' ')
            self.out.append(data)


def re_heading(tag): return len(tag) == 2 and tag[0] == 'h' and tag[1] in '123456'


def markdown_from_html(value, import_image=None):
    if len(value.encode('utf8')) > 32*1024*1024: raise ValueError('HTML exceeds byte budget')
    parser = _ClipboardHTML(import_image); parser.feed(value); parser.close()
    return ''.join(parser.out).strip()


def document_html(document, title='Document'):
    def inline(runs):
        result = ''
        for run in runs:
            value = escape(run.text).replace('\n', '<br>\n')
            if run.kind == 'image': value = '<img src="'+escape(run.target, quote=True)+'" alt="'+value+'">'
            for flag, tag in ((MONO, 'code'), (BOLD, 'strong'), (ITALIC, 'em'), (UNDERLINE, 'u'), (STRIKE, 's'), (HIGHLIGHT, 'mark'), (KEYBOARD, 'kbd')):
                if run.style & flag: value = '<'+tag+'>'+value+'</'+tag+'>'
            if run.style & SPOILER: value = '<details><summary>Spoiler</summary>'+value+'</details>'
            if run.kind == 'link' and (':' not in run.target or run.target.lower().startswith(('https:', 'http:', 'mailto:'))): value = '<a href="'+escape(run.target, quote=True)+'">'+value+'</a>'
            result += value
        return result
    out = '<!doctype html><html><head><meta charset="utf-8"><title>'+escape(title)+'</title></head><body>'
    for block in document.blocks:
        if block.kind == 'code': out += '<pre><code>'+escape(block.text)+'</code></pre>'
        elif block.kind == 'rule': out += '<hr>'
        elif block.kind == 'table': out += '<table>'+''.join('<tr>'+''.join('<td>'+inline(cell)+'</td>' for cell in row)+'</tr>' for row in block.cells)+'</table>'
        else:
            tag = 'h'+str(block.level) if block.kind == 'heading' else 'p'
            out += '<'+tag+'>'+inline(block.inlines)+'</'+tag+'>'
    return out+'</body></html>'


def document_package_html(package, title='Document'):
    import base64
    import mimetypes
    from .documents import Document
    output = document_html(Document(package.markdown), title)
    for path, data in package.assets.items():
        if not asset_path(path) or len(data) > 32*1024*1024: raise ValueError('Invalid asset or byte budget')
        quoted = '"'+escape(path, quote=True)+'"'
        if quoted not in output: continue
        mime = mimetypes.guess_type(path)[0] or 'application/octet-stream'
        output = output.replace(quoted, '"data:'+mime+';base64,'+base64.b64encode(data).decode('ascii')+'"')
        if len(output.encode('utf8')) > 256*1024*1024: raise ValueError('HTML exceeds byte budget')
    return output
