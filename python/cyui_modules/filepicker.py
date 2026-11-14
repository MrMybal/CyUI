"""Async host/VFS file picker. This module never accesses the filesystem."""
from dataclasses import dataclass, field
import re
from .common import ui, item_id


@dataclass
class FileEntry:
    id: int
    name: str
    directory: bool = False
    bytes: int = 0
    modified: str = ''


@dataclass
class FileListing:
    directory: str
    entries: list = field(default_factory=list)
    loading: bool = False
    error: str = ''


@dataclass
class FileFilter:
    label: str
    extensions: tuple = ()


@dataclass
class FilePickerOptions:
    mode: str = 'openFile'
    root: str = '/'
    filters: list = field(default_factory=list)
    confirm_overwrite: bool = True


@dataclass
class FilePickerState:
    directory: str = '/'
    path_text: str = '/'
    filename: str = ''
    query: str = ''
    selection: list = field(default_factory=list)
    filter: int = 0
    history: list = field(default_factory=list)
    history_index: int = 0
    pending_overwrite: str = ''
    error: str = ''


def _filename(name): return bool(name) and name not in ('.', '..') and not any(ord(c) < 32 or c in '/\\:' for c in name)


def normalize_file_path(path, root='/'):
    def clean(text):
        text = text.replace('\\', '/')
        prefix = '/' if text.startswith('/') else text[:3] if re.match(r'^[A-Za-z]:/', text) else ''
        if not prefix: return ''
        parts = []
        for part in text[len(prefix):].split('/'):
            if part == '..':
                if not parts: return ''
                parts.pop()
            elif part and part != '.':
                if not _filename(part): return ''
                parts.append(part)
        return prefix+'/'.join(parts)
    base, value = clean(root), clean(path)
    return value if base and value and (value == base or value.startswith(base.rstrip('/')+'/')) else ''


def file_matches_filter(name, filter):
    return not filter.extensions or any(ext in ('*', '*.*') or name.casefold().endswith('.'+ext.lstrip('.').casefold()) for ext in filter.extensions)


def validate_file_picker(state, listing, options, overwrite_confirmed=False):
    result = dict(accepted=False, overwrite_requested=False, paths=[], directory=state.directory)
    if not normalize_file_path(state.directory, options.root) or state.directory != listing.directory or listing.loading or listing.error: return result
    join = lambda name: state.directory.rstrip('/')+'/'+name
    matches = lambda name: not options.filters or (0 <= state.filter < len(options.filters) and file_matches_filter(name, options.filters[state.filter]))
    if options.mode == 'directory': result.update(accepted=True, paths=[state.directory])
    elif options.mode == 'saveFile':
        if not _filename(state.filename) or not matches(state.filename): return result
        entry = next((e for e in listing.entries if e.name == state.filename), None)
        if entry and entry.directory: return result
        result['paths'] = [join(state.filename)]
        if entry and options.confirm_overwrite and not overwrite_confirmed: result['overwrite_requested'] = True
        else: result['accepted'] = True
    else:
        if not state.selection or (options.mode == 'openFile' and len(state.selection) != 1): return result
        for name in state.selection:
            entry = next((e for e in listing.entries if e.name == name), None)
            if not _filename(name) or entry is None or entry.directory or not matches(name): return result
        result.update(accepted=True, paths=[join(n) for n in state.selection])
    return result


