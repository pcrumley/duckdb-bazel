#!/usr/bin/env python3
"""Sync each module version's overlay with its source.json.

For every modules/<name>/<version>/ directory this:
  1. copies MODULE.bazel into overlay/MODULE.bazel (the two must be identical), and
  2. rewrites the "overlay" map in source.json with a sha256 integrity per overlay file.

Run with --check to verify without writing; exits 1 if anything is out of date.
"""

import argparse
import base64
import hashlib
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def integrity(path: pathlib.Path) -> str:
    digest = hashlib.sha256(path.read_bytes()).digest()
    return "sha256-" + base64.b64encode(digest).decode()


def sync(version_dir: pathlib.Path, check: bool) -> bool:
    """Returns True if the directory was already up to date."""
    ok = True
    label = version_dir.relative_to(ROOT)
    overlay_dir = version_dir / "overlay"
    module = version_dir / "MODULE.bazel"
    overlay_module = overlay_dir / "MODULE.bazel"

    if not overlay_module.exists() or overlay_module.read_bytes() != module.read_bytes():
        ok = False
        print(f"{label}: overlay/MODULE.bazel differs from MODULE.bazel")
        if not check:
            overlay_dir.mkdir(exist_ok=True)
            overlay_module.write_bytes(module.read_bytes())

    source_path = version_dir / "source.json"
    source = json.loads(source_path.read_text())
    overlay = {
        path.relative_to(overlay_dir).as_posix(): integrity(path)
        for path in sorted(overlay_dir.rglob("*"))
        if path.is_file()
    }
    if source.get("overlay") != overlay:
        ok = False
        print(f"{label}: source.json overlay hashes are out of date")
        if not check:
            source["overlay"] = overlay
            source_path.write_text(json.dumps(source, indent=4) + "\n")
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="verify only, do not write")
    args = parser.parse_args()

    version_dirs = sorted(p.parent for p in ROOT.glob("modules/*/*/source.json"))
    results = [sync(d, args.check) for d in version_dirs]
    if all(results):
        print("up to date")
        return 0
    if args.check:
        print("run tools/update_integrity.py to fix")
        return 1
    print("updated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
