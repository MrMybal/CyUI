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


def hierarchy(identifier, state, items, size=(0, 350), read_only=False, can_reparent=None):
    i, c = ui(); requests = []; i.push_id(identifier)
    _, state.query = i.input_text('##search', state.query)
    i.begin_child('##tree', (size[0], max(c.dp(1), size[1]-i.get_frame_height_with_spacing()) if size[1] else 0))
    rows = hierarchy_rows(items, state); clip = i.ListClipper(); clip.begin(len(rows), i.get_frame_height_with_spacing())
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
                if i.is_window_appearing(): i.set_keyboard_focus_here()
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
                            if action == 'rename': state.renaming, state.rename_text = item.id, item.label
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
    i.end_child(); i.pop_id(); return requests
