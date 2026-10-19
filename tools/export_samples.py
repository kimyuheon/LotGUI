#!/usr/bin/env python3
"""Stage public examples/headers and, optionally, an installed DesktopSDK.

No Git operations, no source repository removal and no framework .cpp export.
Run on the target OS after its own SDK build; binaries are not cross-compiled.
"""
import argparse
import json
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXAMPLE_DIRS = ('platform_probe', 'list_control_demo', 'ribbon_demo')
PUBLIC_HEADERS = (
    'platform/platform_backend.h', 'platform/platform_event.h', 'platform/runtime_paths.h',
    'text/text_layout.h', 'text/freetype/freetype_text_engine.h',
    'renderer/paint_batch.h', 'renderer/texture.h',
    'renderer/vulkan/vulkan_renderer.h', 'renderer/vulkan/vulkan_embedded_renderer.h',
    'declarative/lotml.h',
)

def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)

def export(destination, build_dir=None, triplet=None, config='Release', cmake='cmake'):
    destination = destination.resolve()
    if destination == ROOT or ROOT in destination.parents:
        raise ValueError('Destination must be outside the development repository')
    if build_dir and (not triplet or not re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)+', triplet)):
        raise ValueError('A lower-case OS-architecture-toolchain --triplet is required')
    if not (destination / 'CMakeLists.txt').is_file():
        raise ValueError('Destination must be an initialized LotGUI_Sample checkout')
    for dirname in EXAMPLE_DIRS:
        for p in (ROOT / 'examples' / dirname).rglob('*'):
            if p.is_file() and p.suffix in ('.cpp', '.h', '.md', '.ttf', '.txt'):
                copy(p, destination / p.relative_to(ROOT))
    for name in ('settings.lotml', 'ribbon.lotml'):
        copy(ROOT / 'examples' / name, destination / 'examples' / name)
    # Public declarations only. Header inline code remains visible by C++ design.
    headers = list(PUBLIC_HEADERS)
    for directory in ('core', 'widgets', 'lotui'):
        headers.extend(str(p.relative_to(ROOT)) for p in (ROOT / directory).glob('*.h'))
    for name in headers:
        copy(ROOT / name, destination / 'sdk' / 'include' / name)
    for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
        copy(ROOT / name, destination / name)
    for p in (ROOT / 'licenses').glob('*'):
        if p.is_file():
            copy(p, destination / 'licenses' / p.name)
    # User-facing API docs, excluding internal engine migration documents.
    for p in (ROOT / 'docs').glob('*.md'):
        if p.name not in ('3dengine-imgui-parity.md', 'sdk-distribution.md'):
            copy(p, destination / 'docs' / p.name)
    for dirname in ('images', 'videos'):
        for p in (ROOT / 'docs' / dirname).glob('*'):
            if p.is_file() and not p.name.startswith('engine-'):
                copy(p, destination / 'docs' / dirname / p.name)
    if build_dir:
        # Install into temporary space first; failed builds leave an existing SDK untouched.
        with tempfile.TemporaryDirectory(prefix='lotgui-sdk-') as temp:
            prefix = Path(temp) / 'sdk'
            subprocess.run([cmake, '--install', str(build_dir.resolve()), '--config', config,
                            '--component', 'DesktopSDK', '--prefix', str(prefix)], check=True)
            package = prefix / 'lib' / 'cmake' / 'LotUI' / 'LotUIConfig.cmake'
            if not package.is_file():
                raise ValueError('DesktopSDK config missing; configure with the updated LotGUI CMake files')
            forbidden = [p for p in prefix.rglob('*') if p.suffix in ('.cpp', '.mm', '.c', '.vert', '.frag', '.pdb')]
            if forbidden:
                raise ValueError(f'Implementation/debug files cannot be exported: {forbidden}')
            info = json.loads((prefix / 'share' / 'LotUI' / f'build-info-{config}.json').read_text())
            os_name = {'Windows': 'windows', 'Darwin': 'macos', 'Linux': 'linux'}.get(info['system'])
            arch = info['architecture'].lower()
            arch = {'amd64': 'x64', 'x86_64': 'x64', 'aarch64': 'arm64'}.get(arch, arch)
            compiler = {'GNU': 'gcc', 'Clang': 'clang', 'AppleClang': 'clang', 'MSVC': 'msvc'}.get(info['compiler_id'])
            expected = f'{os_name}-{arch}-{compiler}'
            if triplet != expected:
                raise ValueError(f'Triplet {triplet!r} does not match producer metadata {expected!r}')
            final = destination / 'sdk' / triplet
            # Merge per-configuration archives (e.g. Debug + Release) and generated imports.
            shutil.copytree(prefix, final, dirs_exist_ok=True)
            info['triplet'] = triplet
            info['source_commit'] = subprocess.check_output(
                ['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
            (final / f'build-info-{config}.json').write_text(json.dumps(info, indent=2)+'\n', encoding='utf-8')
    print('Exported public headers/examples' + (' and DesktopSDK' if build_dir else ' (no binaries built or copied)'))
    print(destination)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--destination', type=Path, default=ROOT.parent / 'LotGUI_Sample')
    p.add_argument('--build-dir', type=Path)
    p.add_argument('--triplet')
    p.add_argument('--config', choices=('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel'), default='Release')
    p.add_argument('--cmake', default='cmake')
    a = p.parse_args()
    export(a.destination, a.build_dir, a.triplet, a.config, a.cmake)
if __name__ == '__main__':
    main()
