"""Native CommonMark/GFM documents and cached message transcripts."""
from dataclasses import dataclass, field
import re
from .common import ui, item_id
from .code import CodeDocument, CodeViewState, code_viewer


from ._markdown import Block, Inline, parse, BOLD, ITALIC, UNDERLINE, STRIKE, HIGHLIGHT, SPOILER, KEYBOARD, MONO


class Document:
    def __init__(self, text=''):
        self.text, self.blocks, self.revision = None, [], 0
        self.set_markdown(text)

    def set_markdown(self, text):
        if text == self.text:
            return False
        if len(text.encode('utf8')) > 32*1024*1024:
            raise ValueError('Document exceeds 32 MiB')
        blocks = parse(text)
        self.text, self.blocks = text, blocks
        self.revision += 1
        return True

    def plain_text(self):
        return '\n'.join('\n'.join('\t'.join(row) for row in block.rows) if block.kind == 'table' else block.text for block in self.blocks)


@dataclass
class DocumentState:
    code_views: dict = field(default_factory=dict)
    spoilers: set = field(default_factory=set)
    active_embeds: set = field(default_factory=set)
    anchors: dict = field(default_factory=dict)
    jump_to: str = ''
    enlarged_image: str = ''
    enlarged_caption: str = ''
    image_view: object = None
    revision: int = 0
    scroll: float = 0
    scroll_max: float = 0


# Explicit tokens plus words allow wrapping at each word, including links. The group that
# matched says what a token is: an unmatched '*', '**' or '`' is part of a plain word and is
# drawn as written (*args, **kwargs, a lone backtick).
_INLINE = re.compile(r'(?P<link>\[(?P<label>[^\]]+)\]\((?P<url>[^)]+)\))|(?P<mono>`[^`]+`)'
                     r'|(?P<strong>\*\*[^*\s](?:[^*]*[^*\s])?\*\*)|(?P<emphasis>\*[^*\s](?:[^*]*[^*\s])?\*)|(?P<word>\S+\s*)')


def _inline(text):
    i, c = ui()
    link, first = '', True
    tokens = _INLINE.finditer(text)
    right = i.get_cursor_screen_pos().x+i.get_content_region_avail().x
    previous_end = 0
    for token_match in tokens:
        gap = i.calc_text_size(" ").x if token_match.start() > previous_end else 0
        previous_end = token_match.end()
        token, kind = token_match.group(), token_match.lastgroup
        i.push_id(token_match.start())
        match = token_match if kind == 'link' else None
        mono, strong, emphasis = kind == 'mono', kind == 'strong', kind == 'emphasis'
        label = match['label'] if match else token[1:-1] if mono or emphasis else token[2:-2] if strong else token
        font = c.mono_font() if mono else c.bold_font() if strong else None
        if font:
            i.push_font(font, 0.)
        width = i.calc_text_size(label).x
        if not first:
            if i.get_item_rect_max().x+width+gap < right:
                i.same_line(0, gap)
        if match:
            if c.link_text(label):
                link = match['url']
        elif mono:
            c.text_coloured(c.palette().accent, label)
        elif emphasis:
            c.text_coloured(c.palette().textDim, label)
        else:
            i.text_unformatted(label)
        if font:
            i.pop_font()
        first = False
        i.pop_id()
    return link


from ._document_ui import DocumentServices, DocumentOptions, DocumentMedia, LinkPreview
from ._document_ui import content as _content, lightbox as _lightbox


def document_view(identifier, state, document, size=(0, 320), options=None):
    i, _ = ui()
    options = options or DocumentOptions()
    i.push_id(identifier)
    i.begin_child('##document', size)
    if state.revision != document.revision:
        state.spoilers.clear(); state.active_embeds.clear(); state.revision = document.revision
    state.anchors.clear()
    result = _content(state, document, options)
    if state.jump_to:
        if state.jump_to in state.anchors: i.set_scroll_y(state.anchors[state.jump_to])
        state.jump_to = ''
    state.scroll, state.scroll_max = i.get_scroll_y(), i.get_scroll_max_y()
    i.end_child()
    _lightbox(state, options)
    i.pop_id()
    return result


@dataclass
class TranscriptMessage:
    id: int
    role: str
    content: str
    time: str = ''


@dataclass
class TranscriptState:
    documents: dict = field(default_factory=dict)
    views: dict = field(default_factory=dict)
    follow: bool = True
    count: int = 0


def transcript(identifier, state, messages, size=(0, 320), options=None):
    i, c = ui()
    result, changed = {'copy': 0, 'link': ''}, False
    i.push_id(identifier)
    i.begin_child('##transcript', size)
    bottom = i.get_scroll_y() >= i.get_scroll_max_y()-c.dp(3)
    present = {m.id for m in messages}
    for message in messages:
        doc = state.documents.setdefault(message.id, Document())
        changed |= doc.set_markdown(message.content)
        view = state.views.setdefault(message.id, DocumentState())
        i.push_id(item_id(message.id))
        c.heading(message.role, 3); i.same_line(); c.text_faint(message.time)
        if c.ghost_button(c.tr_label('Copy message')):
            i.set_clipboard_text(message.content)
            result['copy'] = message.id
        result['link'] = _content(view, doc, options)['link'] or result['link']
        c.divider()
        i.pop_id()
    if state.follow and bottom and (changed or state.count != len(messages)):
        i.set_scroll_here_y(1.)
    state.count = len(messages)
    for key in set(state.documents)-present:
        del state.documents[key]
        state.views.pop(key, None)
    i.end_child()
    i.pop_id()
    return result
