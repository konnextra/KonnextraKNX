#!/usr/bin/env python3
"""Sync the project version into VERSION and the root library.properties file."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = ROOT / "VERSION"
CHANGES_FILE = ROOT / "Changes.md"
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")


def pending_changes() -> int:
    """Count the collected release-note lines still sitting in Changes.md.

    Nothing enforces that they were folded into the website's Release Notes before a tag, and this
    script runs at exactly the moment it matters, so it reminds rather than checks.
    """
    if not CHANGES_FILE.exists():
        return 0
    body = CHANGES_FILE.read_text().split("## Changes", 1)[-1]
    body = re.sub(r"<!--.*?-->", "", body, flags=re.DOTALL)  # the template's example line
    return sum(1 for line in body.splitlines() if line.startswith("- "))


def main() -> None:
    if len(sys.argv) != 2:
        print("usage: bump_version.py <new_version>", file=sys.stderr)
        sys.exit(1)

    new_version = sys.argv[1]
    if not VERSION_RE.match(new_version):
        print(f"error: '{new_version}' is not of the form X.Y.Z", file=sys.stderr)
        sys.exit(1)

    lib_properties = ROOT / "library.properties"
    if not lib_properties.exists():
        print("error: library.properties not found at repo root", file=sys.stderr)
        sys.exit(1)

    VERSION_FILE.write_text(new_version + "\n")
    print(f"wrote VERSION ({new_version})")

    text = lib_properties.read_text()
    new_text, count = re.subn(
        r"^version=.*$", f"version={new_version}", text, count=1, flags=re.MULTILINE
    )
    if count != 1:
        print("error: no 'version=' line found in library.properties", file=sys.stderr)
        sys.exit(1)
    lib_properties.write_text(new_text)
    print("updated library.properties")

    waiting = pending_changes()
    if waiting:
        print(
            f"\nChanges.md still holds {waiting} "
            f"{'entry. Rewrite it' if waiting == 1 else 'entries. Rewrite them'} into "
            "the website's Release Notes (Website_/docs/content/{en,de}/releasenotes.md) under the "
            "new version heading, then empty Changes.md."
        )
    else:
        print("\nChanges.md is empty. Check the website's Release Notes already cover this release.")

    print(f"\nVersion set to {new_version}. Next steps (not run automatically):")
    print("  git add VERSION library.properties Changes.md")
    print(f"  git commit -m 'Bump version to {new_version}'")
    print(f"  git tag v{new_version}")
    print("  git push --tags")


if __name__ == "__main__":
    main()
