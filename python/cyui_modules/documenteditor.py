"""Optional native Markdown editor with shared source/live/reading history."""
from dataclasses import dataclass, replace
import re
from .common import ui
from .documents import Document, DocumentState, document_view, DocumentOptions
from ._markdown import Inline, Block, BOLD, ITALIC, UNDERLINE, STRIKE, HIGHLIGHT, SPOILER, KEYBOARD, MONO
from ._document_ui import inline, lightbox


def _escape(text): return re.sub(r'([\\`*_{}\[\]<>#~|])', r'\\\1', text)


def _serialize(run):
    if run.kind == 'image': return '!['+_escape(run.text)+(f'|{run.width:g}' if run.width else '')+'](<'+run.target+'>)'
    if '\n' in run.text:
        return '\n'.join(_serialize(replace(run, text=line)) if line else '' for line in run.text.split('\n'))
    text = _escape(run.text)
    if run.style & MONO:
        fence = '`'
        while fence in run.text: fence += '`'
        text = fence+' '+run.text+' '+fence
    for flag, opening, closing in ((BOLD, '**', '**'), (ITALIC, '*', '*'), (UNDERLINE, '<u>', '</u>'),
                                   (STRIKE, '~~', '~~'), (HIGHLIGHT, '<mark>', '</mark>'), (KEYBOARD, '<kbd>', '</kbd>'), (SPOILER, '||', '||')):
        if run.style & flag: text = opening+text+closing
    if run.target: text = '['+text+'](<'+run.target+'>)'
    return text


class RichText:
    def __init__(self, runs=()): self.runs = list(runs)
    def text(self): return ''.join(run.text for run in self.runs)

    def markdown(self):
        runs = []
        for run in self.runs:
            if runs and (runs[-1].style, runs[-1].target, runs[-1].kind) == (run.style, run.target, run.kind) and run.kind != 'image':
                runs[-1] = replace(runs[-1], text=runs[-1].text+run.text)
            else: runs.append(run)
        return ''.join(map(_serialize, runs))

    def replace(self, first, last, text):
        raw = self.text().encode('utf8')
        if not 0 <= first <= last <= len(raw) or len(raw)-(last-first)+len(text.encode('utf8')) > 32*1024*1024: return False
        try: raw[:first].decode('utf8'); raw[:last].decode('utf8')
        except UnicodeDecodeError: return False
        before, after, inherited, at = [], [], Inline(), 0
        for run in self.runs:
            data = run.text.encode('utf8'); end = at+len(data)
            if at <= first <= end: inherited = run
            if at < first: before.append(replace(run, text=data[:min(len(data), first-at)].decode('utf8')))
            if end > last: after.append(replace(run, text=data[max(0, last-at):].decode('utf8')))
            at = end
        self.runs = before+([replace(inherited, text=text)] if text else [])+after
        return True


@dataclass
class DocumentEditorOptions:
    document: object = None
    toolbar: bool = True
    mode_selector: bool = True
    read_only: bool = False
    paste_rich: object = None


def _prefix(block):
    text = '> '*block.quote_depth+'  '*max(0, block.indent-1)
    if block.kind == 'heading': text += '#'*block.level+' '
    if block.kind == 'list': text += ('- [x] ' if block.checked else '- [ ] ') if block.task else f'{block.number}. ' if block.number else '- '
    return text


def _live_markup(block, rich):
    lines = rich.markdown().split('\n'); output = []
    listed, number = block.kind == 'list', block.number
    ordinary = _prefix(replace(block, kind='paragraph'))
    for n, line in enumerate(lines):
        if listed and not line and n < len(lines)-1: listed = False
        if n == 0 and (line or len(lines) == 1): prefix = _prefix(block)
        elif listed: prefix = _prefix(replace(block, number=number, checked=False))
        else: prefix = ordinary
        output.append(prefix+line)
        if number: number += 1
    return '\n'.join(output)


def _project(field):
    plain = field['rich'].text().encode('utf8'); projected = Document(field['canonical'])
    projection = [field['start']]*(len(plain)+1); search = last = 0
    for block in projected.blocks:
        for run in block.inlines:
            text = run.text.encode('utf8')
            if not text: continue
            at = plain.find(text, search)
            if at < 0: continue
            for n in range(search, at): projection[n] = field['start']+min(len(field['canonical'].encode('utf8')), last+n-search)
            for n in range(len(text)): projection[at+n] = field['start']+run.source_begin+min(n, run.source_end-run.source_begin)
            search, last = at+len(text), run.source_end
            projection[search] = field['start']+last
    for n in range(search, len(plain)): projection[n] = field['start']+min(len(field['canonical'].encode('utf8')), last+n-search)
    projection[-1] = field['start']+len(field['canonical'].encode('utf8'))
    field['projection'], field['blocks'] = projection, len(projected.blocks)


