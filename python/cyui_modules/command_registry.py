"""Context-aware command registry; menus, palettes and toolbars share Command records."""
from dataclasses import replace
import json
import re
from .common import ui, item_id


def parse_shortcut(text):
    if not text: return ''
    modifiers, key = set(), ''
    aliases = {'control': 'Ctrl', 'ctrl': 'Ctrl', 'shift': 'Shift', 'alt': 'Alt', 'super': 'Super', 'cmd': 'Super', 'command': 'Super'}
    names = {k.lower(): k for k in ('Tab', 'LeftArrow', 'RightArrow', 'UpArrow', 'DownArrow', 'PageUp', 'PageDown', 'Home', 'End', 'Insert', 'Delete', 'Backspace', 'Space', 'Enter', 'Escape', 'Minus', 'Equal', 'Comma', 'Period', 'Slash', 'Semicolon', 'Apostrophe', 'LeftBracket', 'RightBracket', 'Backslash', 'GraveAccent')}
    for part in text.split('+'):
        part = part.strip().lower()
        if part in aliases:
            if aliases[part] in modifiers: raise ValueError('Duplicate modifier')
            modifiers.add(aliases[part])
        else:
            if key: raise ValueError('Expected one shortcut key')
            key = part.upper() if re.fullmatch(r'[a-z0-9]|f(?:[1-9]|1[0-9]|2[0-4])', part) else names.get(part, '')
            if not key: raise ValueError('Unknown shortcut key')
    if not key: raise ValueError('Expected shortcut key')
    return '+'.join([m for m in ('Ctrl', 'Shift', 'Alt', 'Super') if m in modifiers]+[key])


class CommandRegistry:
    def __init__(self): self.entries = {}

    def add(self, command):
        if not command.id or command.id in self.entries: return False
        try: shortcut = parse_shortcut(command.shortcut)
        except ValueError: return False
        if shortcut and any(e.context == command.context and e.shortcut == shortcut for e in self.entries.values()): return False
        self.entries[command.id] = replace(command, shortcut=shortcut)
        return True

    def rebind(self, identifier, shortcut):
        if identifier not in self.entries: return False
        try: shortcut = parse_shortcut(shortcut)
        except ValueError: return False
        command = self.entries[identifier]
        if shortcut and any(e.id != identifier and e.context == command.context and e.shortcut == shortcut for e in self.entries.values()): return False
        command.shortcut = shortcut; return True

    def commands(self, contexts=()): return [replace(e) for e in self.entries.values() if not e.context or e.context in contexts]

    def match(self, shortcut, contexts=(), typing=False):
        shortcut = parse_shortcut(shortcut); winner, priority = 0, len(contexts)+1
        for command in self.commands(contexts):
            rank = contexts.index(command.context) if command.context else len(contexts)
            if shortcut and command.shortcut == shortcut and rank < priority and command.enabled and (not typing or command.allow_while_typing): winner, priority = command.id, rank
        return winner

    def bindings_json(self): return json.dumps(dict(format='cyui.commands', version=1, bindings=[dict(id=e.id, shortcut=e.shortcut) for e in self.entries.values()]), separators=(',', ':'))

    def load_bindings_json(self, text):
        if len(text.encode('utf8')) > 1024*1024: raise ValueError('Clipboard exceeds 1 MiB')
        value = json.loads(text)
        if value['format'] != 'cyui.commands' or value['version'] != 1: raise ValueError('Unsupported binding format')
        ids = [p['id'] for p in value['bindings']]
        if len(set(ids)) != len(ids) or any(key not in self.entries for key in ids): raise ValueError('Unknown or duplicate command ID')
        copy = CommandRegistry(); copy.entries = {key: replace(command, shortcut='' if key in ids else command.shortcut) for key, command in self.entries.items()}
        for binding in value['bindings']:
            if not copy.rebind(binding['id'], binding['shortcut']): raise ValueError('Invalid or conflicting shortcut')
        self.entries = copy.entries

    def poll(self, contexts=()):
        i, _ = ui(); seen = set()
        for command in self.commands(contexts):
            if not command.shortcut or command.shortcut in seen: continue
            seen.add(command.shortcut)
            winner = self.match(command.shortcut, contexts, i.get_io().want_text_input)
            chord = 0
            for part in command.shortcut.split('+'):
                if part in ('Ctrl', 'Shift', 'Alt', 'Super'): chord |= getattr(i.Key, 'mod_'+part.lower()).value
                else:
                    key = next((k for k in range(i.Key.named_key_begin.value, i.Key.mouse_left.value) if i.get_key_name(i.Key(k)).lower() == part.lower()), 0)
                    chord |= key
            if winner and i.shortcut(chord): return winner
        return 0

    def menu_item(self, identifier):
        i, c = ui(); command = self.entries.get(identifier)
        return bool(command and i.menu_item(c.tr_label(command.label), command.shortcut, command.checked, command.enabled)[0])

    def toolbar_button(self, identifier, icon):
        i, c = ui(); command = self.entries.get(identifier)
        if command is None: return False
        i.push_id(item_id(identifier)); i.begin_disabled(not command.enabled)
        clicked = c.icon_button(icon, c.tr(command.label)+(f' ({command.shortcut})' if command.shortcut else ''), command.checked)
        i.end_disabled(); i.pop_id(); return clicked
