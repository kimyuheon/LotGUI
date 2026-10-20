#!/usr/bin/env python3
"""Install a built DesktopSDK into the matching LotGUI_Sample SDK triplet."""

import argparse
import datetime
import json
import re
import subprocess
import sys
from pathlib import Path


def cache_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("//") or line.startswith("#") or "=" not in line:
            continue
        key_with_type, value = line.split("=", 1)
        key = key_with_type.split(":", 1)[0]
        values[key] = value
    return values


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--triplet", required=True)
    parser.add_argument("--config", default="Release",
                        choices=("Release", "Debug", "RelWithDebInfo", "MinSizeRel"))
    args = parser.parse_args()

    source = Path(__file__).resolve().parent.parent
    destination = args.destination.resolve()
    build_dir = args.build_dir.resolve()
    if not re.fullmatch(r"[a-z0-9]+-[a-z0-9]+-[a-z0-9]+", args.triplet):
        parser.error("triplet must be an OS-architecture-toolchain name")
    if not (destination / "CMakeLists.txt").is_file() or not (
        destination / "sdk" / "README.md").is_file():
        parser.error("destination is not a LotGUI_Sample checkout")
    if not (destination / "sdk" / "include" / "lotui" / "lotui.h").is_file():
        parser.error("destination needs its shared sdk/include headers")
    cache_file = build_dir / "CMakeCache.txt"
    if not cache_file.is_file():
        parser.error("build-dir does not contain a configured CMake build")
    cache = cache_values(cache_file)
    if cache.get("LOTUI_BUILD_DESKTOP_SDK") != "ON":
        parser.error("build-dir must have LOTUI_BUILD_DESKTOP_SDK=ON")
    if cache.get("LOTUI_SAMPLE_SDK_TRIPLET") != args.triplet:
        parser.error("triplet does not match the configured build target")
    if Path(cache.get("CMAKE_HOME_DIRECTORY", "")).resolve() != source:
        parser.error("build-dir was not configured from this LotGUI checkout")

    prefix = destination / "sdk" / args.triplet
    subprocess.run([
        "cmake", "--install", str(build_dir), "--config", args.config,
        "--component", "DesktopSDK", "--prefix", str(prefix),
    ], check=True)
    windows = args.triplet.startswith("windows-")
    library_prefix = "" if windows else "lib"
    library_suffix = ".lib" if windows else ".a"
    expected = (
        prefix / "lib" / args.config / (
            library_prefix + "lotui_widgets" + library_suffix),
        prefix / "lib" / args.config / (
            library_prefix + "lotui_vulkan" + library_suffix),
        prefix / "lib" / "cmake" / "LotUI" / "LotUIConfig.cmake",
    )
    missing = [str(path) for path in expected if not path.is_file()]
    if missing:
        raise RuntimeError("incomplete DesktopSDK: " + ", ".join(missing))

    revision = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=source,
        capture_output=True, text=True, check=True).stdout.strip()
    dirty = bool(subprocess.run(
        ["git", "status", "--porcelain"], cwd=source,
        capture_output=True, text=True, check=True).stdout.strip())
    build_info = {
        "schemaVersion": 1,
        "triplet": args.triplet,
        "configuration": args.config,
        "sourceCommit": revision,
        "sourceDirty": dirty,
        "exportedAtUtc": datetime.datetime.now(
            datetime.timezone.utc).isoformat(),
    }
    info_path = prefix / f"build-info-{args.config}.json"
    info_path.write_text(
        json.dumps(build_info, indent=2) + "\n", encoding="utf-8")
    print(f"DesktopSDK staged: {prefix}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
