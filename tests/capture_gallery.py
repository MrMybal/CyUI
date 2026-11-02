"""Windows/DX11 visual review matrix. Outputs remain under ignored build/."""
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import argparse
import subprocess
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
themes = ['mid-grey', 'light-grey', 'amber-night', 'violet', 'navy']


def capture(executable, folder, theme, density, tab, force=False, locale='en', gallery_only=False, image_graph=False):
    name = f'{tab:02}-{theme}-{density}'
    bitmap, output = folder/(name+'.bmp'), folder/(name+'.png')
    if output.is_file() and not force:
        return output
    command = [str(executable), '--capture', str(bitmap), '--theme', theme, '--tab', str(tab)]
    command += ['--locale', locale]
    if gallery_only:
        command.append('--gallery-only')
    if image_graph:
        command.append('--image-graph')
    if density == 'comfortable':
        command.append('--comfortable')
    startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow = 0
    subprocess.run(command, check=True, timeout=30, startupinfo=startup)
    if not bitmap.is_file():
        raise RuntimeError(f'Missing capture: {name}')
    with Image.open(bitmap) as image:
        image.convert('RGB').save(output)
    bitmap.unlink()
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, default=root/'build/cyui_demo.exe')
    parser.add_argument('--tabs', nargs='+', type=int, default=list(range(22)))  # modules, Layout (20), Filtering (21)
    parser.add_argument('--themes', nargs='+', default=themes)
    parser.add_argument('--densities', nargs='+', default=['compact', 'comfortable'])
    parser.add_argument('--force', action='store_true', help='Refresh existing captures')
    parser.add_argument('--locale', choices=['en', 'fr'], default='en')
    parser.add_argument('--gallery-only', action='store_true')
    parser.add_argument('--image-graph', action='store_true')
    args = parser.parse_args()
    suffix = '' if args.locale == 'en' and not args.gallery_only and not args.image_graph else '-'+args.locale+('-gallery' if args.gallery_only else '')+('-images' if args.image_graph else '')
    folder = root/('build/gallery-review'+suffix); folder.mkdir(parents=True, exist_ok=True)
    jobs = [(theme, density, tab) for tab in args.tabs for density in args.densities for theme in args.themes]
    with ThreadPoolExecutor(max_workers=2) as pool:
        pending = [pool.submit(capture, args.exe.resolve(), folder, *job, args.force, args.locale, args.gallery_only, args.image_graph) for job in jobs]
        for n, task in enumerate(as_completed(pending), 1):
            task.result()
            if n % 10 == 0:
                print(f'{n}/{len(jobs)} captures verified', flush=True)
    for tab in args.tabs:
        images = [(theme, density, folder/f'{tab:02}-{theme}-{density}.png') for density in args.densities for theme in args.themes]
        cell_width, cell_height = 480, 320
        sheet = Image.new('RGB', (cell_width*len(args.themes), cell_height*len(args.densities)), '#202020')
        draw = ImageDraw.Draw(sheet)
        for n, (theme, density, file) in enumerate(images):
            x, y = n % len(args.themes)*cell_width, n // len(args.themes)*cell_height
            with Image.open(file) as image:
                image.thumbnail((cell_width, cell_height-24))
                sheet.paste(image, (x, y+24))
            draw.text((x+8, y+4), f'Tab {tab} | {theme} | {density}', fill='white')
        sheet.save(folder/f'sheet-{tab:02}.png')
