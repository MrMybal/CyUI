"""Bounded thread-safe logs. Drawing stays on the UI thread."""
from dataclasses import dataclass, field
from collections import deque
from itertools import count
from math import isfinite
from threading import Lock
from .common import ui, item_id
from .filters import TextFilter, filter_box

# Revisions are unique per buffer (instance << 40, as in C++), so a LogState moved to a
# recreated buffer never mistakes its revision for the new buffer's and keeps stale rows.
_instances = count(1)


@dataclass(frozen=True)
class LogEntry:
    id: int
    level: str
    text: str
    source: str = ''


class LogBuffer:
    def __init__(self, capacity=10000, max_bytes=8*1024*1024):
        self.capacity, self.max_bytes = max(1, capacity), max(1, max_bytes)
        self._rows, self._lock, self._bytes, self._next, self._revision = deque(), Lock(), 0, 1, (next(_instances) << 40) | 1

    def append(self, level, text, source=''):
        lines = text.split('\n')
        if len(lines) > 1 and lines[-1] == '':
            lines.pop()
        with self._lock:
            for line in lines:
                # Bound each entry as well as the queue on Unicode boundaries.
                value = line.encode('utf8')[:self.max_bytes].decode('utf8', 'ignore')
                origin = source.encode('utf8')[:max(0, self.max_bytes-len(value.encode('utf8')))].decode('utf8', 'ignore')
                self._rows.append(LogEntry(self._next, level, value, origin))
                self._next += 1
                self._bytes += len((value+origin).encode('utf8'))
                while len(self._rows) > self.capacity or self._bytes > self.max_bytes:
                    old = self._rows.popleft()
                    self._bytes -= len((old.text+old.source).encode('utf8'))
            self._revision += 1

    def clear(self):
        with self._lock:
            self._rows.clear()
            self._bytes = 0
            self._revision += 1

    def snapshot(self, known_revision=0):
        with self._lock:
            return self._revision, list(self._rows) if known_revision != self._revision else None


@dataclass
class LogState:
    query: str = ''
    rows: list = field(default_factory=list)
    revision: int = 0
    follow: bool = True
    levels: set = field(default_factory=lambda: {'debug', 'info', 'warning', 'error'})
    selected: int = 0


def level_color(level):
    _, c = ui()
    return getattr(c.palette(), {'error': 'error', 'warning': 'warning', 'debug': 'textFaint'}.get(level, 'text'))


def log_view(identifier, state, buffer, size=(0, 320)):
    i, c = ui()
    revision, rows = buffer.snapshot(state.revision)
    changed = revision != state.revision
    state.revision = revision
    if rows is not None:
        state.rows = rows
    i.push_id(identifier)
    i.set_next_item_width(c.dp(180))
    _, state.query = i.input_text('##filter', state.query)
    i.same_line()
    _, state.follow = i.checkbox(c.tr_label('Follow'), state.follow)
    i.same_line()
    if i.button(c.tr_label('Copy visible')):
        i.set_clipboard_text('\n'.join(e.text for e in state.rows if e.level in state.levels and state.query.casefold() in (e.text+' '+e.source).casefold()))
    for level in ('debug', 'info', 'warning', 'error'):
        if level != 'debug':
            i.same_line()
        toggle, on = i.checkbox(c.tr_label(level), level in state.levels)
        if toggle:
            state.levels.add(level) if on else state.levels.discard(level)
    i.begin_child('##log', (size[0], max(c.dp(1), size[1]-2*i.get_frame_height_with_spacing()) if size[1] else 0), 0, i.WindowFlags_.horizontal_scrollbar)
    bottom = i.get_scroll_y() >= i.get_scroll_max_y()-c.dp(3)
    filtered = [e for e in state.rows if e.level in state.levels and state.query.casefold() in (e.text+' '+e.source).casefold()]
    request = 0
    clip = i.ListClipper(); clip.begin(len(filtered))
    while clip.step():
        for index in range(clip.display_start, clip.display_end):
            entry = filtered[index]
            i.push_id(item_id(entry.id))
            pos = tuple(i.get_cursor_screen_pos())
            label = (entry.source+': ' if entry.source else '')+entry.text.replace('\n', ' ↵ ')
            clicked, _ = i.selectable('##row', state.selected == entry.id, 0, (max(i.get_content_region_avail().x, i.calc_text_size(label).x), i.get_text_line_height_with_spacing()))
            i.get_window_draw_list().add_text(pos, level_color(entry.level), label)
            if clicked:
                request = state.selected = entry.id
            i.pop_id()
    if state.follow and bottom and changed:
        i.set_scroll_here_y(1.)
    i.end_child()
    i.pop_id()
    return request


@dataclass
class Diagnostic:
    id: int
    message: str
    source: str = ''
    line: int = 0
    column: int = 0
    level: str = 'error'


