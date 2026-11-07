"""Regenerate the CyUI manual with the pinned, locally installed CyDoc CLI."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent


def stage_inputs(project, destination):
    """Use Git's canonical LF text without changing an author's working files."""
    for source, target in ((project / '.CyDocBuild/manual', destination / '.CyDocBuild/manual'),
                           (project / 'include/cyui', destination / 'include/cyui')):
        for path in source.rglob('*'):
            if path.is_file():
                output = target / path.relative_to(source)
                output.parent.mkdir(parents=True, exist_ok=True)
                output.write_text(path.read_text('utf-8'), encoding='utf-8', newline='\n')
    shutil.copytree(project / '.CyDocBuild/assets', destination / '.CyDocBuild/assets')


def publish(source, destination, check=False):
    """Replace only files owned by the preceding generated export."""
    files = {p.relative_to(source).as_posix(): p.read_bytes() for p in source.rglob('*') if p.is_file()}
    stamp = destination / '.cydoc-files.json'
    old = json.loads(stamp.read_text('utf-8')) if stamp.exists() else []
    for name in old:
        target = (destination / name).resolve()
        if not target.is_relative_to(destination.resolve()):
            raise RuntimeError('Invalid generated file inventory')
        if name not in files:
            if check:
                raise RuntimeError(f'Stale documentation resource: {name}')
            target.unlink(missing_ok=True)
    for name, content in files.items():
        target = destination / name
        if check:
            if not target.exists() or target.read_bytes() != content:
                raise RuntimeError(f'Regenerate documentation: {target.relative_to(ROOT)}')
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
    inventory = json.dumps(sorted(files), indent=2) + '\n'
    if check:
        if not stamp.exists() or stamp.read_text('utf-8') != inventory:
            raise RuntimeError('Regenerate the documentation file inventory')
    else:
        stamp.write_text(inventory, encoding='utf-8', newline='\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Regenerate privately and compare shipped exports')
    args = parser.parse_args()
    subprocess.run(['node', str(HERE / 'check-api-translations.mjs')], cwd=ROOT, check=True)
    cli = HERE / 'node_modules/cydoc/tools/cli.mjs'
    if not cli.is_file():
        sys.exit('Install the pinned toolchain first: npm ci --prefix .CyDocBuild --omit=optional')
    assets = HERE / 'assets'
    assets.mkdir(exist_ok=True)
    shutil.copy2(ROOT / 'site/assets/mark.svg', assets / 'app-logo.svg')
    shutil.copy2(ROOT / 'site/assets/mark.svg', assets / 'favicon.svg')
    for path in (ROOT / 'site/assets/screenshots').glob('*.webp'):
        shutil.copy2(path, assets / path.name)
    # Reset only the disposable output, after resolving its workspace boundary.
    output = (HERE / '.cache/export').resolve()
    if not output.is_relative_to((HERE / '.cache').resolve()):
        raise RuntimeError('Export must remain inside the generation cache')
    if output.exists():
        shutil.rmtree(output)
    settings = json.loads((HERE / 'cydoc.config.json').read_text('utf-8'))
    settings['out'] = '../Documentation'
    # Keep the original project-relative paths in API/source links while making
    # Markdown code blocks and derived hashes independent of checkout line endings.
    with tempfile.TemporaryDirectory(prefix='inputs-', dir=HERE / '.cache') as temporary:
        workspace = Path(temporary).resolve()
        if not workspace.is_relative_to((HERE / '.cache').resolve()):
            raise RuntimeError('Inputs must remain inside the generation cache')
        stage_inputs(ROOT, workspace)
        generated = workspace / '.CyDocBuild/cydoc.config.json'
        generated.write_text(json.dumps(settings), encoding='utf-8', newline='\n')
        subprocess.run(['node', str(cli), 'check', str(generated)], cwd=ROOT, check=True)
        shutil.copytree(workspace / 'Documentation', output)
    subprocess.run(['node', str(HERE / 'compile-reader.mjs')], cwd=ROOT, check=True)
    (output / 'DocData/cyui.css').write_text((HERE / 'reader.css').read_text('utf-8'), encoding='utf-8', newline='\n')
    # Brand the stock HTML shell; CyDoc still owns the complete reader and providers.
    html = (output / 'Documentation.html').read_text('utf-8')
    html = html.replace('<html lang="fr">', '<html lang="en">')
    html = html.replace('CyDoc — Documentation', 'CyUI — Documentation')
    html = re.sub(r'<meta name="description"[^>]+>', '<meta name="description" content="CyUI: integration, widgets and optional interface modules. Offline English and French manual.">', html)
    html = html.replace('Activez JavaScript pour utiliser le lecteur de documentation CyDoc.', 'Enable JavaScript to read this manual. Activez JavaScript pour lire ce manuel.')
    html = html.replace('</head>', '<link rel="stylesheet" href="DocData/cyui.css"></head>')
    for name in ('viewer.css', 'data.js', 'viewer.js', 'cyui.css'):
        digest = hashlib.sha256((output / 'DocData' / name).read_bytes()).hexdigest()[:12]
        html = html.replace(f'DocData/{name}"', f'DocData/{name}?v={digest}"')
    (output / 'Documentation.html').write_text(html, encoding='utf-8', newline='\n')
    (output / 'DocData/CyDoc-LICENSE.txt').write_text((HERE / 'vendor/CyDoc-LICENSE.txt').read_text('utf-8'), encoding='utf-8', newline='\n')
    # No mesh runtime, third-party source trees or generation workspace are shipped.
    for destination in (ROOT / 'Documentation', ROOT / 'site/Documentation', ROOT / 'python/cyui_documentation/Documentation'):
        publish(output, destination, args.check)
    print('CyUI documentation: exports ' + ('verified' if args.check else 'updated'))


if __name__ == '__main__':
    main()
