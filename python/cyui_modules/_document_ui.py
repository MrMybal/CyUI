"""Native Markdown presentation. Resource services are explicit and nonblocking."""
from dataclasses import dataclass
import re
from .common import ui
from .images import ImageData, ImageState, image_viewport
from ._markdown import BOLD, ITALIC, UNDERLINE, STRIKE, HIGHLIGHT, SPOILER, KEYBOARD, MONO


@dataclass
class LinkPreview:
    title: str = ''
    description: str = ''
    site: str = ''
    image: object = None
    embed_source: str = ''
    kind: str = 'web'


@dataclass
class DocumentMedia:
    id: int
    kind: str
    source: str
    title: str = ''
    width: float = 0
    height: float = 0


@dataclass
class DocumentServices:
    image: object = None
    preview: object = None
    embed: object = None


@dataclass
class DocumentOptions:
    services: object = None
    tasks: bool = True
    link_previews: bool = True
    image_zoom: bool = True
    image_height: float = 280
    embed_height: float = 240
    scroll_to_source: object = None
    breaks: bool = True


def media_kind(source):
    path = re.split(r'[?#]', source.lower())[0]
    if path.endswith(('.mp4', '.mkv', '.mov', '.webm', '.ogv', '.m4v')): return 'video'
    if path.endswith(('.mp3', '.wav', '.flac', '.ogg', '.oga', '.opus', '.m4a', '.aac')): return 'audio'
    return 'image'


def media(run, state, options, result):
    i, c = ui()
    services, kind = options.services, media_kind(run.target)
    identifier = i.get_id(run.target)
    if kind != 'image':
        if identifier in state.active_embeds and services and services.embed:
            if c.ghost_button(c.tr_label('Close content')): state.active_embeds.discard(identifier)
            elif services.embed(DocumentMedia(identifier, kind, run.target, run.text, run.width, run.height), (i.get_content_region_avail().x, c.dp(options.embed_height))): return
            else: state.active_embeds.discard(identifier)
        c.heading(run.text or run.target, 3)
        c.text_dim(c.tr('Video' if kind == 'video' else 'Audio'))
        if c.ghost_button(c.tr_label('Open content')):
            result['media'] = run.target
            if services and services.embed: state.active_embeds.add(identifier)
        return
    image = services.image(run.target) if services and services.image else ImageData()
    if not image or not image.texture or image.width <= 0 or image.height <= 0:
        c.callout('info', c.tr('Image unavailable')); c.text_dim(run.text or run.target); return
    width = min(i.get_content_region_avail().x, c.dp(run.width) if run.width else image.width)
    height = width*image.height/image.width
    limit = c.dp(run.height or options.image_height)
    if height > limit: width, height = width*limit/height, limit
    position = tuple(i.get_cursor_screen_pos())
    if i.invisible_button('##image', (max(c.dp(1), width), max(c.dp(1), height)), i.ButtonFlags_.enable_nav) and options.image_zoom:
        state.enlarged_image, state.enlarged_caption, state.image_view = run.target, run.text, ImageState()
    texture = image.texture if isinstance(image.texture, i.ImTextureRef) else i.ImTextureRef(image.texture)
    i.get_window_draw_list().add_image(texture, position, (position[0]+width, position[1]+height), image.uv_min, image.uv_max)
    if run.text: c.text_dim(run.text)


def inline(runs, state, options, result):
    i, c = ui()
    first, right = True, i.get_cursor_screen_pos().x+i.get_content_region_avail().x
    for n, run in enumerate(runs):
        i.push_id(n)
        if run.kind == 'image':
            media(run, state, options, result); first = True; i.pop_id(); continue
        font = c.mono_font() if run.style & (MONO | KEYBOARD) else c.bold_font() if run.style & BOLD else None
        if font: i.push_font(font, 0.)
        shown = ' ' if run.kind == 'soft_break' and not options.breaks else run.text
        pieces = re.findall(r'\n|[^\S\n]+|[^\s]+', shown)
        for part, piece in enumerate(pieces):
            i.push_id(part)
            if piece == '\n': first = True; i.new_line(); i.pop_id(); continue
            width = i.calc_text_size(piece).x
            if not first and i.get_item_rect_max().x+width < right: i.same_line(0, 0)
            elif piece.isspace(): i.pop_id(); continue
            hidden = run.style & SPOILER and run.spoiler not in state.spoilers
            if hidden:
                pos = tuple(i.get_cursor_screen_pos())
                if i.invisible_button('##spoiler', (max(c.dp(1), width), i.get_text_line_height()), i.ButtonFlags_.enable_nav): state.spoilers.add(run.spoiler)
                i.get_window_draw_list().add_rect_filled(pos, (pos[0]+width, pos[1]+i.get_text_line_height()), c.palette().bg3, c.dp(3))
            elif run.target:
                if c.link_text(piece):
                    if run.target.startswith('#'): state.jump_to = run.target[1:]
                    else: result['link'] = run.target
            else:
                draw = i.get_window_draw_list()
                start = tuple(i.get_cursor_screen_pos())
                if run.style & (MONO | KEYBOARD): draw.add_rect_filled(start, (start[0]+width, start[1]+i.get_text_line_height()), c.palette().bg3, c.dp(2))
                vertex = len(draw.vtx_buffer)
                i.text_unformatted(piece)
                if run.style & ITALIC:
                    for v in range(vertex, len(draw.vtx_buffer)):
                        point = draw.vtx_buffer[v].pos
                        draw.vtx_buffer[v].pos = (point.x+(start[1]+i.get_font_size()-point.y)*.14, point.y)
                draw, a, b = i.get_window_draw_list(), tuple(i.get_item_rect_min()), tuple(i.get_item_rect_max())
                if run.style & HIGHLIGHT: draw.add_rect_filled(a, b, c.alpha(c.palette().warning, .2))
                if run.style & UNDERLINE: draw.add_line((a[0], b[1]), b, c.palette().text, c.dp(1))
                if run.style & STRIKE: draw.add_line((a[0], (a[1]+b[1])/2), (b[0], (a[1]+b[1])/2), c.palette().text, c.dp(1))
            first = False; i.pop_id()
        if font: i.pop_font()
        i.pop_id()


