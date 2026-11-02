"""Dependency selection and standalone Unreal plugin packaging (no engine required)."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import json

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('package_unreal', ROOT/'tools/package_unreal.py')
package = importlib.util.module_from_spec(spec); spec.loader.exec_module(package)


class UnrealPackage(unittest.TestCase):
    def test_selection(self):
        self.assertEqual(package.select_modules(['Core']), [])
        self.assertEqual(package.select_modules(['Diff']), ['Code', 'Diff'])
        self.assertEqual(package.select_modules(['AudioViews']), ['AudioViews', 'Canvas', 'Plots'])
        with self.assertRaises(ValueError):
            package.select_modules(['Missing'])

    def test_self_contained_selective_plugin(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)/'CyUI'
            self.assertEqual(package.prepare_plugin(output, ['Diff']), ['Code', 'Diff'])
            descriptor = json.loads((output/'CyUI.uplugin').read_text())
            self.assertEqual(descriptor['Modules'][0]['Type'], 'Editor')
            self.assertFalse(descriptor['EnabledByDefault'])
            rules = (output/'Source/CyUI/CyUI.Build.cs').read_text()
            self.assertIn('"CODE", "DIFF"', rules)
            self.assertNotIn('/*CYUI_MODULES*/', rules)
            sources = list((output/'Source/CyUI/Private').glob('CyUILibrary_*.cpp'))
            self.assertEqual(len(sources), 9)  # 3 core + 2 selected + 4 imgui
            for source in sources:
                include = '../../../ThirdParty/'+source.read_text().split('#include "../../../ThirdParty/')[1].split('"')[0]
                self.assertTrue((source.parent/include).is_file(), include)
            self.assertFalse((output/'ThirdParty/CyUI/src/modules/Plots.cpp').exists())
            self.assertFalse((output/'ThirdParty/CyUI/src/Gallery.cpp').exists())
            self.assertTrue((output/'ThirdParty/CyUI/third_party/imgui/LICENSE.txt').is_file())
            self.assertTrue((output/'LICENSE').is_file())
            with self.assertRaises(FileExistsError):
                package.prepare_plugin(output, ['Core'])

    def test_all_and_gallery(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)/'CyUI'
            chosen = package.prepare_plugin(output, ['All'], True)
            self.assertEqual(set(chosen), set(package.DEPENDENCIES))
            self.assertTrue((output/'ThirdParty/CyUI/src/Gallery.cpp').is_file())
            self.assertTrue((output/'ThirdParty/CyUI/third_party/texteditor/LICENSE').is_file())
            self.assertIn('"GALLERY"', (output/'Source/CyUI/CyUI.Build.cs').read_text())


if __name__ == '__main__':
    unittest.main(verbosity=2)
