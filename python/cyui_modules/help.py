"""Validated links to the bundled CyDoc reader; importing this module launches nothing."""
from importlib.resources import files
import json
from pathlib import Path
from urllib.parse import quote, urlencode
import webbrowser


def documentation_url(page='overview', section='', *, locale='en', media=None, manual=None):
    """Return an encoded file URI, checking its page, anchor and optional media ID.

    ``manual`` may override the packaged Documentation.html for an application's
    own staged copy. Installed wheels must be extracted normally, as pip does.
    """
    reader = Path(manual).resolve() if manual is not None else Path(str(
        files('cyui_documentation').joinpath('Documentation', 'Documentation.html'))).resolve()
    if not reader.is_file():
        raise FileNotFoundError(f'CyUI manual is missing: {reader}')
    manifest = json.loads((reader.parent / 'DocData/manifest.json').read_text(encoding='utf-8'))
    codes = {item['code'] for item in manifest['locales']}
    locale = locale.lower().replace('_', '-')
    locale = locale if locale in codes else locale.split('-')[0]
    if locale not in codes:
        locale = manifest['defaultLocale']
    target = manifest.get('aliases', {}).get(page, page)
    if '#' in target:
        target, alias_section = target.split('#', 1)
        section = section or alias_section
    found = next((p for p in manifest['pages'] if p['id'] == target and p['locale'] == locale), None)
    if found is None:
        raise ValueError(f'Unknown CyUI help page: {page}')
    if section and section not in ({h['id'] for h in found['headings']} | set(found.get('anchors', []))):
        raise ValueError(f'Unknown CyUI help section: {section}')
    route = reader.as_uri() + '#/' + quote(locale, safe='') + '/' + quote(target, safe='')
    if section:
        route += '/' + quote(section, safe='')
    if media is not None:
        if f'data-cydoc-id="{media}"' not in found['html']:
            raise ValueError(f'Unknown CyUI help media: {media}')
        route += '?' + urlencode({'media': media})
    return route


def open_documentation(page='overview', section='', **options):
    """Ask the system browser to show local help, returning whether it accepted."""
    return webbrowser.open(documentation_url(page, section, **options))
