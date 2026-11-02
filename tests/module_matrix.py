"""Build/link/run each module alone, checking the exact dependency closure.

Each module is configured through tests/module_consumer, which sets CyUI's options with
plain set() before add_subdirectory (the documented host pattern), links one CyUI::<Module>
target against a host ImGui and runs its checks: tests/features.cpp for Core and the
optional modules, tests/module_smoke.cpp for Audio and Gallery.
"""
import argparse
from pathlib import Path
import subprocess
import sys

dependencies = {
    'Core': [], 'Audio': [], 'Gallery': [], 'Canvas': [], 'Plots': ['Canvas'],
    'Curves': ['Plots'], 'Nodes': ['Canvas'], 'Timeline': ['Canvas'],
    'AudioViews': ['Plots'], 'Code': [], 'Diff': ['Code'], 'CodeEditor': ['Code'],
    'Inspector': [], 'Assets': [], 'Images': ['Canvas'], 'Viewport': ['Images'],
    'Commands': [], 'Diagnostics': [], 'DocumentViews': ['Code'], 'Web': [],
}
# Modules whose consumer runs module_smoke.cpp (a link and a few calls), not features.cpp.
smoke_only = {'Audio', 'Gallery'}


def closure(name):
    result = set() if name in ('Core', 'Audio', 'Gallery') else {name}
    for dependency in dependencies[name]:
        result |= closure(dependency)
    return result


def find_executable(directory, config):
    """The consumer binary, for single-config (Makefiles, Ninja) and multi-config
    (Visual Studio, Xcode, Ninja Multi-Config) generators, on every OS."""
    names = ('consumer.exe', 'consumer')
    for folder in (directory, directory/config):
        for name in names:
            if (folder/name).is_file():
                return folder/name
    found = sorted(p for name in names for p in directory.rglob(name) if p.is_file() and 'CMakeFiles' not in p.parts)
    if found:
        return found[0]
    raise FileNotFoundError(f'No consumer executable under {directory}; see {directory/"build.log"}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cmake', default='cmake'); parser.add_argument('--generator')
    parser.add_argument('--make-program'); parser.add_argument('--imgui-library', type=Path,
                        help='A prebuilt host ImGui library, to skip compiling ImGui for every module')
    parser.add_argument('--modules', nargs='*', default=list(dependencies))
    parser.add_argument('--build-dir', type=Path, help='Parent folder of the per-module builds (default: build/)')
    parser.add_argument('--config', default='Release'); parser.add_argument('--parallel', default='4')
    args = parser.parse_args(); root = Path(__file__).resolve().parents[1]
    unknown = [module for module in args.modules if module not in dependencies]
    if unknown:
        parser.error(f'unknown modules: {", ".join(unknown)}')
    parent = (args.build_dir or root/'build').resolve()
    for module in args.modules:
        directory = parent/('isolated-'+module.lower()); directory.mkdir(parents=True, exist_ok=True)
        # A fresh cache each time: the first configure is the one a host's plain set() must survive.
        (directory/'CMakeCache.txt').unlink(missing_ok=True)
        command = [args.cmake, '-S', str(root/'tests/module_consumer'), '-B', str(directory), '-DCMAKE_BUILD_TYPE='+args.config, '-DCYUI_SELECTED_MODULE='+module]
        if args.generator: command += ['-G', args.generator]
        if args.make_program: command += ['-DCMAKE_MAKE_PROGRAM='+args.make_program]
        if args.imgui_library: command += ['-DCYUI_TEST_IMGUI_LIBRARY='+str(args.imgui_library.resolve())]
        try:
            with (directory/'configure.log').open('w') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
            actual = set(filter(None, (directory/'modules.txt').read_text().split(';')))
            assert actual == closure(module), (module, actual, closure(module))
            with (directory/'build.log').open('w') as log:
                subprocess.run([args.cmake, '--build', str(directory), '--config', args.config, '--parallel', args.parallel], stdout=log, stderr=subprocess.STDOUT, check=True)
            executable = find_executable(directory, args.config)
            subprocess.run([str(executable)], check=True, stdout=subprocess.DEVNULL)
        except subprocess.CalledProcessError as error:
            logs = [directory/name for name in ('configure.log', 'build.log') if (directory/name).is_file()]
            for log in logs[-1:]:
                sys.stdout.write(log.read_text(errors='replace')[-4000:])
            sys.exit(f'{module}: failed ({error})')
        checks = 'smoke checks (module_smoke.cpp)' if module in smoke_only else 'feature checks (features.cpp)'
        print(f'{module}: isolated build, host ImGui, dependency closure and {checks} passed', flush=True)
