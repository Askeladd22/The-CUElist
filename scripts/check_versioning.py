#!/usr/bin/env python3
"""Check the canonical The Cuelist VERSION.txt and cumulative CHANGELOG.md.

With --base-sha and --head-sha, also enforce a monotonic version bump
and changes to both files for every repository update.
"""
from __future__ import annotations

import argparse
from datetime import date
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = "VERSION.txt"
CHANGELOG_FILE = "CHANGELOG.md"
SEMVER = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-([A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*))?"
    r"(?:\+([A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*))?$"
)
HEADERS = re.compile(r"^## \[([^\]]+)\] - (\d{4}-\d{2}-\d{2})\s*$", re.M)
COMMIT = re.compile(r"^[a-fA-F0-9]{40}$")


class VersioningError(ValueError):
    pass


def semver(value: str) -> tuple[int, int, int, tuple[str, ...] | None]:
    value = value.strip()
    if "\n" in value or "\r" in value:
        raise VersioningError("VERSION.txt must be one line")
    match = SEMVER.fullmatch(value)
    if match is None:
        raise VersioningError("Not SemVer: " + repr(value))
    prerelease = match.group(4)
    if prerelease is not None:
        for item in prerelease.split("."):
            if item.isdecimal() and (not item.isascii() or
                                     len(item) > 1 and item[0] == "0"):
                raise VersioningError("Numeric prerelease identifier has a leading zero")
    return (int(match[1]), int(match[2]), int(match[3]),
            tuple(prerelease.split(".")) if prerelease else None)


def compare_version(a: str, b: str) -> int:
    """Compare SemVer precedence: -1 if a<b, 0 if equal, 1 if a>b."""
    va, vb = semver(a), semver(b)
    if va[:3] != vb[:3]:
        return 1 if va[:3] > vb[:3] else -1
    pa, pb = va[3], vb[3]
    if pa is None or pb is None:
        return (pa is None) - (pb is None)
    for x, y in zip(pa, pb):
        if x == y:
            continue
        nx, ny = x.isascii() and x.isdigit(), y.isascii() and y.isdigit()
        if nx and ny:
            return 1 if int(x) > int(y) else -1
        if nx != ny:
            return -1 if nx else 1
        return 1 if x > y else -1
    return (len(pa) > len(pb)) - (len(pa) < len(pb))


def check_changelog(version: str, changelog: str) -> None:
    semver(version)
    headers = HEADERS.findall(changelog)
    if not headers:
        raise VersioningError("No versioned changelog sections found")
    if headers[0][0] != version.strip():
        raise VersioningError(
            "First changelog section " + headers[0][0] +
            " differs from VERSION.txt " + version
        )
    seen = set()
    for ver, date_text in headers:
        semver(ver)
        if ver in seen:
            raise VersioningError("Duplicate changelog version: " + ver)
        seen.add(ver)
        try:
            date.fromisoformat(date_text)
        except ValueError as exc:
            raise VersioningError("Invalid changelog date: " + date_text) from exc
    for (later, _), (earlier, _) in zip(headers, headers[1:]):
        if compare_version(later, earlier) <= 0:
            raise VersioningError(
                "Changelog history not sorted newest first: " + later + "/" + earlier
            )
    body_start = changelog.find(
        f"## [{headers[0][0]}] - {headers[0][1]}"
    )
    if body_start < 0:
        raise VersioningError("Cannot locate changelog entry")
    next_version = changelog.find("\n## [", body_start + 5)
    body = changelog[body_start + 5:next_version if next_version >= 0 else None]
    if len(body.strip()) < 90 or not re.search(r"^### .+", body, re.M):
        raise VersioningError("Newest changelog entry lacks categorized change notes")


def git(*args: str) -> str:
    proc = subprocess.run(
        ["git", *args], cwd=ROOT, check=False, text=True, capture_output=True
    )
    if proc.returncode:
        raise VersioningError("git " + " ".join(args) + ": " + proc.stderr.strip())
    return proc.stdout


def check_pr(version: str, base: str, head: str) -> None:
    if not COMMIT.fullmatch(base or "") or not COMMIT.fullmatch(head or ""):
        raise VersioningError("Both base/head must be 40-character Git SHAs")
    changed = set(git("diff", "--name-only", f"{base}...{head}").splitlines())
    missing = {VERSION_FILE, CHANGELOG_FILE} - changed
    if missing:
        raise VersioningError(
            "Every update must modify VERSION.txt and CHANGELOG.md: missing " +
            ", ".join(sorted(missing))
        )
    previous = git("show", f"{base}:{VERSION_FILE}").strip()
    if compare_version(version, previous) <= 0:
        raise VersioningError("Version must increase from " + previous + " to " + version)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-sha")
    parser.add_argument("--head-sha")
    args = parser.parse_args(argv)
    try:
        version = (ROOT / VERSION_FILE).read_text(encoding="utf-8").strip()
        changes = (ROOT / CHANGELOG_FILE).read_text(encoding="utf-8")
        check_changelog(version, changes)
        if (args.base_sha is None) != (args.head_sha is None):
            raise VersioningError("Specify both --base-sha and --head-sha")
        if args.base_sha is not None:
            check_pr(version, args.base_sha, args.head_sha)
        print("PASS The Cuelist: VERSION.txt and CHANGELOG.md aligned at " + version)
        return 0
    except (VersioningError, OSError) as exc:
        print("FAIL versioning: " + str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
