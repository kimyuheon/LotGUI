"""Distribution checks that do not require a platform SDK build."""

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "export_samples.py"
spec = importlib.util.spec_from_file_location("lotgui_export", SCRIPT)
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)


class ExportSamplesTests(unittest.TestCase):
    def test_cache_values(self):
        with tempfile.TemporaryDirectory() as temp:
            cache = Path(temp) / "CMakeCache.txt"
            cache.write_text("# comment\nLOTUI_BUILD_DESKTOP_SDK:BOOL=ON\n"
                             "LOTUI_SAMPLE_SDK_TRIPLET:STRING=windows-x64-msvc\n")
            self.assertEqual(exporter.cache_values(cache), {
                "LOTUI_BUILD_DESKTOP_SDK": "ON",
                "LOTUI_SAMPLE_SDK_TRIPLET": "windows-x64-msvc",
            })

    def test_requires_existing_shared_headers(self):
        with tempfile.TemporaryDirectory() as temp:
            destination = Path(temp) / "sample"
            (destination / "sdk").mkdir(parents=True)
            (destination / "CMakeLists.txt").write_text("# consumer\n")
            (destination / "sdk" / "README.md").write_text("# SDK\n")
            original = destination / "CMakeLists.txt"
            result = subprocess.run([
                sys.executable, str(SCRIPT), "--destination", str(destination),
                "--build-dir", str(destination / "missing-build"),
                "--triplet", "windows-x64-msvc",
            ], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("shared sdk/include headers", result.stderr)
            self.assertEqual(original.read_text(), "# consumer\n")
            self.assertFalse((destination / "sdk" / "windows-x64-msvc").exists())

    def test_rejects_wrong_triplet_before_install(self):
        with tempfile.TemporaryDirectory() as temp:
            destination = Path(temp) / "sample"
            header = destination / "sdk" / "include" / "lotui" / "lotui.h"
            header.parent.mkdir(parents=True)
            header.write_text("// existing public API\n")
            (destination / "CMakeLists.txt").write_text("# consumer\n")
            (destination / "sdk" / "README.md").write_text("# SDK\n")
            build = Path(temp) / "build"
            build.mkdir()
            (build / "CMakeCache.txt").write_text(
                "LOTUI_BUILD_DESKTOP_SDK:BOOL=ON\n"
                "LOTUI_SAMPLE_SDK_TRIPLET:STRING=windows-x64-msvc\n"
                f"CMAKE_HOME_DIRECTORY:INTERNAL={ROOT}\n")
            result = subprocess.run([
                sys.executable, str(SCRIPT), "--destination", str(destination),
                "--build-dir", str(build), "--triplet", "linux-x64-gcc",
            ], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("triplet does not match", result.stderr)
            self.assertEqual(header.read_text(), "// existing public API\n")
            self.assertFalse((destination / "sdk" / "linux-x64-gcc").exists())


if __name__ == "__main__":
    unittest.main()