def diagnostic_list(identifier, diagnostics, size=(0, 200)):
    i, c = ui()
    request = 0
    i.push_id(identifier)
    i.begin_child('##diagnostics', size)
    clip = i.ListClipper(); clip.begin(len(diagnostics))
    while clip.step():
        for n in range(clip.display_start, clip.display_end):
            entry = diagnostics[n]
            text = f'{entry.source}:{entry.line}:{entry.column}  {entry.message}'
            pos = tuple(i.get_cursor_screen_pos())
            clicked, _ = i.selectable(item_id(entry.id), False, 0, (0, i.get_text_line_height_with_spacing()))
            c.clipped_text(i.get_window_draw_list(), pos, i.get_content_region_avail().x, level_color(entry.level), text)
            if clicked:
                request = entry.id
    i.end_child()
    i.pop_id()
    return request


@dataclass
class TaskRowData:
    id: int
    title: str
    detail: str = ''
    status: str = 'queued'
    progress: float | None = None
    can_cancel: bool = False
    log: LogBuffer | None = None


@dataclass
class TaskPanelState:
    filter: TextFilter = field(default_factory=TextFilter)
    selected: int | None = None
    log: LogState = field(default_factory=LogState)


_TASK_LABELS = {'queued': 'Queued', 'running': 'Running', 'succeeded': 'Succeeded',
                'warning': 'Warning', 'failed': 'Failed', 'canceled': 'Canceled'}


def _task_color(palette, status):
    return getattr(palette, {'running': 'info', 'succeeded': 'success',
                             'warning': 'warning', 'failed': 'error'}.get(status, 'textDim'))


def task_row(identifier, item, selected=False, size=(0, 0)):
    i, c = ui()
    palette = c.palette()
    color = _task_color(palette, item.status)
    width = max(c.dp(1), size[0] or i.get_content_region_avail().x)
    line = i.get_text_line_height_with_spacing()
    height, pad = size[1] or line * 2 + c.dp(12), c.dp(8)
    x, y = i.get_cursor_screen_pos()
    with c.scoped_id(identifier):
        clicked, _ = i.selectable(item_id(item.id), selected, 0, (width, height))
    status = c.tr(_TASK_LABELS.get(item.status, 'Queued'))
    status_width = i.calc_text_size(status).x
    left = x + pad + c.dp(13)
    draw = i.get_window_draw_list()
    draw.push_clip_rect((x, y), (x + width, y + height), True)
    try:
        draw.add_circle_filled((x + pad, y + pad + i.get_text_line_height() * .5), c.dp(3), color)
        c.clipped_text(draw, (left, y + pad), max(0., x + width - left - status_width - pad * 2), palette.text, item.title)
        if width > status_width + pad * 3:
            draw.add_text((x + width - status_width - pad, y + pad), color, status)
        c.clipped_text(draw, (left, y + pad + line), max(0., x + width - left - pad), palette.textDim, item.detail)
        if (item.status == 'running' or item.progress is not None) and width > pad * 2:
            start = x + pad
            span = width - pad * 2
            top = y + height - c.dp(3)
            bottom = y + height - c.dp(1)
            draw.add_rect_filled((start, top), (start + span, bottom), palette.bg4)
            known = item.progress is not None and isfinite(item.progress)
            low = 0. if known else (i.get_time() * .65 % 1.) * .75
            high = max(0., min(1., item.progress)) if known else low + .25
            if high > low:
                draw.add_rect_filled((start + low * span, top), (start + high * span, bottom), color)
    finally:
        draw.pop_clip_rect()
    return clicked


def task_panel(identifier, state, rows, size=(0, 380)):
    i, c = ui()
    previous = state.selected
    available = i.get_content_region_avail()
    width, height = (max(c.dp(1), value if value > 0 else available[n] + value) for n, value in enumerate(size))
    with c.scoped_id(identifier):
        filter_box('##tasks-filter', state.filter, width=width)
        matches = []
        active = None
        for item in rows:
            if item.id == state.selected:
                active = item
            if state.filter.matches(item.title, item.detail):
                matches.append(item)
        if active is None:
            state.selected = None
        row_height = i.get_text_line_height_with_spacing() * 2 + c.dp(12)
        list_height = max(row_height * 2, height * .46)
        if i.begin_child('##tasks', (width, list_height), i.ChildFlags_.borders):
            if not matches:
                c.text_dim(c.tr('No tasks to display'))
            clip = i.ListClipper()
            clip.begin(len(matches), row_height + i.get_style().item_spacing.y)
            while clip.step():
                for index in range(clip.display_start, clip.display_end):
                    item = matches[index]
                    if task_row('##task-row', item, item.id == state.selected):
                        active, state.selected = item, item.id
        i.end_child()
        changed = previous != state.selected
        if changed:
            state.log = LogState()
        cancel = None
        if active:
            c.section_title(active.title)
            c.text_coloured(_task_color(c.palette(), active.status), c.tr(_TASK_LABELS.get(active.status, 'Queued')))
            if active.can_cancel:
                i.same_line()
                if c.button(c.tr_label('Cancel task'), 'danger'):
                    cancel = active.id
            if active.detail:
                c.text_dim(active.detail)
            if active.log is not None:
                log_view('##task-log', state.log, active.log, (width, max(c.dp(70), height - list_height - c.dp(90))))
    return {'selected': state.selected, 'cancel': cancel, 'selection_changed': changed}
