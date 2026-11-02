"""Asset presentation and requests; no filesystem operations."""
from dataclasses import dataclass, field
from .common import select, ui, item_id


@dataclass
class Asset:
    id: int
    label: str
    parent: int = 0
    type: str = ''
    thumbnail: object = 0
    folder: bool = False
    favorite: bool = False


@dataclass
class AssetState:
    query: str = ''
    folder: int = 0
    selection: list = field(default_factory=list)
    grid: bool = True


def asset_browser(identifier, state, assets, size=(0, 320)):
    i, c = ui()
    result = {'id': 0, 'action': 'none', 'query_changed': False}
    i.push_id(identifier)
    if i.button(c.tr_label('Up')):
        state.folder = next((a.parent for a in assets if a.id == state.folder), 0)
    i.same_line()
    _, state.grid = i.checkbox(c.tr_label('Grid'), state.grid)
    i.same_line()
    i.set_next_item_width(-1)
    result['query_changed'], state.query = i.input_text('##search', state.query)
    terms = state.query.casefold().split()
    visible = [a for a in assets if (bool(terms) or a.parent == state.folder) and all(t in (a.label+' '+a.type).casefold() for t in terms)]
    height = max(c.dp(1), size[1]-i.get_frame_height_with_spacing()) if size[1] else 0
    i.begin_child('##assets', (size[0], height))
    columns = max(1, int(i.get_content_region_avail().x/c.dp(128))) if state.grid else 1
    if i.begin_table('##grid', columns):
        for asset in visible:
            i.table_next_column()
            i.push_id(item_id(asset.id))
            pos = tuple(i.get_cursor_screen_pos())
            height = c.dp(104) if state.grid else i.get_frame_height()
            width = max(c.dp(16), i.get_content_region_avail().x)
            clicked, _ = i.selectable('##asset', asset.id in state.selection, i.SelectableFlags_.allow_double_click, (width, height))
            draw, p = i.get_window_draw_list(), c.palette()
            if state.grid:
                if asset.thumbnail:
                    texture = asset.thumbnail if isinstance(asset.thumbnail, i.ImTextureRef) else i.ImTextureRef(asset.thumbnail)
                    draw.add_image(texture, (pos[0]+c.dp(8), pos[1]+c.dp(4)), (pos[0]+c.dp(72), pos[1]+c.dp(68)))
                else:
                    draw.add_rect_filled((pos[0]+c.dp(8), pos[1]+c.dp(4)), (pos[0]+c.dp(72), pos[1]+c.dp(68)), p.bg3, c.dp(4))
                c.clipped_text(draw, (pos[0]+c.dp(8), pos[1]+c.dp(76)), width-c.dp(16), p.text, asset.label)
            else:
                c.clipped_text(draw, pos, width, p.text, asset.label)
            if clicked:
                select(state.selection, asset.id, i.get_io().key_ctrl)
                if i.is_mouse_double_clicked(0):
                    if asset.folder:
                        state.folder = asset.id
                    else:
                        result.update(id=asset.id, action='open')
            if i.begin_drag_drop_source():
                # Payload is a stable numeric ID, never an object address.
                i.set_drag_drop_payload_py_id('CYUI_ASSET', asset.id)
                i.text_unformatted(asset.label)
                i.end_drag_drop_source()
            if i.begin_popup_context_item('##context'):
                for label, action in (('Open', 'open'), ('Toggle favorite', 'favorite'), ('Rename', 'rename'), ('Remove', 'remove')):
                    if i.menu_item(c.tr_label(label), '', False)[0]:
                        result.update(id=asset.id, action=action)
                i.end_popup()
            i.pop_id()
        i.end_table()
    i.end_child()
    i.pop_id()
    return result