def content(state, document, options=None):
    from .code import CodeViewState, code_viewer
    i, c = ui()
    options = options or DocumentOptions()
    result = {'link': '', 'copied_block': -1, 'task_offset': None, 'task_checked': False, 'media': ''}
    for n, block in enumerate(document.blocks):
        i.push_id(n)
        if block.anchor: state.anchors[block.anchor] = i.get_cursor_pos_y()
        indent = c.dp(max(0, block.indent-1)*18+block.quote_depth*12)
        if indent > 0: i.indent(indent)
        if block.kind == 'heading':
            i.push_font(c.bold_font(), i.get_font_size()*(1.5 if block.level == 1 else 1.25 if block.level == 2 else 1.1))
            inline(block.inlines, state, options, result); i.pop_font()
        elif block.kind == 'rule': c.divider()
        elif block.kind == 'code':
            c.text_faint(block.language); i.same_line()
            if c.ghost_button(c.tr_label('Copy')):
                i.set_clipboard_text(block.text); result['copied_block'] = n
            code_viewer('##code', state.code_views.setdefault(n, CodeViewState()), block.code, (0, c.dp(max(90, min(320, len(block.code.lines)*20+48)))))
        elif block.kind == 'table':
            columns = min(32, len(block.alignments))
            if columns and i.begin_table('##table', columns, i.TableFlags_.borders | i.TableFlags_.row_bg | i.TableFlags_.resizable):
                for row, cells in enumerate(block.cells):
                    i.push_id(row); i.table_next_row(i.TableRowFlags_.headers if row == 0 else 0)
                    for col, cell in enumerate(cells[:columns]):
                        i.push_id(col); i.table_next_column(); inline(cell, state, options, result); i.pop_id()
                    i.pop_id()
                i.end_table()
        else:
            if block.kind == 'list':
                if block.task:
                    i.begin_disabled(not options.tasks)
                    changed, checked = i.checkbox('##task', block.checked)
                    if changed: result['task_offset'], result['task_checked'] = block.task_offset, checked
                    i.end_disabled()
                elif block.number: i.text_unformatted(str(block.number)+'.')
                else: i.bullet()
                i.same_line(); i.begin_group()
            inline(block.inlines, state, options, result)
            if block.kind == 'list': i.end_group()
        if options.link_previews and options.services and options.services.preview:
            for run in [r for r in block.inlines if r.kind == 'link' and r.text == r.target and r.target.startswith(('https://', 'http://'))][:5]:
                if run.source_begin and document.text.encode('utf8')[run.source_begin-1:run.source_begin] == b'<': continue
                preview = options.services.preview(run.target)
                if not preview or not preview.title: continue
                c.text_faint(preview.site)
                if c.link_text(preview.title): result['link'] = run.target
                i.text_wrapped(preview.description)
        if indent > 0: i.unindent(indent)
        c.space(5); i.pop_id()
    return result


def lightbox(state, options):
    if not state.enlarged_image: return
    i, c = ui(); i.open_popup('##documentImage')
    viewport = i.get_main_viewport()
    i.set_next_window_pos((viewport.work_pos.x+viewport.work_size.x*.5, viewport.work_pos.y+viewport.work_size.y*.5), i.Cond_.always, (.5, .5))
    i.set_next_window_size((viewport.work_size.x*.9, viewport.work_size.y*.9), i.Cond_.always)
    visible, opened = i.begin_popup_modal('##documentImage', True, i.WindowFlags_.no_saved_settings)
    if visible:
        if c.ghost_button(c.tr_label('Close')) or i.is_key_pressed(i.Key.escape): opened = False; i.close_current_popup()
        i.same_line(); c.text_dim(state.enlarged_caption)
        image = options.services.image(state.enlarged_image) if options.services and options.services.image else ImageData()
        image_viewport('##zoom', state.image_view, image, tuple(i.get_content_region_avail()))
        i.end_popup()
    if not opened: state.enlarged_image = ''
