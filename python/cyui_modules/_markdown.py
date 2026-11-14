"""CommonMark/GFM models, independent of the UI and browser engines."""
from dataclasses import dataclass, field
import html
import re
from .code import CodeDocument

BOLD, ITALIC, UNDERLINE, STRIKE, HIGHLIGHT, SPOILER, KEYBOARD, MONO = (1 << n for n in range(8))


@dataclass
class Inline:
    kind: str = 'text'
    text: str = ''
    target: str = ''
    style: int = 0
    source_begin: int = 0
    source_end: int = 0
    spoiler: int = 0
    width: float = 0
    height: float = 0


@dataclass
class Block:
    kind: str
    text: str = ''
    level: int = 0
    language: str = ''
    code: object = None
    rows: list = field(default_factory=list)
    inlines: list = field(default_factory=list)
    cells: list = field(default_factory=list)
    alignments: list = field(default_factory=list)
    indent: int = 0
    quote_depth: int = 0
    number: int = 0
    task: bool = False
    checked: bool = False
    task_offset: int = 0
    source_begin: int = 0
    source_end: int = 0
    anchor: str = ''


def parser():
    from markdown_it import MarkdownIt
    md = MarkdownIt('commonmark', {'html': True, 'breaks': True, 'linkify': True, 'maxNesting': 256})
    md.enable(['table', 'strikethrough', 'linkify'])
    md.core.ruler.disable('text_join')
    md.linkify.set({'fuzzy_link': False, 'fuzzy_email': False})
    base_terminators = getattr(md.inline, 'terminator_re', None)
    if base_terminators is not None:
        terminators = re.compile('(?:'+base_terminators.pattern+'|[|=])')
    else:
        # markdown-it-py 4.0 predates the parser's public terminator expression.
        from markdown_it.rules_inline.text import _TerminatorChars
        terminators = re.compile('['+re.escape(''.join(_TerminatorChars | {'|', '='}))+']')

    def text_rule(state, silent):
        match = terminators.search(state.src, state.pos, state.posMax)
        end = match.start() if match else state.posMax
        if end == state.pos:
            return False
        if not silent:
            state.pending += state.src[state.pos:end]
        state.pos = end
        return True

    def pair_rule(state, silent):
        delimiter = state.src[state.pos:state.pos+2]
        if delimiter not in ('||', '=='):
            return False
        end = state.src.find(delimiter, state.pos+2, state.posMax)
        if end < 0 or not state.src[state.pos+2:end].strip():
            return False
        if not silent:
            name = 'spoiler' if delimiter == '||' else 'mark'
            state.push(name+'_open', 'span', 1)
            saved = state.posMax
            state.pos, state.posMax = state.pos+2, end
            state.md.inline.tokenize(state)
            state.posMax = saved
            state.push(name+'_close', 'span', -1)
        state.pos = end+2
        return True

    md.inline.ruler.at('text', text_rule)
    md.inline.ruler.before('emphasis', 'cyui_pairs', pair_rule)
    return md


