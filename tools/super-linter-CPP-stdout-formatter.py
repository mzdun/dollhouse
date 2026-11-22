#!/usr/bin/env python3

# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

# pylint: disable=invalid-name
# (the module name matches the other tools/ scripts, not snake_case)

"""
Condense the cpplint part of a super-linter log.

Reads the log on stdin and writes it to stdout unchanged, except for the
block super-linter prints after "Stderr contents for CPP:", which is laid
out as::

    ... Stderr contents for CPP:
    ------
    /tmp/lint/libs/a.cpp:3:  Add #include <string> for string  [build/...] [4]
    Done processing /tmp/lint/libs/a.cpp
    ------

Inside that block the workspace prefix is removed from every path and the
confidence suffix from every finding. Missing includes and missing newlines
at the end of a file are not printed one by one; they are collected and
listed per file after the log:

    super-linter ... 2>&1 | tools/super-linter-CPP-stdout-formatter.py
"""

import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Callable, Iterable, TextIO

MARKER = "Stderr contents for CPP:"
DASHES = "------"

# /tmp/lint is the workspace of a local super-linter run, /github/workspace
# the one inside GitHub Actions
WORKSPACE = re.compile(r"(?:/tmp/lint|/github/workspace)/")
FINDING = re.compile(
    r"^(?P<path>[^:\s]+):(?P<line>\d+):\s+(?P<msg>.*?)\s+"
    r"\[(?P<rule>[^\]]+)\]\s+\[\d+\]$"
)
ADD_HEADER = re.compile(r"^Add (?P<header>.+?) for ")


@dataclass
class Fixes:
    """Fixes collected for one file instead of printing each finding."""

    headers: set[str] = field(default_factory=set)
    missing_newline: bool = False


FixMap = defaultdict[str, Fixes]
Handler = Callable[[FixMap, str, str], bool]


def add_header(fixes: FixMap, path: str, msg: str) -> bool:
    """Collect the header named by a build/include_what_you_use finding."""
    match = ADD_HEADER.match(msg)
    if not match:
        return False
    fixes[path].headers.add(match.group("header"))
    return True


def add_newline(fixes: FixMap, path: str, _msg: str) -> bool:
    """Collect a whitespace/ending_newline finding."""
    fixes[path].missing_newline = True
    return True


HANDLERS: dict[str, Handler] = {
    "build/include_what_you_use": add_header,
    "whitespace/ending_newline": add_newline,
}


def filter_log(lines: Iterable[str], out: TextIO) -> FixMap:
    """Copy the log to out, condensing the cpplint block."""
    fixes: FixMap = defaultdict(Fixes)
    after_marker = False
    in_block = False

    for line in lines:
        text = line.rstrip()
        if text.endswith(MARKER):
            after_marker = True
        elif after_marker and text == DASHES:
            # the first dashes open the block, the second close it
            in_block = not in_block
            after_marker = in_block
        elif in_block:
            text = WORKSPACE.sub("", text)
            match = FINDING.match(text)
            if not match:
                print(text, file=out)
                continue
            path, line_no, msg, rule = match.groups()
            handler = HANDLERS.get(rule)
            if not handler or not handler(fixes, path, msg):
                print(f"{path}:{line_no}:  {msg}  [{rule}]", file=out)
            continue
        print(line, end="", file=out)

    return fixes


def print_fixes(fixes: FixMap, out: TextIO) -> None:
    """List the collected fixes, file by file."""
    if not fixes:
        return
    print("\n=== cpplint: missing includes and newlines ===\n", file=out)
    for path, fix in sorted(fixes.items()):
        print(f"-- {path}", file=out)
        for header in sorted(fix.headers):
            print(header, file=out)
        if fix.missing_newline:
            print("<eof>", file=out)
        print(file=out)


def main() -> None:
    """Filter stdin to stdout."""
    print_fixes(filter_log(sys.stdin, sys.stdout), sys.stdout)


if __name__ == "__main__":
    main()
