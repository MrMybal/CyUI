"""Host-owned scene/layer hierarchy with cycle-safe reparent requests."""
from dataclasses import dataclass, field
from .common import ui, item_id, select


@dataclass
class HierarchyItem:
    id: int
    parent: int
    label: str
    kind: str = ''
    visible: bool = True
    locked: bool = False


@dataclass
class HierarchyState:
    query: str = ''
    selection: list = field(default_factory=list)
    collapsed: set = field(default_factory=set)
    anchor: int = 0
    renaming: int = 0
    rename_text: str = ''
    focused: int = 0
    scroll_to_focus: bool = False
    rename_focus: bool = False


def hierarchy_rows(items, state):
    index = {p.id: p for p in items if p.id}
    children = {}
    for item in index.values(): children.setdefault(item.parent if item.parent in index else 0, []).append(item)
    matching = set()
    if state.query:
        for item in items:
            if state.query.casefold() in (item.label+' '+item.kind).casefold():
                key, path = item.id, set()
                while key in index and key not in path:
                    path.add(key); matching.add(key); key = index[key].parent
    rows, seen = [], set()
    def walk(root):
        stack = [(root, 0)]
        while stack:
            item, depth = stack.pop()
            if item.id in seen: continue
            seen.add(item.id)
            if state.query and item.id not in matching: continue
            descendants = children.get(item.id, [])
            rows.append((item, depth, bool(descendants)))
            if state.query or item.id not in state.collapsed:
                stack += [(p, depth+1) for p in reversed(descendants)]
    for root in children.get(0, []): walk(root)
    for item in index.values():
        key, path = item.id, set()
        if key in seen: continue
        while key in index and key not in path:
            path.add(key); key = index[key].parent
        if key in path: walk(item)
    return rows


def can_reparent_hierarchy(items, selected, parent):
    index = {p.id: p for p in items}
    if not selected or len(index) != len(items) or (parent and (parent not in index or index[parent].locked)): return False
    if any(key not in index or index[key].locked for key in selected): return False
    seen = set()
    while parent in index:
        if parent in selected or parent in seen: return False
        seen.add(parent); parent = index[parent].parent
    return True


def navigate_hierarchy(items, state, key, extend=False, additive=False, page=10):
    rows = hierarchy_rows(items, state)
    if not rows: state.focused = 0; return
    ids = [row[0].id for row in rows]
    if key == 'select_all':
        state.selection = ids; state.focused = state.anchor = ids[0]; state.scroll_to_focus = True; return
    previous = state.focused or (state.selection[-1] if state.selection else 0)
    at = ids.index(previous) if previous in ids else 0; old = at; identifier = ids[at]
    if key == 'toggle': select(state.selection, identifier, True); state.focused = state.anchor = identifier; return
    if key == 'up': at = max(0, at-1)
    elif key == 'down': at = min(at+1, len(rows)-1) if previous in ids else 0
    elif key == 'home': at = 0
    elif key == 'end': at = len(rows)-1
    elif key == 'page_up': at = max(0, at-page)
    elif key == 'page_down': at = min(len(rows)-1, at+page)
    elif key == 'right':
        if rows[at][2] and identifier in state.collapsed:
            state.collapsed.remove(identifier); state.focused = identifier; state.scroll_to_focus = True; return
        if at+1 < len(rows) and rows[at+1][1] > rows[at][1]: at += 1
    elif key == 'left':
        if rows[at][2] and identifier not in state.collapsed and not state.query:
            state.collapsed.add(identifier); state.focused = identifier; return
        while at and rows[at-1][1] >= rows[old][1]: at -= 1
        at = at-1 if at else old
    state.focused = ids[at]; state.scroll_to_focus = True
    if extend:
        if not state.anchor: state.anchor = ids[old]
        anchor = ids.index(state.anchor) if state.anchor in ids else old
        if not additive: state.selection.clear()
        state.selection += [key for key in ids[min(anchor, at):max(anchor, at)+1] if key not in state.selection]
    elif not additive: state.selection = [state.focused]; state.anchor = state.focused


