"""Dependency selection and standalone Unreal plugin packaging (no engine required)."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import json
import hashlib

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
            syntax_units=list((ROOT/'src/modules/code').glob('*.cpp'))+list((ROOT/'third_party/lexilla').rglob('*.cxx'))
            self.assertEqual(len(sources), 9+len(syntax_units))
            self.assertTrue((output/'ThirdParty/CyUI/third_party/code-syntax.lock.json').is_file())
            self.assertTrue((output/'ThirdParty/CyUI/third_party/lexilla/License.txt').is_file())
            for source in sources:
                include = '../../../ThirdParty/'+source.read_text().split('#include "../../../ThirdParty/')[1].split('"')[0]
                self.assertTrue((source.parent/include).is_file(), include)
            self.assertFalse((output/'ThirdParty/CyUI/src/modules/Plots.cpp').exists())
            self.assertFalse((output/'ThirdParty/CyUI/src/Gallery.cpp').exists())
            self.assertTrue((output/'ThirdParty/CyUI/third_party/imgui/LICENSE.txt').is_file())
            self.assertTrue((output/'LICENSE').is_file())
            self.assertTrue((output/'.gitattributes').is_file())
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

    def test_embedded_module_is_relocatable_and_has_no_duplicate_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)/'Source/CyUI'
            package.prepare_module(output, ['Diff'])
            self.assertFalse(list(output.rglob('*.uplugin')))
            self.assertEqual(json.loads((output/'ModuleDescriptor.json').read_text())['Name'], 'CyUI')
            self.assertTrue((output/'Public/CyUIModule.h').is_file())
            self.assertIn('Library/third_party/', (output/'THIRD_PARTY_NOTICES.md').read_text())
            self.assertFalse(list((output/'Library').rglob('*.cpp')))
            self.assertFalse((output/'Library/src/modules/Plots.inl').exists())
            self.assertTrue((output/'Library/src/modules/Diff.inl').is_file())
            # Moving the single folder must keep every source bridge valid.
            relocated = Path(directory)/'OtherPlugin/Source/CyUI'
            relocated.parent.mkdir(parents=True)
            output.rename(relocated)
            bridges = list((relocated/'Private').glob('CyUILibrary_*.cpp'))
            syntax_units=list((ROOT/'src/modules/code').glob('*.cpp'))+list((ROOT/'third_party/lexilla').rglob('*.cxx'))
            self.assertEqual(len(bridges), 9+len(syntax_units))
            self.assertFalse(list((relocated/'Library').rglob('*.cxx')))
            self.assertTrue((relocated/'Library/src/modules/code/CodeSyntax.inl').is_file())
            for bridge in bridges:
                include = bridge.read_text().split('#include "../Library/')[1].split('"')[0]
                self.assertTrue((relocated/'Library'/include).is_file(), include)
            layout = json.loads((relocated/'Library/SOURCE_LAYOUT.json').read_text())
            for original, packaged in layout.items():
                self.assertEqual((ROOT/original).read_bytes(), (relocated/'Library'/packaged).read_bytes())
            with self.assertRaises(FileExistsError):
                package.prepare_module(relocated)

    def test_module_preserves_third_party_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)/'CyUI'
            package.prepare_module(output, ['All'], True)
            third_party = output/'Library/third_party/texteditor'
            version = json.loads((third_party/'VERSION.json').read_text())
            for name, digest in version['files'].items():
                self.assertEqual(hashlib.sha256((third_party/name).read_bytes()).hexdigest(), digest)
            rules = (output/'CyUI.Build.cs').read_text()
            self.assertNotIn('"ToolMenus"', rules)
            self.assertNotIn('"UnrealEd"', rules)

    def test_output_outside_copied_trees(self):
        for inside in ('docs/x', 'src/x', 'web/x', 'tools/x', 'examples/x', 'Build2/x'):
            with self.assertRaises(ValueError, msg=inside):
                package.validate_output(ROOT/inside)
        self.assertEqual(package.validate_output(ROOT/'build/cyui-package-test-unused'), (ROOT/'build/cyui-package-test-unused').resolve())

    def test_renamed_embedded_module(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)/'Source/MyToolCyUI'
            package.prepare_module(output, ['Core'], module_name='MyToolCyUI')
            self.assertFalse((output/'CyUI.Build.cs').exists())
            self.assertFalse((output/'Library/third_party/lexilla').exists())
            rules = (output/'MyToolCyUI.Build.cs').read_text()
            self.assertIn('public class MyToolCyUI : ModuleRules', rules)
            self.assertIn('public MyToolCyUI(ReadOnlyTargetRules Target)', rules)
            self.assertIn('"CYUI_UNREAL_MODULE=" + Name', rules)
            self.assertIn('"CYUI_API=" + Name.ToUpperInvariant() + "_API"', rules)
            self.assertEqual(json.loads((output/'ModuleDescriptor.json').read_text())['Name'], 'MyToolCyUI')
            self.assertIn('"MyToolCyUI.Editor.Lifecycle"', (output/'Private/Tests/CyUIEditorTests.cpp').read_text())
            module = (output/'Private/CyUIEditorModule.cpp').read_text()
            self.assertIn('CYUI_IMPLEMENT_MODULE(CYUI_UNREAL_MODULE)', module)
            self.assertNotIn('IMPLEMENT_MODULE(FCyUIEditorModule, CyUI)', module)
            self.assertNotIn('"CyUI"', (output/'Public/CyUIEditorModule.h').read_text())
            for bridge in (output/'Private').glob('CyUILibrary_*.cpp'):
                include = bridge.read_text().split('#include "../Library/')[1].split('"')[0]
                self.assertTrue((output/'Library'/include).is_file(), include)

    def test_renamed_provider_plugin(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)/'MyToolCyUI'
            package.prepare_plugin(output, ['Core'], module_name='MyToolCyUI')
            descriptor = json.loads((output/'CyUI.uplugin').read_text())
            self.assertEqual(descriptor['Modules'][0]['Name'], 'MyToolCyUI')
            self.assertFalse((output/'Source/CyUI').exists())
            self.assertTrue((output/'Source/MyToolCyUI/MyToolCyUI.Build.cs').is_file())
            for source in (output/'Source/MyToolCyUI/Private').glob('CyUILibrary_*.cpp'):
                include = '../../../ThirdParty/'+source.read_text().split('#include "../../../ThirdParty/')[1].split('"')[0]
                self.assertTrue((source.parent/include).is_file(), include)
            for bad in ('', '1CyUI', 'My-UI', 'a'*65):
                with self.assertRaises(ValueError):
                    package.prepare_plugin(Path(directory)/'bad', ['Core'], module_name=bad)

    def test_example_plugin_declares_shared_dependency(self):
        path = ROOT/'examples/unreal/CyUIToolExample'
        descriptor = json.loads((path/'CyUIToolExample.uplugin').read_text())
        self.assertEqual(descriptor['Plugins'], [{'Name':'CyUI','Enabled':True}])
        rules = (path/'Source/CyUIToolExample/CyUIToolExample.Build.cs').read_text()
        self.assertIn('"CyUI"', rules)


if __name__ == '__main__':
    unittest.main(verbosity=2)
