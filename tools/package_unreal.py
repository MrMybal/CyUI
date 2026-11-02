"""Prepare a reusable Unreal Editor module or its optional plugin container.

No engine files or existing projects are changed. Output must be a new directory.
Run the resulting plugin with UnrealBuildTool, not the CyUI CMake build.
"""
from pathlib import Path
import argparse
import shutil
import json
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEPENDENCIES = {
    'Audio': [], 'Canvas': [], 'Plots': ['Canvas'], 'Curves': ['Plots'],
    'Nodes': ['Canvas'], 'Timeline': ['Canvas'], 'AudioViews': ['Plots'],
    'Code': [], 'Diff': ['Code'], 'CodeEditor': ['Code'], 'Inspector': [],
    'Assets': [], 'Images': ['Canvas'], 'Viewport': ['Images'], 'Commands': [],
    'Diagnostics': [], 'DocumentViews': ['Code'], 'Web': [],
}


def select_modules(names):
    selected = set()
    def require(name):
        if name == 'Core':
            return
        if name not in DEPENDENCIES:
            raise ValueError(f'Unknown module: {name}')
        if name in selected:
            return
        for dependency in DEPENDENCIES[name]:
            require(dependency)
        selected.add(name)
    for name in names:
        for item in (DEPENDENCIES if name == 'All' else [name]):
            require(item)
    return sorted(selected)


def validate_output(output):
    output = Path(output).resolve()
    # Refuse overwrites, symlinks and partial replacement of an installed plugin.
    if output.exists() or output.is_symlink():
        raise FileExistsError(f'Output already exists: {output}. Choose a new directory.')
    if output == ROOT or ROOT in output.parents and output.parts[len(ROOT.parts)].casefold() in ('src', 'include', 'third_party', 'integrations'):
        raise ValueError('Output must not overlap the CyUI source directories')
    return output


def prepare_plugin(output, modules=('Core',), gallery=False):
    output = validate_output(output)
    selected = select_modules(modules)
    shutil.copytree(ROOT/'integrations/unreal/CyUI', output)
    library = output/'ThirdParty/CyUI'
    shutil.copytree(ROOT/'include', library/'include')
    sources = ['src/Theme.cpp', 'src/Widgets.cpp', 'src/Layout.cpp']
    sources += [f'src/modules/{name}.cpp' for name in selected]
    if gallery:
        sources.append('src/Gallery.cpp')
    (library/'src/modules').mkdir(parents=True)
    shutil.copy2(ROOT/'src/modules/ModuleInternal.h', library/'src/modules/ModuleInternal.h')
    for source in sources:
        target = library/source
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT/source, target)
    imgui = library/'third_party/imgui'
    imgui.mkdir(parents=True)
    for name in ('imgui.h', 'imgui_internal.h', 'imconfig.h', 'imstb_rectpack.h', 'imstb_textedit.h', 'imstb_truetype.h', 'LICENSE.txt'):
        shutil.copy2(ROOT/'third_party/imgui'/name, imgui/name)
    for name in ('imgui', 'imgui_draw', 'imgui_tables', 'imgui_widgets'):
        source = f'third_party/imgui/{name}.cpp'
        shutil.copy2(ROOT/source, library/source)
        sources.append(source)
    if 'CodeEditor' in selected:
        shutil.copytree(ROOT/'third_party/texteditor', library/'third_party/texteditor')
        sources.append('third_party/texteditor/TextEditor.cpp')
    private = output/'Source/CyUI/Private'
    for index, source in enumerate(sources):
        vendor = source.startswith('third_party/') or source == 'src/modules/CodeEditor.cpp'
        (private/f'CyUILibrary_{index:02}_{Path(source).stem}.cpp').write_text(
            '// Generated source bridge; the implementation remains in ThirdParty/CyUI.\n'
            '#include "CoreTypes.h"\n'
            + ('#if defined(_MSC_VER)\n#pragma warning(push)\n#pragma warning(disable: 4668 4800)\n#endif\n' if vendor else '')
            + f'#include "../../../ThirdParty/CyUI/{source}"\n'
            + ('#if defined(_MSC_VER)\n#pragma warning(pop)\n#endif\n' if vendor else ''), encoding='utf8')
    rules = output/'Source/CyUI/CyUI.Build.cs'
    features = selected + (['Gallery'] if gallery else [])
    rules.write_text(rules.read_text(encoding='utf8').replace('/*CYUI_MODULES*/', ', '.join('"'+s.upper()+'"' for s in features)), encoding='utf8')
    for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
        shutil.copy2(ROOT/name, output/name)
        shutil.copy2(ROOT/name, library/name)
    notices = output/'THIRD_PARTY_NOTICES.md'
    notices.write_text(notices.read_text(encoding='utf8').replace('(third_party/', '(ThirdParty/CyUI/third_party/'), encoding='utf8')
    (output/'.gitattributes').write_text('ThirdParty/CyUI/third_party/** -text !eol\n', encoding='utf8')
    shutil.copytree(ROOT/'docs', library/'docs', ignore=shutil.ignore_patterns('screenshots'))
    (output/'Config').mkdir(exist_ok=True)
    (output/'Config/FilterPlugin.ini').write_text('[FilterPlugin]\n/ThirdParty/...\n/LICENSE\n/THIRD_PARTY_NOTICES.md\n', encoding='utf8')
    return selected


