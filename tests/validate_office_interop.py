"""Optional real LibreOffice exchange checks, using only generated synthetic files.

Build cyui_test_office, then run this script. The isolated LibreOffice profile and
all conversions stay under the requested ignored build directory.
"""
from pathlib import Path
import argparse
import subprocess
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def local(tag): return tag.rsplit('}', 1)[-1]


def verify_xlsx(path):
    with zipfile.ZipFile(path) as archive:
        xml = ET.fromstring(archive.read('xl/worksheets/sheet1.xml'))
        cell = next(n for n in xml.iter() if local(n.tag) == 'c' and n.get('r') == 'D4')
        value = next(n.text for n in cell if local(n.tag) == 'v')
        assert abs(float(value)-248) < 1e-9, (path, value)


def verify_docx(path, images=True):
    with zipfile.ZipFile(path) as archive:
        xml = ET.fromstring(archive.read('word/document.xml'))
        text = ''.join(n.text or '' for n in xml.iter() if local(n.tag) == 't')
        assert 'Second page — résumé 😀' in text, (path, text)
        assert 'Native table' in text
        assert any(local(n.tag) == 'tbl' for n in xml.iter())
        if images:
            assert any(n.startswith('word/media/') for n in archive.namelist()), path
            assert any(local(n.tag) == 'drawing' for n in xml.iter()), path
        footers = [n for n in archive.namelist() if n.startswith('word/footer') and n.endswith('.xml')]
        assert footers, path
        footer = ET.fromstring(archive.read(footers[0]))
        fields = [v for n in footer.iter() for k, v in n.attrib.items() if local(k) == 'instr']
        fields += [n.text or '' for n in footer.iter() if local(n.tag) == 'instrText']
        names = [instruction.strip().split()[0] for instruction in fields if instruction.strip()]
        assert 'PAGE' in names and 'NUMPAGES' in names, (path, fields)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT/'build/document-final/cyui_test_office.exe')
    parser.add_argument('--soffice', type=Path, default=Path('C:/Program Files/LibreOffice/program/soffice.exe'))
    parser.add_argument('--output', type=Path, default=ROOT/'build/office-validation')
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to((ROOT/'build').resolve()):
        parser.error('Validation output must remain inside build/')
    if not args.soffice.is_file(): parser.error('Set --soffice to a LibreOffice installation')
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(args.exe.resolve()), str(output)], check=True, timeout=30)
    profile = (output/'lo-profile').as_uri()
    for source, target in [('odt','docx'),('docx','odt'),('ods','xlsx'),('xlsx','ods'),('fodt','odt'),('fods','ods')]:
        folder = output/'libreoffice'/source
        folder.mkdir(parents=True, exist_ok=True)
        command = [str(args.soffice), '-env:UserInstallation='+profile, '--headless', '--convert-to', target,
                   '--outdir', str(folder), str(output/('sample.'+source))]
        completed = subprocess.run(command, capture_output=True, text=True, timeout=45)
        if completed.returncode: raise RuntimeError(completed.stderr)
        converted = folder/('sample.'+target)
        if not converted.is_file(): raise RuntimeError(completed.stdout+completed.stderr)
        normalized = folder/('native-roundtrip.'+('xlsx' if source in ('ods','xlsx','fods') else 'docx'))
        subprocess.run([str(args.exe.resolve()), '--convert', str(converted), str(normalized)], check=True, timeout=30)
        if normalized.suffix == '.xlsx': verify_xlsx(normalized)
        else: verify_docx(normalized, images=source != 'fodt')
        if converted.suffix == '.xlsx': verify_xlsx(converted)
        if converted.suffix == '.docx': verify_docx(converted, images=source != 'fodt')
        print(f'{source} -> LibreOffice {target} -> native {normalized.suffix}: passed', flush=True)


if __name__ == '__main__': main()