def hierarchy(identifier, state, items, size=(0, 350), read_only=False, can_reparent=None):
    i, c = ui(); requests = []; i.push_id(identifier)
    _, state.query = i.input_text('##search', state.query)
    i.begin_child('##tree', (size[0], max(c.dp(1), size[1]-i.get_frame_height_with_spacing()) if size[1] else 0))
    focused = i.is_window_focused(i.FocusedFlags_.child_windows) and not i.get_io().want_text_input
    if focused and not state.renaming:
        for imgui_key, key in [(i.Key.up_arrow, 'up'), (i.Key.down_arrow, 'down'), (i.Key.left_arrow, 'left'), (i.Key.right_arrow, 'right'),
                               (i.Key.home, 'home'), (i.Key.end, 'end'), (i.Key.page_up, 'page_up'), (i.Key.page_down, 'page_down'), (i.Key.space, 'toggle')]:
            if i.is_key_pressed(imgui_key): navigate_hierarchy(items, state, key, i.get_io().key_shift, i.get_io().key_ctrl,
                max(1, int(i.get_content_region_avail().y/i.get_frame_height_with_spacing())))
        if i.get_io().key_ctrl and i.is_key_pressed(i.Key.a): navigate_hierarchy(items, state, 'select_all')
    rows = hierarchy_rows(items, state); clip = i.ListClipper(); clip.begin(len(rows), i.get_frame_height_with_spacing())
    for index, row in enumerate(rows):
        if (state.scroll_to_focus and row[0].id == state.focused) or row[0].id == state.renaming: clip.include_item_by_index(index)
    while clip.step():
        for row in range(clip.display_start, clip.display_end):
            item, depth, children = rows[row]; i.push_id(item_id(item.id))
            if depth: i.indent(c.dp(16)*depth)
            if children:
                if c.ghost_button('>' if item.id in state.collapsed else 'v'):
                    state.collapsed.remove(item.id) if item.id in state.collapsed else state.collapsed.add(item.id)
            else: i.dummy((i.get_frame_height(), i.get_frame_height()))
            i.same_line()
            if state.renaming == item.id:
                if state.rename_focus: i.set_keyboard_focus_here(); state.rename_focus = False
                done, state.rename_text = i.input_text('##rename', state.rename_text, i.InputTextFlags_.enter_returns_true)
                if done and not read_only and not item.locked and state.rename_text:
                    requests.append(dict(action='rename', ids=[item.id], text=state.rename_text)); state.renaming = 0
                if i.is_key_pressed(i.Key.escape) or read_only: state.renaming = 0
            else:
                clicked, _ = i.selectable(item.label, item.id in state.selection, 0, (max(c.dp(40), i.get_content_region_avail().x-c.dp(70)), i.get_frame_height()))
                if clicked:
                    anchor = next((n for n, r in enumerate(rows) if r[0].id == state.anchor), row)
                    if i.get_io().key_shift:
                        if not i.get_io().key_ctrl: state.selection.clear()
                        state.selection += [rows[n][0].id for n in range(min(anchor, row), max(anchor, row)+1) if rows[n][0].id not in state.selection]
                    else: select(state.selection, item.id, i.get_io().key_ctrl); state.anchor = item.id
                    state.focused = item.id
                if state.scroll_to_focus and state.focused == item.id: i.set_scroll_here_y(); state.scroll_to_focus = False
                if not read_only and not item.locked and i.begin_drag_drop_source():
                    if item.id not in state.selection: state.selection = [item.id]
                    i.set_drag_drop_payload_py_id('CYUI_HIERARCHY', item.id); i.text_unformatted(item.label); i.end_drag_drop_source()
                if not read_only and i.begin_drag_drop_target():
                    payload = i.accept_drag_drop_payload_py_id('CYUI_HIERARCHY')
                    parent = item.parent if i.get_io().key_shift else item.id
                    if payload is not None and can_reparent_hierarchy(items, state.selection, parent) and (can_reparent is None or can_reparent(state.selection, parent)):
                        requests.append(dict(action='reparent', ids=list(state.selection), parent=parent, before=item.id if i.get_io().key_shift else 0))
                    i.end_drag_drop_target()
                if i.begin_popup_context_item('##context'):
                    for label, action in (('Rename', 'rename'), ('Duplicate', 'duplicate'), ('Remove', 'remove')):
                        if i.menu_item(c.tr_label(label), '', False, not read_only and not item.locked)[0]:
                            if action == 'rename': state.renaming, state.rename_text, state.rename_focus = item.id, item.label, True
                            else: requests.append(dict(action=action, ids=list(state.selection) if item.id in state.selection else [item.id]))
                    i.end_popup()
            i.same_line(); i.begin_disabled(read_only or item.locked)
            if c.ghost_button('V' if item.visible else '-'): requests.append(dict(action='visibility', ids=[item.id], value=not item.visible))
            i.end_disabled(); i.same_line(); i.begin_disabled(read_only)
            if c.ghost_button('L' if item.locked else '-'): requests.append(dict(action='locking', ids=[item.id], value=not item.locked))
            i.end_disabled()
            if depth: i.unindent(c.dp(16)*depth)
            i.pop_id()
    if not read_only:
        i.selectable(c.tr_label('Drop here to move to root'), False)
        if i.begin_drag_drop_target():
            payload = i.accept_drag_drop_payload_py_id('CYUI_HIERARCHY')
            if payload is not None and can_reparent_hierarchy(items, state.selection, 0) and (can_reparent is None or can_reparent(state.selection, 0)):
                requests.append(dict(action='reparent', ids=list(state.selection), parent=0, before=0))
            i.end_drag_drop_target()
    if focused and not read_only:
        if i.is_key_pressed(i.Key.delete):
            keys = [item.id for item in items if item.id in state.selection and not item.locked]
            if keys: requests.append(dict(action='remove', ids=keys))
        if (i.is_key_pressed(i.Key.f2) or i.is_key_pressed(i.Key.enter)) and len(state.selection) == 1:
            item = next((item for item in items if item.id == state.selection[0] and not item.locked), None)
            if item: state.renaming, state.rename_text, state.rename_focus = item.id, item.label, True
    i.end_child(); i.pop_id(); return requests