def prepare_module(output, modules=('Core',), gallery=False):
    """A relocatable Source/CyUI folder, with no .uplugin or outside files required.

    UBT discovers .cpp recursively inside a module. Library implementations are
    therefore stored byte-for-byte as .inl and compiled only by source bridges.
    """
    output = validate_output(output)
    with tempfile.TemporaryDirectory(prefix='cyui-module-') as temporary:
        staging = Path(temporary)/'CyUI'
        selected = prepare_plugin(staging, modules, gallery)
        shutil.copytree(staging/'Source/CyUI', output)
        library = output/'Library'
        shutil.copytree(staging/'ThirdParty/CyUI', library)
        sources = {}
        for source in sorted(library.rglob('*.cpp')):
            target = source.with_suffix('.inl')
            sources[source.relative_to(library).as_posix()] = target.relative_to(library).as_posix()
            source.rename(target)
        for bridge in (output/'Private').glob('CyUILibrary_*.cpp'):
            text = bridge.read_text(encoding='utf8')
            for original, target in sources.items():
                text = text.replace('../../../ThirdParty/CyUI/'+original, '../Library/'+target)
            bridge.write_text(text, encoding='utf8')
        version = library/'third_party/texteditor/VERSION.json'
        if version.exists():
            data = json.loads(version.read_text(encoding='utf8'))
            data['files']['TextEditor.inl'] = data['files'].pop('TextEditor.cpp')
            data['packaged_paths'] = {'TextEditor.cpp': 'TextEditor.inl'}
            version.write_text(json.dumps(data, indent=2)+'\n', encoding='utf8')
        (library/'SOURCE_LAYOUT.json').write_text(json.dumps(sources, indent=2)+'\n', encoding='utf8')
        for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
            shutil.copy2(ROOT/name, output/name)
        notices = output/'THIRD_PARTY_NOTICES.md'
        notices.write_text(notices.read_text(encoding='utf8').replace('(third_party/', '(Library/third_party/'), encoding='utf8')
        (output/'.gitattributes').write_text('Library/third_party/** -text !eol\n', encoding='utf8')
        (output/'ModuleDescriptor.json').write_text(json.dumps({
            'Name': 'CyUI', 'Type': 'Editor', 'LoadingPhase': 'PostEngineInit'
        }, indent=2)+'\n', encoding='utf8')
    return selected


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='New Source/CyUI directory or plugin staging directory')
    parser.add_argument('--layout', choices=['module', 'plugin'], default='module', help='Default: reusable module to embed in an existing plugin')
    parser.add_argument('--modules', nargs='+', default=['Core'], choices=['Core', 'All', *DEPENDENCIES])
    parser.add_argument('--gallery', action='store_true')
    args = parser.parse_args()
    prepare = prepare_module if args.layout == 'module' else prepare_plugin
    chosen = prepare(args.output, args.modules, args.gallery)
    print('Prepared CyUI editor '+args.layout+' with Core'+(', '+', '.join(chosen) if chosen else '')+(' and Gallery' if args.gallery else ''))