def _reload_input(identifier):
    i, _ = ui()
    context = i.get_current_context()
    if context.input_text_deactivated_state.id_ == identifier: context.input_text_deactivated_state.id_ = 0
    active = i.internal.get_input_text_state(identifier)
    if active:
        if context.active_id == identifier: active.reload_user_buf_and_keep_selection()
        else: active.id_ = 0; active.want_reload_user_buf = False


def _decorate(identifier, rich):
    i, c = ui()
    parent = i.internal.get_current_window()
    child = next((w for w in parent.dc.child_windows if w.child_id == identifier), None)
    if child is None: return
    plain, font, size = rich.text(), i.get_font(), i.get_font_size()
    active = i.internal.get_input_text_state(identifier)
    wrap = active.wrap_width if active else max(c.dp(1), child.inner_rect.get_width()-i.get_style().frame_padding.x*2-i.get_style().scrollbar_size)
    start_x = child.dc.cursor_start_pos.x+i.get_style().frame_padding.x
    x, y = start_x, child.dc.cursor_start_pos.y+i.get_style().frame_padding.y
    draw = child.draw_list
    draw.push_clip_rect(child.inner_clip_rect.min, child.inner_clip_rect.max, True)
    styles = [(run.style, run.target) for run in rich.runs for _ in run.text]
    cursor = len(plain.encode('utf8')[:active.get_cursor_pos()].decode('utf8', errors='ignore')) if active else -1
    line_end = 0
    for at, char in enumerate(plain):
        if at >= line_end:
            if at: x, y = start_x, y+size
            tail = i.internal.im_font_calc_word_wrap_position_ex(font, size, plain[at:], wrap, i.internal.ImDrawTextFlags_.wrap_keep_blanks)
            line_end = at+max(1, len(plain[at:])-len(tail))
            if char == '\n': continue
        style, target = styles[at]
        width = i.calc_text_size(char).x
        if style & HIGHLIGHT: draw.add_rect_filled((x, y), (x+width, y+size), c.alpha(c.palette().warning, .25))
        color = c.palette().accent if target else c.palette().text
        vertex = len(draw.vtx_buffer)
        draw.add_text((c.bold_font() or font) if style & BOLD else font, size, (x, y), color, char)
        if style & ITALIC:
            for v in range(vertex, len(draw.vtx_buffer)):
                point = draw.vtx_buffer[v].pos
                draw.vtx_buffer[v].pos = (point.x+(y+size-point.y)*.14, point.y)
        if style & UNDERLINE or target: draw.add_line((x, y+size), (x+width, y+size), color, c.dp(1))
        if style & STRIKE: draw.add_line((x, y+size*.55), (x+width, y+size*.55), color, c.dp(1))
        if at == cursor and i.is_item_active(): draw.add_line((x, y), (x, y+size), c.palette().text, c.dp(1))
        x += width
    if cursor == len(plain) and i.is_item_active(): draw.add_line((x, y), (x, y+size), c.palette().text, c.dp(1))
    draw.pop_clip_rect()


