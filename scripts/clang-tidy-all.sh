#!/usr/bin/env bash
# Run clang-tidy across every translation unit in build/<preset>/compile_commands.json,
# in parallel. Mirrors the .github/workflows/clang-tidy.yml job so a clean run
# here means a clean run in CI.
#
# Usage:
#   scripts/clang-tidy-all.sh <build-dir>
#
#   build-dir  Directory containing compile_commands.json (e.g. build/linux-x64-ci).
#
# The `-U__clang_analyzer__` extra-arg suppresses the `clang-analyzer` branch
# inside enet.h (vcpkg vendor header) that triggers a parser error under tidy's
# analyzer-aware preprocessor. The analyzer family itself is disabled in
# .clang-tidy; this only avoids the header-parsing fallout.

set -euo pipefail

if [ "$#" -lt 1 ]; then
	cat <<'EOF' >&2
usage: clang-tidy-all.sh <build-dir>
EOF
	exit 2
fi

build_dir="$1"

if [ ! -f "${build_dir}/compile_commands.json" ]; then
	echo "error: ${build_dir}/compile_commands.json not found" >&2
	echo "       configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
	exit 1
fi

if ! jobs=$(nproc 2>/dev/null); then
	jobs=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

# Collect every src/ TU recorded in compile_commands.json. The path filter
# matches the in-repo absolute path so we don't tidy vcpkg or test infra.
# `src/game/metadata.cpp` is generated at configure time from
# tools/cmake/metadata.cpp.in; skip it.
mapfile -t files < <(jq -r '.[] | .file' "${build_dir}/compile_commands.json" \
	| grep -E "/src/.*\.(cpp|cc|cxx)$" \
	| grep -v "/src/game/metadata\.cpp$" \
	| sort -u)

if [ "${#files[@]}" -eq 0 ]; then
	echo "error: no source files found in compile_commands.json" >&2
	exit 1
fi

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT
log="$tmpdir/clang-tidy.log"

printf '%s\n' "${files[@]}" \
	| xargs -P "$jobs" -I{} bash -c '
		clang-tidy -p "$0" --quiet --extra-arg=-U__clang_analyzer__ "$1" 2>&1 \
			| grep -E "^/.*: (warning|error):" \
			| grep -vE "vcpkg_installed/|/build/" || true
	' "$build_dir" {} | sort -u >"$log"

if [ -s "$log" ]; then
	cat "$log"
	echo "clang-tidy reported $(wc -l <"$log") warning/error line(s) tree-wide" >&2
	exit 1
fi
