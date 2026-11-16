"""Capture the native SDL3 Markdown playground on Windows/Linux/macOS; needs Pillow."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
THEMES = ['mid-grey', 'light-grey', 'amber-night', 'violet', 'navy']


def capture(exe, folder, view, theme, density, mode='live', locale='fr', scenario='default', frames=44):
    name = f'{view}-{mode}-{theme}-{density}-{locale}'
    if scenario != 'default':
        name += f'-{scenario}'
    if frames != 44:
        name += f'-frame-{frames}'
    bitmap = folder / f'{name}.bmp'
    output = folder / f'{name}.png'
    command = [str(exe), '--view', view, '--document-mode', mode,
               '--theme', theme, '--locale', locale, '--capture', str(bitmap)]
    if scenario != 'default':
        command += ['--scenario', scenario]
    command += ['--frames', str(frames)]
    if density == 'comfortable':
        command.append('--comfortable')
    subprocess.run(command, check=True, timeout=30)
    with Image.open(bitmap) as image:
        image.convert('RGB').save(output)
    bitmap.unlink()
    return output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, default=ROOT / ('build/cyui_markdown_test.exe' if sys.platform == 'win32' else 'build/cyui_markdown_test'))
    args = parser.parse_args()
    folder = ROOT / 'build/markdown-review'
    folder.mkdir(parents=True, exist_ok=True)
    jobs = [(view, theme, density) for view in ['chat', 'markdown']
            for density in ['compact', 'comfortable'] for theme in THEMES]
    jobs += [('markdown', 'mid-grey', 'compact', mode) for mode in ['split', 'source', 'reading']]
    jobs += [('chat', 'light-grey', 'comfortable', 'split', 'en'),
             ('markdown', 'light-grey', 'comfortable', 'split', 'en')]
    jobs += [('chat', 'mid-grey', 'compact', 'live', 'fr', state)
             for state in ['hover', 'edit', 'reply', 'pending', 'lightbox']]
    jobs += [('markdown', theme, density, 'live', 'fr', 'document-edit')
             for density in ['compact', 'comfortable'] for theme in THEMES]
    jobs += [('chat', theme, 'compact', 'live', 'fr', 'million-chat') for theme in THEMES]
    jobs += [('markdown', theme, density, 'live', 'fr', 'document-video', frames)
             for density in ['compact', 'comfortable'] for theme in THEMES for frames in [12, 64]]
    subprocess.run([str(args.exe.resolve()), '--self-test'], check=True, timeout=30)
    with ThreadPoolExecutor(max_workers=2) as pool:
        pending = [pool.submit(capture, args.exe.resolve(), folder, *job) for job in jobs]
        for n, result in enumerate(as_completed(pending), 1):
            result.result()
            if n % 5 == 0:
                print(f'{n}/{len(jobs)} captures verified', flush=True)
    for view in ['chat', 'markdown']:
        sheet = Image.new('RGB', (480 * 5, 350 * 2), '#202020')
        draw = ImageDraw.Draw(sheet)
        for row, density in enumerate(['compact', 'comfortable']):
            for col, theme in enumerate(THEMES):
                file = folder / f'{view}-live-{theme}-{density}-fr.png'
                with Image.open(file) as image:
                    image.thumbnail((480, 320))
                    sheet.paste(image, (col * 480, row * 350 + 26))
                draw.text((col * 480 + 8, row * 350 + 6), f'{theme} | {density}', fill='white')
        sheet.save(folder / f'sheet-{view}.png')
    print(folder, flush=True)


if __name__ == '__main__':
    main()
