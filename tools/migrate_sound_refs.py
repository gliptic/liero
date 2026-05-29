#!/usr/bin/env python3
"""Rewrite integer sound indices in weapon/sobject .cfg files as quoted names.

Reads `types.sounds` from a TC's tc.cfg, builds an index -> name map, then
rewrites every `launchSound`, `loopSound`, `exploSound`, `startSound` value
in `weapons/*.cfg` and `sobjects/*.cfg`. Values already quoted are left
alone. -1 (or any out-of-range index) becomes "".

Usage:
    tools/migrate_sound_refs.py data/TC/openliero
"""
import argparse
import re
import sys
from pathlib import Path

SOUND_FIELDS = {"launchSound", "loopSound", "exploSound", "startSound"}

SOUNDS_RE = re.compile(r'sounds\s*=\s*\[([^\]]*)\]')
STRING_RE = re.compile(r'"([^"]*)"')
FIELD_RE = re.compile(r'^(\w+)\s*=\s*(.+?)\s*$')


def load_sound_names(tc_cfg: Path) -> list[str]:
    text = tc_cfg.read_text(encoding="utf-8")
    m = SOUNDS_RE.search(text)
    if not m:
        sys.exit(f"no `sounds = [...]` array found in {tc_cfg}")
    return STRING_RE.findall(m.group(1))


def rewrite_value(idx_str: str, names: list[str]) -> str | None:
    try:
        idx = int(idx_str)
    except ValueError:
        return None
    if 0 <= idx < len(names):
        return f'"{names[idx]}"'
    return '""'


def migrate_file(path: Path, names: list[str]) -> bool:
    changed = False
    out = []
    for line in path.read_text(encoding="utf-8").splitlines(keepends=True):
        m = FIELD_RE.match(line.rstrip("\r\n"))
        if m and m.group(1) in SOUND_FIELDS:
            raw = m.group(2)
            if not (raw.startswith('"') and raw.endswith('"')):
                new = rewrite_value(raw, names)
                if new is not None:
                    eol = line[len(line.rstrip("\r\n")):]
                    out.append(f"{m.group(1)} = {new}{eol}")
                    changed = True
                    continue
        out.append(line)
    if changed:
        path.write_text("".join(out), encoding="utf-8")
    return changed


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("tc_root", type=Path)
    args = p.parse_args()

    names = load_sound_names(args.tc_root / "tc.cfg")
    print(f"loaded {len(names)} sound names from {args.tc_root}/tc.cfg")

    n_changed = 0
    for sub in ("weapons", "sobjects"):
        for cfg in sorted((args.tc_root / sub).glob("*.cfg")):
            if migrate_file(cfg, names):
                n_changed += 1
                print(f"  migrated {cfg.relative_to(args.tc_root)}")
    print(f"rewrote {n_changed} file(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
