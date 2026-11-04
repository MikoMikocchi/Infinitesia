#!/usr/bin/env python3
"""Checks the layering of src/ against the #include graph.

Every directory under src/ (possibly nested, like domains/natural/geology) belongs to a module (presentation and app are compiled into
the executable, the rest are static libraries), and a module may include only the
modules listed for it below (plus itself and the standard/third-party headers).
Run from anywhere:  python tools/check_dependencies.py
Exit code 1 lists every include that breaks the rules.

To add a domain: give it a line in MODULE_OF and ALLOWED, and a matching infinitesia_add_module(...)
in CMakeLists.txt. Dependencies go downwards only; never sideways between domains.
"""

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"

# path under src/ -> module (the longest matching prefix wins)
MODULE_OF = {
    "core": "core",
    "content": "content",
    "domains/natural/chemistry": "chemistry",
    "domains/natural/planetary": "planet",
    "matter": "matter",
    "domains/natural/geology": "geology",
    "domains/natural/biology": "biology",
    "blocks": "blocks",
    "world": "world",
    "simulation": "simulation",
    "gameplay": "gameplay",
    "presentation": "presentation",  # compiled into the executable, but a layer of its own
    "app": "app",
}

# module -> modules it may include. Keep in step with CMakeLists.txt.
ALLOWED = {
    "core": set(),
    # content reads XML definitions into typed registries; it owns pugixml, so
    # no other module includes it (docs/DataDriven.md, sections 4.6 and 9).
    "content": {"core"},
    "chemistry": {"core"},
    "planet": {"core"},
    # matter is the one list of what things are made of (docs/06_BLOCKS.md); it reads its
    # XML through the content layer and also owns the JSON helpers the catalog loaders of
    # geology and blocks share.
    "matter": {"core", "content", "chemistry"},
    # geology takes from planet only the data types (planet/geology/Province.h, PlanetEpochs.h);
    # GeologyModel reads the planet through GeologyPlanetView, never planet/PlanetMap.h.
    "geology": {"core", "chemistry", "planet", "matter"},
    # biology takes from planet only the data types it reads (climate and soil fields), like geology.
    "biology": {"core", "planet", "matter"},
    "blocks": {"core", "chemistry", "matter"},
    "world": {"core", "planet", "matter", "geology", "biology", "blocks"},
    "simulation": {"core"},
    "gameplay": {"core", "planet", "matter", "geology", "blocks", "world", "simulation"},
    "presentation": {"core", "chemistry", "planet", "matter", "geology", "blocks", "world", "simulation", "gameplay"},
    "app": {"core", "chemistry", "planet", "matter", "geology", "biology", "blocks", "world", "simulation", "gameplay", "presentation"},
}

# Include prefixes that are not part of src/.
THIRD_PARTY = {"nlohmann", "doctest"}

INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')


def module_of(parts: tuple[str, ...]) -> str:
    """Module of a path under src/, given as its components (the file name included)."""
    for length in range(len(parts) - 1, 0, -1):
        prefix = "/".join(parts[:length])
        if prefix in MODULE_OF:
            return MODULE_OF[prefix]
    raise SystemExit(f"src/{'/'.join(parts[:-1])}/ belongs to no module: add it to MODULE_OF and ALLOWED")


def main() -> int:
    violations = []
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        relative = path.relative_to(SRC)
        if len(relative.parts) < 2:
            continue  # src/main.cpp: the executable's entry point
        owner = module_of(relative.parts)
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
            dependency = module_of(tuple(parts))
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
