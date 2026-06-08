#!/usr/bin/env bash
# Apply clang-tidy auto-fixes tree-wide. Uses the export-fixes /
# clang-apply-replacements two-phase flow with a YAML path-canonicalisation
# step in between — without that step, parallel TUs that reach the same
# header via different relative include paths (e.g.
# src/game/ai/../worm.hpp vs src/game/controller/../worm.hpp) emit
# fixes whose FilePath: strings are textually distinct, so
# clang-apply-replacements skips deduplication and the replacements
# stack 5–7 deep on the same line. `run-clang-tidy -fix` has the same
# bug.
#
# Usage:
#   scripts/clang-tidy-fix.sh <build-dir> [check-filter]
#
#   build-dir     Directory containing compile_commands.json
#                 (e.g. build/linux-x64-ci).
#   check-filter  Optional clang-tidy -checks expression. Default:
#                 use whatever .clang-tidy enables. Examples:
#                   '-*,readability-braces-around-statements'
#                   '-*,modernize-*'
#
# This script modifies source files. Always run on a clean working
# tree so you can inspect the diff afterwards. Some fixes (e.g.
# modernize-*) can unlock further fixes on the next pass; re-run
# until two consecutive runs produce no diff.

set -euo pipefail

if [ "$#" -lt 1 ]; then
	cat <<'EOF' >&2
usage: clang-tidy-fix.sh <build-dir> [check-filter]
EOF
	exit 2
fi

build_dir="$1"
checks="${2:-}"

if [ ! -f "${build_dir}/compile_commands.json" ]; then
	echo "error: ${build_dir}/compile_commands.json not found" >&2
	echo "       configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
	exit 1
fi

for cmd in run-clang-tidy clang-apply-replacements python3; do
	if ! command -v "$cmd" >/dev/null 2>&1; then
		echo "error: $cmd not on PATH" >&2
		exit 1
	fi
done

if ! jobs=$(nproc 2>/dev/null); then
	jobs=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

fixdir=$(mktemp -d)
trap 'rm -rf "$fixdir"' EXIT

tidy_args=(
	-p "$build_dir"
	-header-filter='.*/src/.*'
	-export-fixes "$fixdir"
	-j "$jobs"
	-quiet
)
if [ -n "$checks" ]; then
	tidy_args+=(-checks="$checks")
fi

echo "==> running clang-tidy (export-fixes)" >&2
run-clang-tidy "${tidy_args[@]}" 'src/.*\.(cpp|cc|cxx)$'

echo "==> canonicalising YAML FilePath entries" >&2
python3 - "$fixdir" <<'PY'
import glob, os, re, sys
fixdir = sys.argv[1]
pat = re.compile(r"(FilePath:\s+')([^']+)(')")
def canon(m):
	return m.group(1) + os.path.normpath(m.group(2)) + m.group(3)
n = 0
for f in glob.glob(os.path.join(fixdir, '*.yaml')):
	with open(f) as h: s = h.read()
	new = pat.sub(canon, s)
	if new != s:
		with open(f, 'w') as h: h.write(new)
		n += 1
print(f'    canonicalised paths in {n} YAML file(s)', file=sys.stderr)
PY

echo "==> applying replacements" >&2
clang-apply-replacements "$fixdir"

echo "==> done. Review with: git diff --stat" >&2
