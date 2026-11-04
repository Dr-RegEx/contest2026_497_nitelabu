#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Verify the selected local LittleFS module before either build entrance."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", type=Path, required=True)
    parser.add_argument("--pin", type=Path, default=Path(__file__).with_name("local-module.json"))
    args = parser.parse_args()
    try:
        pin = json.loads(args.pin.read_text())
        if not args.module.is_dir():
            raise ValueError("required local module is missing: " + str(args.module))
        for name, expected in pin["files_sha256"].items():
            p = args.module / name
            if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest() != expected:
                raise ValueError("pinned source mismatch: " + str(p))
        header = (args.module / "lfs.h").read_text()
        for macro, key in [("LFS_VERSION", "littlefs_version"),
                           ("LFS_DISK_VERSION", "disk_version")]:
            value = re.search(r"^#define\s+" + macro + r"\s+(0x[0-9a-fA-F]+)\s*$", header, re.M)
            if not value or int(value[1], 16) != int(pin[key], 16):
                raise ValueError("version mismatch: " + macro)
        # Exact selected bytes also permit an exported staging directory.
        # A present repository must additionally have the recorded ancestry.
        if (args.module / ".git").exists():
            head = subprocess.check_output(["git", "-C", str(args.module),
                                            "rev-parse", "HEAD"], text=True).strip()
            if head != pin["repository_head"]:
                raise ValueError("module HEAD mismatch: " + head)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as err:
        print("LittleFS offline preflight FAILED: " + str(err), file=sys.stderr)
        return 1
    print("LittleFS offline preflight OK: local 2.7 / disk 2.1; selected patched bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
