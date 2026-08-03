#!/usr/bin/env python3
"""Sync the project version into VERSION and every lib/*/library.json file."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = ROOT / "VERSION"
CHANGES_FILE = ROOT / "Changes.md"
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")


def pending_changes() -> int:
    """Count the collected release-note lines still sitting in Changes.md.

    Nothing enforces that they were folded into docs/ReleaseNotes.md before a tag, and this
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

    library_jsons = sorted(ROOT.glob("lib/*/library.json"))
    if not library_jsons:
        print("error: no lib/*/library.json files found", file=sys.stderr)
        sys.exit(1)

    VERSION_FILE.write_text(new_version + "\n")
    print(f"wrote VERSION ({new_version})")

    for path in library_jsons:
        data = json.loads(path.read_text())
        data["version"] = new_version
        path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")
        print(f"updated {path.relative_to(ROOT)}")

    waiting = pending_changes()
    if waiting:
        print(
            f"\nChanges.md still holds {waiting} "
            f"{'entry. Rewrite it' if waiting == 1 else 'entries. Rewrite them'} into "
            "docs/ReleaseNotes.md under the new version heading, then empty Changes.md."
        )
    else:
        print("\nChanges.md is empty. Check docs/ReleaseNotes.md already covers this release.")

    rel_paths = " ".join(str(p.relative_to(ROOT)) for p in library_jsons)
    print(f"\nVersion set to {new_version}. Next steps (not run automatically):")
    print(f"  git add VERSION {rel_paths} docs/ReleaseNotes.md Changes.md")
    print(f"  git commit -m 'Bump version to {new_version}'")
    print(f"  git tag v{new_version}")
    print("  git push --tags")


if __name__ == "__main__":
    main()