def file_picker(identifier, state, listing, size=(0, 420), options=None):
    options = options or FilePickerOptions(); i, c = ui(); i.push_id(identifier)
    result = dict(accepted=False, cancelled=False, navigate=False, paths=[], directory=state.directory)
    def navigate(path, record=True):
        target = normalize_file_path(path, options.root)
        if not target: state.error = 'Path is outside the allowed root'; return
        if target == state.directory: return
        old = state.directory
        state.directory = state.path_text = target; state.selection.clear(); state.pending_overwrite = ''; state.error = ''
        result.update(navigate=True, directory=target)
        if record:
            if not state.history: state.history = [old]
            state.history = state.history[:state.history_index+1]+[target]; state.history_index = len(state.history)-1
    if not normalize_file_path(state.directory, options.root): navigate(options.root)
    i.begin_disabled(not state.history or state.history_index == 0)
    if c.ghost_button('<'): state.history_index -= 1; navigate(state.history[state.history_index], False)
    i.end_disabled(); i.same_line(); i.begin_disabled(not state.history or state.history_index+1 >= len(state.history))
    if c.ghost_button('>'): state.history_index += 1; navigate(state.history[state.history_index], False)
    i.end_disabled(); i.same_line()
    if c.ghost_button(c.tr('Up')): navigate(state.directory.rsplit('/', 1)[0] or '/')
    i.same_line(); i.set_next_item_width(-1)
    entered, state.path_text = i.input_text('##path', state.path_text, i.InputTextFlags_.enter_returns_true)
    if entered: navigate(state.path_text)
    i.set_next_item_width(-1); _, state.query = i.input_text('##search', state.query)
    i.begin_child('##files', (size[0], max(c.dp(50), size[1]-5*i.get_frame_height_with_spacing()) if size[1] else c.dp(240)))
    submit = False
    if listing.directory != state.directory or listing.loading: c.text_dim(c.tr('Loading...'))
    elif listing.error: c.text_coloured(c.palette().error, listing.error)
    else:
        entries = sorted((e for e in listing.entries if _filename(e.name) and state.query.casefold() in e.name.casefold() and (e.directory or not options.filters or file_matches_filter(e.name, options.filters[min(state.filter, len(options.filters)-1)]))), key=lambda e: (not e.directory, e.name.casefold()))
        clip = i.ListClipper(); clip.begin(len(entries))
        while clip.step():
            for n in range(clip.display_start, clip.display_end):
                entry = entries[n]; i.push_id(item_id(entry.id))
                clicked, _ = i.selectable(('[+] ' if entry.directory else '')+entry.name, entry.name in state.selection, i.SelectableFlags_.allow_double_click)
                if clicked:
                    if entry.directory:
                        if i.is_mouse_double_clicked(0): navigate(state.directory.rstrip('/')+'/'+entry.name)
                    else:
                        if options.mode == 'openFiles' and i.get_io().key_ctrl:
                            state.selection.remove(entry.name) if entry.name in state.selection else state.selection.append(entry.name)
                        else: state.selection = [entry.name]
                        state.filename = entry.name
                        if i.is_mouse_double_clicked(0): submit = True
                i.pop_id()
    i.end_child()
    if options.filters:
        _, state.filter = i.combo('##filter', min(state.filter, len(options.filters)-1), [c.tr(f.label) for f in options.filters])
    if options.mode == 'saveFile':
        entered, state.filename = i.input_text('##filename', state.filename, i.InputTextFlags_.enter_returns_true); submit |= entered
    submit |= c.primary_button(c.tr_label('Save' if options.mode == 'saveFile' else 'Open'))
    i.same_line()
    if c.button(c.tr_label('Cancel')): result['cancelled'] = True
    if submit:
        accepted = validate_file_picker(state, listing, options)
        if accepted['overwrite_requested']:
            state.pending_overwrite = accepted['paths'][0]; i.open_popup(c.tr_label('Replace existing file?'))
        elif accepted['accepted']: result.update(accepted)
        else: state.error = 'Select a valid file or folder'
    shown, _ = i.begin_popup_modal(c.tr_label('Replace existing file?'))
    if shown:
        i.text_unformatted(state.pending_overwrite)
        if c.danger_button(c.tr_label('Replace')):
            accepted = validate_file_picker(state, listing, options, True)
            if accepted['accepted'] and accepted['paths'][0] == state.pending_overwrite: result.update(accepted)
            i.close_current_popup()
        i.same_line()
        if c.button(c.tr_label('Cancel')) or i.is_key_pressed(i.Key.escape): i.close_current_popup()
        i.end_popup()
    if state.error: c.text_coloured(c.palette().error, c.tr(state.error))
    i.pop_id(); return result
