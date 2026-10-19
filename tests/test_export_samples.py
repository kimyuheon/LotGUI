"""Distribution-boundary checks that do not require platform SDKs."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('lotgui_export', ROOT / 'tools/export_samples.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class ExportSamplesTests(unittest.TestCase):
    def test_only_example_implementations_and_public_headers_are_exported(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            (out / 'CMakeLists.txt').write_text('# sample consumer\n')
            (out / 'keep.txt').write_text('consumer-owned content')
            (out / 'docs').mkdir()
            (out / 'docs/sdk-distribution.md').write_text('consumer build instructions')
            module.export(out)
            self.assertTrue((out / 'sdk/include/core/widget.h').is_file())
            self.assertTrue((out / 'sdk/include/renderer/vulkan/vulkan_renderer.h').is_file())
            self.assertFalse((out / 'sdk/include/renderer/texture_private.h').exists())
            self.assertTrue((out / 'examples/ribbon_demo/plugin_loader_win32.cpp').is_file())
            self.assertTrue((out / 'examples/ribbon_demo/plugin_loader_posix.cpp').is_file())
            self.assertEqual((out / 'keep.txt').read_text(), 'consumer-owned content')
            forbidden = [p for p in out.rglob('*') if p.is_file() and (
                p.suffix in ('.vert', '.frag', '.mm', '.pdb') or
                (p.suffix in ('.cpp', '.c') and 'examples' not in p.relative_to(out).parts))]
            self.assertEqual(forbidden, [])
            self.assertFalse((out / 'docs/3dengine-imgui-parity.md').exists())
            # Re-export remains safe and does not remove consumer customizations.
            module.export(out)
            self.assertTrue((out / 'keep.txt').exists())
            self.assertEqual((out / 'docs/sdk-distribution.md').read_text(), 'consumer build instructions')

    def test_cannot_export_into_development_tree(self):
        with self.assertRaises(ValueError):
            module.export(ROOT / 'sample-output')

    def test_binary_export_requires_a_platform_triplet(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):
                module.export(Path(temp), Path(temp) / 'build', None)

if __name__ == '__main__':
    unittest.main()
