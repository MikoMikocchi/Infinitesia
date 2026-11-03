#!/usr/bin/env python3
"""Checks the layering of src/ against the #include graph.

Every directory under src/ belongs to a module (presentation and app are compiled into
the executable, the rest are static libraries), and a module may include only the
modules listed for it below (plus itself and the standard/third-party headers).
Run from anywhere:  python tools/check_dependencies.py
Exit code 1 lists every include that breaks the rules.

To add a domain: give it a line in ALLOWED, and a matching infinitesia_add_module(...)
in CMakeLists.txt. Dependencies go downwards only; never sideways between domains.
"""

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"

# directory under src/ -> module
MODULE_OF = {
    "foundation": "foundation",
    "chemistry": "chemistry",
    "planet": "planet",
    "materials": "materials",
    "geology": "geology",
    "blocks": "blocks",
    "world": "world",
    "gameplay": "gameplay",
    "presentation": "presentation",  # compiled into the executable, but a layer of its own
    "app": "app",
}

# module -> modules it may include. Keep in step with CMakeLists.txt.
ALLOWED = {
    "foundation": set(),
    "chemistry": {"foundation"},
    "planet": {"foundation"},
    # materials is the one list of what things are made of (docs/06_BLOCKS.md); it also owns the JSON
    # helpers the catalog loaders of geology and blocks share.
    "materials": {"foundation", "chemistry"},
    # geology takes from planet only the data types (planet/geology/Province.h, PlanetEpochs.h);
    # GeologyModel reads the planet through GeologyPlanetView, never planet/PlanetMap.h.
    "geology": {"foundation", "chemistry", "planet", "materials"},
    "blocks": {"foundation", "chemistry", "materials"},
    "world": {"foundation", "planet", "materials", "geology", "blocks"},
    "gameplay": {"foundation", "planet", "materials", "geology", "blocks", "world"},
    "presentation": {"foundation", "chemistry", "planet", "materials", "geology", "blocks", "world", "gameplay"},
    "app": {"foundation", "chemistry", "planet", "materials", "geology", "blocks", "world", "gameplay", "presentation"},
}

# Include prefixes that are not part of src/.
THIRD_PARTY = {"nlohmann", "doctest"}

INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')


def module_of_dir(directory: str) -> str:
    if directory not in MODULE_OF:
        raise SystemExit(f"src/{directory}/ belongs to no module: add it to MODULE_OF and ALLOWED")
    return MODULE_OF[directory]


def main() -> int:
    violations = []
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        relative = path.relative_to(SRC)
        if len(relative.parts) < 2:
            continue  # src/main.cpp: the executable's entry point
        owner = module_of_dir(relative.parts[0])
        for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            target = match.group(1)
            parts = target.split("/")
            if len(parts) < 2 or parts[0] in THIRD_PARTY:
                continue  # raylib, or third party
            if not (SRC / target).exists():
                if not (path.parent / target).exists():
                    violations.append(f"{relative.as_posix()}:{number}: cannot resolve include {target}")
                continue  # same-directory include
            dependency = module_of_dir(parts[0])
            if dependency != owner and dependency not in ALLOWED[owner]:
                violations.append(f"{relative.as_posix()}:{number}: {owner} must not include {target} ({dependency})")

    if violations:
        print("Layering violations:")
        for violation in violations:
            print("  " + violation)
        return 1
    print("Layering OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
