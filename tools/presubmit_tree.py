#!/usr/bin/env python3
"""Lay out a module version's source tree the way a registry does, for testing.

    tools/presubmit_tree.py 1.5.6 /tmp/duckdb-1.5.6

Downloads the release archive named in source.json, checks its integrity, unpacks it
without its top directory and copies the overlay over it. The Bazel Central Registry's
presubmit then runs the test module in bazel/test_module of that tree; run it there with
`bazel test //...` to see what the presubmit will see.
"""

import argparse
import base64
import hashlib
import io
import json
import pathlib
import shutil
import sys
import tarfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
MODULE_DIR = ROOT / "modules" / "duckdb"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("version", help="module version, for example 1.5.6")
    parser.add_argument("directory", type=pathlib.Path, help="directory to create")
    args = parser.parse_args()

    version_dir = MODULE_DIR / args.version
    if not version_dir.is_dir():
        sys.exit(f"{version_dir.relative_to(ROOT)} does not exist")
    if args.directory.exists():
        sys.exit(f"{args.directory} already exists")
    source = json.loads((version_dir / "source.json").read_text())

    print(f"downloading {source['url']}")
    with urllib.request.urlopen(source["url"]) as response:
        archive = response.read()
    integrity = "sha256-" + base64.b64encode(hashlib.sha256(archive).digest()).decode()
    if integrity != source["integrity"]:
        sys.exit(f"archive integrity is {integrity}, source.json says {source['integrity']}")

    prefix = source["strip_prefix"] + "/"
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        members = []
        for member in tar.getmembers():
            if not member.name.startswith(prefix):
                continue
            member.name = member.name[len(prefix):]
            # The archive holds a few symbolic links, which Windows may refuse to create.
            # Nothing the build or the test module uses is one.
            if member.name and not member.issym() and not member.islnk():
                members.append(member)
        tar.extractall(args.directory, members=members, filter="data")

    shutil.copytree(version_dir / "overlay", args.directory, dirs_exist_ok=True)
    print(f"created {args.directory}; the test module is in bazel/test_module")
    return 0


if __name__ == "__main__":
    sys.exit(main())