class DocumentEditor:
    def __init__(self, markdown=''):
        self.mode, self.read_only, self.selection = 'live', False, (0, 0)
        self.document, self.view = Document(markdown), DocumentState()
        self._text, self._undo, self._redo, self._changed = markdown, [], [], False
        self._crlf = '\r\n' in markdown
        self._reload, self._active_input = False, 0
        self._fields, self._revision = {}, 0

    def set_markdown(self, text):
        self.document.set_markdown(text); self._text = text; self._crlf = '\r\n' in text; self._reload = True
        self._undo.clear(); self._redo.clear(); self.selection = (0, 0)
        self._fields.clear(); self._revision += 1

    def markdown(self): return self._text
    def can_undo(self): return bool(self._undo) and not self.read_only
    def can_redo(self): return bool(self._redo) and not self.read_only

    def select(self, first, last):
        raw = self._text.encode('utf8')
        if not 0 <= first <= len(raw) or not 0 <= last <= len(raw): return False
        try: raw[:first].decode('utf8'); raw[:last].decode('utf8')
        except UnicodeDecodeError: return False
        self.selection = first, last
        return True

    def _replace(self, first, last, text, history=True):
        if self.read_only: return False
        if self._crlf: text = text.replace('\r\n', '\n').replace('\n', '\r\n')
        raw, insert = self._text.encode('utf8'), text.encode('utf8')
        if not 0 <= first <= last <= len(raw): return False
        if len(raw)-(last-first)+len(insert) > 32*1024*1024: raise ValueError('Document exceeds 32 MiB')
        before, removed, after = raw[:first].decode('utf8'), raw[first:last].decode('utf8'), raw[last:].decode('utf8')
        if history:
            self._undo.append((first, removed, text, self.selection)); self._redo.clear()
            while len(self._undo) > 256 or sum(len(p[1].encode('utf8'))+len(p[2].encode('utf8')) for p in self._undo) > 16*1024*1024: self._undo.pop(0)
        self._text = before+text+after; self.selection = first, first+len(insert); self._changed = True
        self._reload = True
        self._revision += 1
        self.document.set_markdown(self._text)
        return True

    def insert(self, text): return self._replace(*sorted(self.selection), text)

    def command(self, command, argument=''):
        if self.read_only: return False
        if command in ('undo', 'redo'):
            source, target = (self._undo, self._redo) if command == 'undo' else (self._redo, self._undo)
            if not source: return False
            first, removed, inserted, selection = source.pop()
            old, new = (inserted, removed) if command == 'undo' else (removed, inserted)
            self._replace(first, first+len(old.encode('utf8')), new, False); target.append((first, removed, inserted, selection))
            if command == 'undo': self.selection = selection
            return True
        marks = {'bold': ('**', '**'), 'italic': ('*', '*'), 'underline': ('<u>', '</u>'), 'strike': ('~~', '~~'),
                 'spoiler': ('||', '||'), 'code': ('`', '`'), 'link': ('[', '](<'+argument+'>)')}
        first, last = sorted(self.selection); raw = self._text.encode('utf8')
        flags = {'bold': BOLD, 'italic': ITALIC, 'underline': UNDERLINE, 'strike': STRIKE, 'spoiler': SPOILER, 'code': MONO}
        if self.mode == 'live' and first != last and command in marks:
            self.document.set_markdown(self._text)
            for block in self.document.blocks:
                if first < block.source_begin or last > block.source_end or block.kind in ('code', 'table'): continue
                affected = [r for r in block.inlines if r.source_end > first and r.source_begin < last]
                if not affected: break
                flag = flags.get(command, 0)
                remove = bool(flag) and all(r.style & flag for r in affected)
                runs = []
                for run in block.inlines:
                    if run not in affected: runs.append(run); continue
                    data = run.text.encode('utf8')
                    exact = run.source_end-run.source_begin == len(data)
                    a = max(0, first-run.source_begin) if exact else 0
                    b = min(len(data), last-run.source_begin) if exact else len(data)
                    if a: runs.append(replace(run, text=data[:a].decode('utf8')))
                    selected = replace(run, text=data[a:b].decode('utf8'))
                    if flag: selected.style = run.style & ~flag if remove else run.style | flag
                    else: selected.target = argument
                    runs.append(selected)
                    if b < len(data): runs.append(replace(run, text=data[b:].decode('utf8')))
                end = block.source_end
                while end > block.source_begin and raw[end-1:end] in (b'\n', b'\r'): end -= 1
                prefix = '> '*block.quote_depth+'  '*max(0, block.indent-1)
                if block.kind == 'heading': prefix += '#'*block.level+' '
                if block.kind == 'list': prefix += ('- [x] ' if block.checked else '- [ ] ') if block.task else f'{block.number}. ' if block.number else '- '
                return self._replace(block.source_begin, end, prefix+RichText(runs).markdown())
        if command in marks:
            opening, closing = marks[command]; selected = raw[first:last].decode('utf8')
            self._replace(first, last, opening+selected+closing)
            self.selection = first+len(opening.encode('utf8')), first+len(opening.encode('utf8'))+len(selected.encode('utf8'))
            return True
        prefixes = {'heading1': '# ', 'heading2': '## ', 'heading3': '### ', 'bullet': '- ', 'numbered': '1. ', 'task': '- [ ] ', 'quote': '> ', 'indent': '  ', 'outdent': ''}
        if command not in prefixes: return False
        first = raw.rfind(b'\n', 0, first)+1; end = raw.find(b'\n', last); last = len(raw) if end < 0 else end
        value = raw[first:last].decode('utf8')
        value = '\n'.join(line[2:] if command == 'outdent' and line.startswith('  ') else prefixes[command]+line for line in value.split('\n'))
        return self._replace(first, last, value)

    def draw(self, identifier, size=(0, 480), options=None):
        i, c = ui(); options = options or DocumentEditorOptions(); doc_options = replace(options.document or DocumentOptions(), tasks=not options.read_only)
        self.read_only = options.read_only; self.document.set_markdown(self._text)
        i.push_id(identifier); i.begin_child('##editor', size)
        if options.mode_selector:
            modes = ['live', 'split', 'source', 'reading']; labels = [c.tr(s) for s in ('Live', 'Side by side', 'Source', 'Reading')]
            i.set_next_item_width(c.dp(170)); changed, index = i.combo('##mode', modes.index(self.mode), labels)
            if changed: self.mode = modes[index]
        if options.toolbar:
            i.begin_disabled(self.read_only)
            for n, (label, command) in enumerate((('Bold', 'bold'), ('Italic', 'italic'), ('Underline', 'underline'), ('Spoiler', 'spoiler'), ('Task list', 'task'), ('Undo', 'undo'), ('Redo', 'redo'))):
                if n: i.same_line()
                if c.ghost_button(c.tr_label(label)): self.command(command)
            i.end_disabled()
        if i.is_window_focused(i.FocusedFlags_.root_and_child_windows):
            io = i.get_io()
            if io.key_ctrl or io.key_super:
                if not self.read_only and i.is_key_pressed(i.Key.v) and not io.key_shift:
                    rich = options.paste_rich() if options.paste_rich else ''
                    plain = i.get_clipboard_text() or ''
                    if rich:
                        i.internal.set_key_owner(i.Key.v, i.get_id('##paste'), i.internal.InputFlagsPrivate_.lock_this_frame)
                        self.insert(rich)
                    elif self.selection[0] != self.selection[1] and re.fullmatch(r'https?://[^\s<>]+', plain):
                        i.internal.set_key_owner(i.Key.v, i.get_id('##paste'), i.internal.InputFlagsPrivate_.lock_this_frame)
                        self.command('link', plain)
                for key, command in ((i.Key.b, 'bold'), (i.Key.i, 'italic'), (i.Key.u, 'underline'), (i.Key.e, 'code')):
                    if i.is_key_pressed(key): self.command(command)
                if i.is_key_pressed(i.Key.z): self.command('redo' if io.key_shift else 'undo')
                if i.is_key_pressed(i.Key.y): self.command('redo')
                if io.key_shift:
                    for key, command in ((i.Key.x, 'strike'), (i.Key._1, 'heading1'), (i.Key._2, 'heading2'), (i.Key._3, 'heading3'), (i.Key._7, 'numbered'), (i.Key._8, 'bullet'), (i.Key._9, 'task'), (i.Key.period, 'quote')):
                        if i.is_key_pressed(key): self.command(command)
                else:
                    for key, mode in ((i.Key._1, 'live'), (i.Key._2, 'split'), (i.Key._3, 'source'), (i.Key._4, 'reading')):
                        if i.is_key_pressed(key): self.mode = mode
        result = {'changed': False, 'document': {}}
        flags = i.InputTextFlags_.no_undo_redo | (i.InputTextFlags_.read_only if self.read_only else 0)
        if hasattr(i.InputTextFlags_, 'word_wrap'): flags |= i.InputTextFlags_.word_wrap
        if self.mode == 'reading': result['document'] = document_view('##read', self.view, self.document, tuple(i.get_content_region_avail()), doc_options)
        elif self.mode == 'live':
            i.begin_child('##live', tuple(i.get_content_region_avail()))
            edits, pending, index = [], None, 0
            raw = self._text.encode('utf8')
            for n in range(len(self.document.blocks)+2):
                i.push_id(n)
                input_id = i.get_id('##rich')
                field = self._fields.get(n)
                cached = field is not None and i.get_current_context().active_id == input_id and field['revision'] == self._revision and raw[field['start']:field['start']+len(field['canonical'].encode('utf8'))] == field['canonical'].encode('utf8')
                if field and not cached:
                    _reload_input(input_id); self._fields.pop(n, None)
                if index >= len(self.document.blocks) and not cached and not (not self.document.blocks and n == 0):
                    i.pop_id(); break
                block = field['block'] if cached else self.document.blocks[index] if index < len(self.document.blocks) else Block('paragraph')
                consumed = field['blocks'] if cached else int(index < len(self.document.blocks))
                special = block.kind in ('table', 'code', 'rule') or any(r.kind == 'image' for r in block.inlines)
                if special:
                    from ._document_ui import content
                    class One: blocks = [block]
                    One.text = self._text
                    content(self.view, One, doc_options)
                else:
                    rich = RichText(field['rich'].runs) if cached else RichText(block.inlines)
                    plain = rich.text(); selected = [0, 0]
                    if block.task:
                        i.begin_disabled(self.read_only)
                        changed, checked = i.checkbox('##task', block.checked)
                        i.end_disabled()
                        if changed: edits.append((block.task_offset, block.task_offset+1, 'x' if checked else ' '))
                        i.same_line()
                    elif block.kind == 'list': i.bullet(); i.same_line()
                    def capture(data):
                        def source_offset(position):
                            if cached: return field['projection'][min(position, len(field['projection'])-1)]
                            at = 0
                            for run in rich.runs:
                                length = len(run.text.encode('utf8'))
                                if position <= at+length: return run.source_begin+min(position-at, run.source_end-run.source_begin)
                                at += length
                            return block.source_end
                        selected[:] = data.selection_start, data.selection_end
                        self.selection = source_offset(data.selection_start), source_offset(data.selection_end)
                        return 0
                    width = max(c.dp(40), i.get_content_region_avail().x)
                    height = max(i.get_text_line_height()+c.dp(12), min(c.dp(900), i.calc_text_size(plain, wrap_width=max(c.dp(1), width-c.dp(32))).y+c.dp(16)))
                    if self._reload and input_id == self._active_input:
                        _reload_input(input_id)
                        self._reload = False
                    i.push_style_color(i.Col_.text, c.alpha(c.palette().text, 0))
                    changed, value = i.input_text_multiline('##rich', plain, (width, height), flags | i.InputTextFlags_.callback_always, capture)
                    i.pop_style_color()
                    if i.is_item_active(): self._active_input = input_id
                    if changed:
                        a = 0
                        while a < min(len(plain), len(value)) and plain[a] == value[a]: a += 1
                        z = 0
                        while z < len(plain)-a and z < len(value)-a and plain[-1-z] == value[-1-z]: z += 1
                        rich.replace(len(plain[:a].encode('utf8')), len(plain[:len(plain)-z].encode('utf8')), value[a:len(value)-z])
                        start = field['start'] if cached else block.source_begin
                        end = start+len(field['canonical'].encode('utf8')) if cached else block.source_end
                        while not cached and end > start and raw[end-1:end] in (b'\n', b'\r'): end -= 1
                        canonical = _live_markup(block, rich)
                        if self._crlf: canonical = canonical.replace('\r\n', '\n').replace('\n', '\r\n')
                        pending = (n, {'block': block, 'rich': rich, 'canonical': canonical, 'start': start, 'selection': selected})
                        edits.append((start, end, canonical))
                    _decorate(input_id, rich)
                i.pop_id()
                index += consumed
            if not self.document.blocks and c.ghost_button(c.tr_label('Start writing')): self.insert(c.tr('New paragraph'))
            i.end_child()
            for edit in reversed(edits): self._replace(*edit)
            if pending:
                n, field = pending
                field['revision'] = self._revision; _project(field); self._fields[n] = field
                self.selection = tuple(field['projection'][min(p, len(field['projection'])-1)] for p in field['selection'])
                self._reload = False
            lightbox(self.view, doc_options)
        else:
            room = i.get_content_region_avail(); split = self.mode == 'split'
            def capture(data): self.selection = data.selection_start, data.selection_end; return 0
            input_id = i.get_id('##source')
            if self._reload and input_id == self._active_input:
                _reload_input(input_id)
                self._reload = False
            changed, text = i.input_text_multiline('##source', self._text, (room.x*.5 if split else room.x, room.y), flags | i.InputTextFlags_.callback_always, capture)
            if i.is_item_active(): self._active_input = input_id
            if changed: self._replace(0, len(self._text.encode('utf8')), text)
            if split:
                i.same_line(); result['document'] = document_view('##preview', self.view, self.document, (max(c.dp(1), room.x*.5-c.dp(8)), room.y), doc_options)
        task = result['document'].get('task_offset')
        if task is not None: self._replace(task, task+1, 'x' if result['document']['task_checked'] else ' ')
        result['changed'], self._changed = self._changed, False
        i.end_child(); i.pop_id()
        return result
