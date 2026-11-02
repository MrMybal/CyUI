"""Build/link/run each module alone, checking the exact dependency closure."""
import argparse
from pathlib import Path
import subprocess

dependencies = {
    'Core': [], 'Audio': [], 'Gallery': [], 'Canvas': [], 'Plots': ['Canvas'],
    'Curves': ['Plots'], 'Nodes': ['Canvas'], 'Timeline': ['Canvas'],
    'AudioViews': ['Plots'], 'Code': [], 'Diff': ['Code'], 'CodeEditor': ['Code'],
    'Inspector': [], 'Assets': [], 'Images': ['Canvas'], 'Viewport': ['Images'],
    'Commands': [], 'Diagnostics': [], 'DocumentViews': ['Code'], 'Web': [],
}


def closure(name):
    result = set() if name in ('Core', 'Audio', 'Gallery') else {name}
    for dependency in dependencies[name]:
        result |= closure(dependency)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cmake', default='cmake'); parser.add_argument('--generator')
    parser.add_argument('--make-program'); parser.add_argument('--imgui-library', type=Path)
    parser.add_argument('--modules', nargs='*', default=list(dependencies))
    args = parser.parse_args(); root = Path(__file__).resolve().parents[1]
    for module in args.modules:
        directory = root/'build'/('isolated-'+module.lower()); directory.mkdir(parents=True, exist_ok=True)
        command = [args.cmake, '-S', str(root/'tests/module_consumer'), '-B', str(directory), '-DCMAKE_BUILD_TYPE=Release', '-DCYUI_SELECTED_MODULE='+module]
        if args.generator: command += ['-G', args.generator]
        if args.make_program: command += ['-DCMAKE_MAKE_PROGRAM='+args.make_program]
        if args.imgui_library: command += ['-DCYUI_TEST_IMGUI_LIBRARY='+str(args.imgui_library.resolve())]
        with (directory/'configure.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        actual = set(filter(None, (directory/'modules.txt').read_text().split(';')))
        assert actual == closure(module), (module, actual, closure(module))
        with (directory/'build.log').open('w') as log:
            subprocess.run([args.cmake, '--build', str(directory), '--config', 'Release', '--parallel', '4'], stdout=log, stderr=subprocess.STDOUT, check=True)
        executable = next((p for p in (directory/'consumer.exe', directory/'consumer', directory/'Release/consumer.exe') if p.is_file()))
        subprocess.run([str(executable)], check=True, stdout=subprocess.DEVNULL)
        print(f'{module}: isolated build, host ImGui, dependency closure and interactions passed', flush=True)
