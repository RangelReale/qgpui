#!/usr/bin/env python3
"""Checks the changelog fragments, or moves them into CHANGELOG.md.

Each pull request adds its changelog entries as a file of its own in
changelog.d/, so that pull requests never edit the same lines. A release moves
them into CHANGELOG.md. See docs/decisions/0027-changelog-fragments.md.

    python3 tools/release-changelog.py check
        Checks every fragment. CI runs it on each pull request.

    python3 tools/release-changelog.py release X.Y.Z [--date YYYY-MM-DD]
        Moves the fragments, with the entries already under Unreleased, into a
        new section "## [X.Y.Z]" of CHANGELOG.md, adds its link and deletes
        the fragments. It does not change the version in CMakeLists.txt.

A fragment holds one or more headings of Keep a Changelog, such as
"### Fixed", each with its entries. An entry is a line that starts with "- ",
and the lines under it that are indented. Fragments are read in the order of
their names.

Adapted from tools/release-changelog.py in mirante-core.
"""

import argparse
import datetime
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FRAGMENTS = ROOT / "changelog.d"
CHANGELOG = ROOT / "CHANGELOG.md"

# The order of Keep a Changelog.
KINDS = ["Added", "Changed", "Deprecated", "Removed", "Fixed", "Security"]


class FragmentError(Exception):
    pass


def fragments():
    """The fragment files, in the order of their names."""
    if not FRAGMENTS.is_dir():
        return []
    return sorted(path for path in FRAGMENTS.glob("*.md") if path.name != "README.md")


def read_text(path):
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def write_text(path, text):
    path.write_text(text, encoding="utf-8", newline="\n")


def strip_blank(lines):
    """The lines without blank lines at the start and the end."""
    start, end = 0, len(lines)
    while start < end and not lines[start].strip():
        start += 1
    while end > start and not lines[end - 1].strip():
        end -= 1
    return lines[start:end]


def check_entries(lines, where):
    """Fails unless the lines are entries: "- " lines and indented lines under them."""
    lines = strip_blank(lines)
    if not lines:
        raise FragmentError(f"{where} has no entries.")
    if not lines[0].startswith("- "):
        raise FragmentError(f"{where} must start with an entry, a line that starts with \"- \".")
    for line in lines:
        if line.strip() and not line.startswith("- ") and not line.startswith(" "):
            raise FragmentError(f"{where}: \"{line}\" is neither an entry nor indented under one.")
    return lines


def parse_sections(lines, where):
    """The text before the first heading, and each heading's lines, from the
    lines of a Keep a Changelog section."""
    preamble, sections, kind = [], {}, None
    for line in lines:
        heading = re.match(r"^### (.+?)\s*$", line)
        if heading:
            kind = heading.group(1)
            if kind not in KINDS:
                raise FragmentError(f"{where}: \"### {kind}\" is not one of {', '.join(KINDS)}.")
            sections.setdefault(kind, [])
        elif kind is None:
            preamble.append(line)
        else:
            sections[kind].append(line)
    return strip_blank(preamble), {kind: strip_blank(body) for kind, body in sections.items()}


def read_fragment(path):
    where = path.relative_to(ROOT).as_posix()
    preamble, sections = parse_sections(read_text(path).split("\n"), where)
    if preamble:
        raise FragmentError(f"{where} must start with a heading such as \"### Fixed\".")
    if not sections:
        raise FragmentError(f"{where} has no heading such as \"### Fixed\".")
    return {kind: check_entries(body, f"{where}, under \"### {kind}\"") for kind, body in sections.items()}


def check():
    paths = fragments()
    errors = []
    for path in paths:
        try:
            read_fragment(path)
        except FragmentError as error:
            errors.append(str(error))
    for error in errors:
        print(f"::error::{error}")
    if errors:
        return 1
    print(f"{len(paths)} fragment(s) are well formed.")
    return 0


def add_links(lines, version):
    """Adds the new version's link, comparing from the previous version, and
    moves the Unreleased link to compare from the new version."""
    previous = next((m.group(1) for m in (re.match(r"^## \[(\d+\.\d+\.\d+)\]", line) for line in lines)
                     if m and m.group(1) != version), None)
    # The repository's address, from any version link.
    base = next((m.group(1) for m in (re.match(r"^\[[^\]]+\]: (https://\S+?)/(?:compare|releases|tree)/", line)
                                      for line in lines) if m), None)
    if previous is None or base is None:
        raise FragmentError("CHANGELOG.md has no earlier version with a link, to compare the new one with.")
    link = f"[{version}]: {base}/compare/v{previous}...v{version}"
    unreleased = next((i for i, line in enumerate(lines) if line.startswith("[Unreleased]: ")), None)
    if unreleased is not None:
        lines[unreleased] = f"[Unreleased]: {base}/compare/v{version}...HEAD"
        lines.insert(unreleased + 1, link)
        return
    # Without an Unreleased link, the new link goes above the previous one.
    at = next((i for i, line in enumerate(lines) if line.startswith(f"[{previous}]: ")), None)
    if at is None:
        raise FragmentError(f"CHANGELOG.md has no link for {previous}.")
    lines.insert(at, link)


def release(version, date):
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise FragmentError(f"\"{version}\" is not a version X.Y.Z.")
    lines = read_text(CHANGELOG).split("\n")
    try:
        start = lines.index("## [Unreleased]")
    except ValueError:
        raise FragmentError("CHANGELOG.md has no line \"## [Unreleased]\".")
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("## [")), None)
    if end is None:
        raise FragmentError("CHANGELOG.md has no released section after Unreleased.")
    if any(line == f"## [{version}]" or line.startswith(f"## [{version}] ") for line in lines):
        raise FragmentError(f"CHANGELOG.md already has a section for {version}.")

    # The entries already under Unreleased come first, then the fragments.
    # Every fragment is read and checked before anything is written.
    preamble, sections = parse_sections(lines[start + 1:end], "CHANGELOG.md, under Unreleased")
    paths = fragments()
    for path in paths:
        for kind, body in read_fragment(path).items():
            sections[kind] = sections.get(kind, []) + body
    if not any(sections.values()) and not preamble:
        raise FragmentError("Nothing to release: Unreleased and changelog.d/ are empty.")

    released = [f"## [{version}] - {date}", ""]
    if preamble:
        released += preamble + [""]
    for kind in KINDS:
        if sections.get(kind):
            released += [f"### {kind}", ""] + sections[kind] + [""]
    lines[start + 1:end] = [""] + released

    add_links(lines, version)
    write_text(CHANGELOG, "\n".join(lines))
    for path in paths:
        path.unlink()
    print(f"Moved {len(paths)} fragment(s) into CHANGELOG.md.")
    print(f"Raise the version in project() in CMakeLists.txt to {version}, if it is not already.")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("check", help="check every fragment")
    release_command = commands.add_parser("release", help="move the fragments into CHANGELOG.md")
    release_command.add_argument("version", help="the new version, X.Y.Z")
    release_command.add_argument("--date", default=datetime.date.today().isoformat(),
                                 help="the release date, YYYY-MM-DD; today by default")
    arguments = parser.parse_args()
    try:
        if arguments.command == "check":
            return check()
        return release(arguments.version, arguments.date)
    except FragmentError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