def parse(source):
    md = parser()
    tokens = md.parse(source)
    blocks, lists, slugs = [], [], {}
    quote_depth, block, table, row, cell = 0, None, None, None, None
    lines = source.splitlines(keepends=True)
    starts, position = [], 0
    for line in lines:
        starts.append(position)
        position += len(line.encode('utf8'))
    spoiler_id = 0
    styles = {'strong': BOLD, 'em': ITALIC, 's': STRIKE, 'spoiler': SPOILER, 'mark': HIGHLIGHT}
    tags = {'u': UNDERLINE, 'mark': HIGHLIGHT, 'kbd': KEYBOARD, 'b': BOLD, 'strong': BOLD,
            'i': ITALIC, 'em': ITALIC, 's': STRIKE, 'del': STRIKE}
    for token in tokens:
        kind = token.type
        if kind == 'blockquote_open': quote_depth += 1
        elif kind == 'blockquote_close': quote_depth -= 1
        elif kind in ('bullet_list_open', 'ordered_list_open'):
            lists.append({'number': int(token.attrGet('start') or 1) if kind == 'ordered_list_open' else 0, 'first': True})
        elif kind in ('bullet_list_close', 'ordered_list_close'): lists.pop()
        elif kind == 'list_item_open': lists[-1]['first'] = True
        elif kind == 'list_item_close':
            if lists[-1]['number']: lists[-1]['number'] += 1
        elif kind == 'table_open':
            table = Block('table'); blocks.append(table)
        elif kind == 'table_close': table = None
        elif kind == 'tr_open':
            row = []; table.cells.append(row); table.rows.append([])
        elif kind in ('th_open', 'td_open'):
            cell = []; row.append(cell)
            if kind == 'th_open':
                align = token.attrGet('style') or ''
                table.alignments.append(3 if 'right' in align else 2 if 'center' in align else 1 if 'left' in align else 0)
        elif kind in ('th_close', 'td_close'): cell = None
        elif kind in ('heading_open', 'paragraph_open'):
            if table is not None: continue
            is_item = bool(lists and lists[-1]['first'])
            block = Block('heading' if kind == 'heading_open' else 'list' if is_item else 'quote' if quote_depth else 'paragraph',
                          level=int(token.tag[1:]) if kind == 'heading_open' else 0, indent=len(lists), quote_depth=quote_depth)
            if is_item: block.number = lists[-1]['number']; lists[-1]['first'] = False
            if token.map:
                block.source_begin = starts[token.map[0]] if token.map[0] < len(starts) else position
                block.source_end = starts[token.map[1]] if token.map[1] < len(starts) else position
            blocks.append(block)
        elif kind in ('fence', 'code_block'):
            language = token.info.strip().split()[0] if token.info.strip() else 'plain'
            blocks.append(Block('code', token.content, language=language, code=CodeDocument(token.content, language), quote_depth=quote_depth))
        elif kind == 'hr': blocks.append(Block('rule'))
        elif kind == 'html_block': blocks.append(Block('paragraph', token.content, inlines=[Inline(text=token.content)]))
        elif kind == 'inline':
            current = cell if table is not None else block.inlines
            style, target, spoiler = 0, '', 0
            style_stack = []
            search_at = 0
            source_part = token.content
            base = starts[token.map[0]] if token.map and token.map[0] < len(starts) else 0
            if token.map and token.map[0] < len(lines) and source_part:
                first_line = source_part.split('\n', 1)[0]
                column = lines[token.map[0]].find(first_line)
                if column >= 0: base += len(lines[token.map[0]][:column].encode('utf8'))
            for child in token.children or []:
                name = child.type
                if name.endswith('_open') and name[:-5] in styles:
                    style_stack.append(style)
                    style |= styles[name[:-5]]
                    if name == 'spoiler_open': spoiler_id += 1; spoiler = spoiler_id
                    continue
                if name.endswith('_close') and name[:-6] in styles:
                    style = style_stack.pop() if style_stack else style & ~styles[name[:-6]]
                    continue
                if name == 'link_open': target = child.attrGet('href') or ''; continue
                if name == 'link_close': target = ''; continue
                if name == 'html_inline':
                    match = re.fullmatch(r'<(/?)(u|mark|kbd|b|strong|i|em|s|del)>', child.content)
                    if match:
                        flag = tags[match[2]]
                        style = style & ~flag if match[1] else style | flag
                        tag_at = source_part.find(child.content, search_at)
                        if tag_at >= 0: search_at = tag_at+len(child.content)
                        continue
                if name == 'image':
                    caption = child.content
                    match = re.fullmatch(r'(.*?)\|\s*(\d+)(?:x(\d+))?', caption)
                    current.append(Inline('image', match[1] if match else caption, child.attrGet('src') or '',
                                          width=min(16384, int(match[2])) if match else 0,
                                          height=min(16384, int(match[3] or 0)) if match else 0))
                    continue
                value = '\n' if name in ('softbreak', 'hardbreak') else child.content
                if not value: continue
                marker = child.markup if name == 'text_special' and child.markup else value
                at = source_part.find(marker, search_at)
                if at < 0: at = search_at
                begin = base+len(source_part[:at].encode('utf8'))
                search_at = at+len(marker)
                current.append(Inline('link' if target else 'code' if name == 'code_inline' else 'soft_break' if name == 'softbreak' else 'text', value, target,
                                      style | (MONO if name == 'code_inline' else 0), begin, begin+len(marker.encode('utf8')), spoiler))
            if table is not None:
                table.rows[-1].append(''.join(r.text for r in current))
            else:
                block.text = ''.join(r.text for r in current)
                if block.kind == 'list' and (match := re.match(r'^\[([ xX])\]\s+', block.text)):
                    block.task, block.checked = True, match[1] != ' '
                    block.task_offset = base+len(source_part[:source_part.find('[')+1].encode('utf8'))
                    remove = len(match[0]); block.text = block.text[remove:]
                    while current and remove:
                        if len(current[0].text) <= remove: remove -= len(current.pop(0).text)
                        else:
                            current[0].text = current[0].text[remove:]; current[0].source_begin += remove; remove = 0
                if block.kind == 'heading':
                    slug = re.sub(r'[^\w\s-]', '', block.text.lower()).strip()
                    slug = re.sub(r'\s+', '-', slug) or 'section'
                    number = slugs.get(slug, 0); slugs[slug] = number+1
                    block.anchor = slug+(f'-{number}' if number else '')
        if len(blocks) > 262144:
            raise ValueError('Document block budget exceeded')
    return blocks
