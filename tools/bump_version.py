#!/usr/bin/env python3
"""Add a new DuckDB version to the registry, starting from an existing one.

    tools/bump_version.py 1.5.5 1.5.6

Copies modules/duckdb/<old> to modules/duckdb/<new>, points it at the new release
archive (URL, integrity, commit id) and adds the version to metadata.json. If the smoke
workspace and the README name <old>, they move to <new>. Then build and test: the overlay
is a copy of the old one and may need changes for the new sources.
"""

import argparse
import base64
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
MODULE_DIR = ROOT / "modules" / "duckdb"
UPSTREAM = "https://github.com/duckdb/duckdb"


def archive_integrity(version: str) -> str:
    url = f"{UPSTREAM}/archive/refs/tags/v{version}.tar.gz"
    print(f"downloading {url}")
    digest = hashlib.sha256()
    with urllib.request.urlopen(url) as response:
        while chunk := response.read(1 << 20):
            digest.update(chunk)
    return "sha256-" + base64.b64encode(digest.digest()).decode()


def source_id(version: str) -> str:
    """Short commit id of the release tag, as `PRAGMA version` reports it."""
    tag = f"refs/tags/v{version}"
    lines = subprocess.run(
        ["git", "ls-remote", UPSTREAM, tag, tag + "^{}"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines()
    if not lines:
        sys.exit(f"no tag v{version} in {UPSTREAM}")
    # An annotated tag lists the commit it points to second, as <tag>^{}.
    return lines[-1].split()[0][:10]


def version_key(version: str) -> list:
    """Orders versions such as 1.5.6, 2.0.1 and 1.5.6.bcr.1."""
    return [(0, int(part)) if part.isdigit() else (1, part) for part in version.split(".")]


class Edit:
    """A set of text replacements in one file, checked before any file is written."""

    def __init__(self, path: pathlib.Path, replacements: dict[str, str]):
        self.path = path
        self.replacements = replacements

    def missing(self) -> list[str]:
        text = self.path.read_text()
        return [old for old in self.replacements if old not in text]

    def apply(self) -> None:
        text = self.path.read_text()
        for old, new in self.replacements.items():
            text = text.replace(old, new)
        self.path.write_text(text)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("old", help="existing version to copy, for example 1.5.5")
    parser.add_argument("new", help="version to add, for example 1.5.6")
    args = parser.parse_args()

    old_dir = MODULE_DIR / args.old
    new_dir = MODULE_DIR / args.new
    if not old_dir.is_dir():
        sys.exit(f"{old_dir.relative_to(ROOT)} does not exist")
    if new_dir.exists():
        sys.exit(f"{new_dir.relative_to(ROOT)} already exists")

    integrity = archive_integrity(args.new)
    old_id = source_id(args.old)
    new_id = source_id(args.new)

    # Everything is checked against the old version's files before anything is written, so
    # a failure leaves the registry as it was.
    version_edits = [
        Edit(old_dir / "MODULE.bazel", {f'version = "{args.old}"': f'version = "{args.new}"'}),
        Edit(
            old_dir / "overlay" / "BUILD.bazel",
            {
                f'DUCKDB_VERSION = "v{args.old}"': f'DUCKDB_VERSION = "v{args.new}"',
                f'DUCKDB_SOURCE_ID = "{old_id}"': f'DUCKDB_SOURCE_ID = "{new_id}"',
            },
        ),
    ]
    for edit in version_edits:
        if missing := edit.missing():
            sys.exit(f"{edit.path.relative_to(ROOT)}: expected to find {missing[0]!r}")

    # The smoke workspace and the README follow the newest version. They are moved only if
    # they name the version being copied; when adding a release to an older line they stay.
    follow_edits = [
        Edit(
            ROOT / "e2e" / "smoke" / "MODULE.bazel",
            {f'bazel_dep(name = "duckdb", version = "{args.old}")': f'bazel_dep(name = "duckdb", version = "{args.new}")'},
        ),
        Edit(
            ROOT / "README.md",
            {
                f'bazel_dep(name = "duckdb", version = "{args.old}")': f'bazel_dep(name = "duckdb", version = "{args.new}")',
                f"\n  {args.old}/\n": f"\n  {args.new}/\n",
            },
        ),
    ]
    follow = all(not edit.missing() for edit in follow_edits)

    shutil.copytree(old_dir, new_dir)

    source_path = new_dir / "source.json"
    source = json.loads(source_path.read_text())
    source["url"] = f"{UPSTREAM}/archive/refs/tags/v{args.new}.tar.gz"
    source["integrity"] = integrity
    source["strip_prefix"] = f"duckdb-{args.new}"
    source_path.write_text(json.dumps(source, indent=4) + "\n")

    for edit in version_edits:
        Edit(new_dir / edit.path.relative_to(old_dir), edit.replacements).apply()

    metadata_path = MODULE_DIR / "metadata.json"
    metadata = json.loads(metadata_path.read_text())
    metadata["versions"] = sorted(set(metadata["versions"]) | {args.new}, key=version_key)
    metadata_path.write_text(json.dumps(metadata, indent=4) + "\n")

    if follow:
        for edit in follow_edits:
            edit.apply()
    else:
        print(f"the smoke workspace and README do not name {args.old}; left as they are")

    subprocess.run([sys.executable, str(ROOT / "tools" / "update_integrity.py")], check=True)
    print(f"added {new_dir.relative_to(ROOT)}; now build and test it from e2e/smoke")
    return 0


if __name__ == "__main__":
    sys.exit(main())
