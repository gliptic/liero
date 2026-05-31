#!/usr/bin/env bash
# Check every C++ source under src/ against the project's clang-format
# config. Useful for triaging existing formatting drift; does not
# modify files. Exits non-zero if any file would change.

set -euo pipefail

if ! command -v clang-format >/dev/null 2>&1; then
	echo "error: clang-format not on PATH" >&2
	exit 1
fi

if ! jobs=$(nproc 2>/dev/null); then
	jobs=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

find src \
	-type f \
	\( -name '*.cpp' -o -name '*.hpp' -o -name '*.cc' -o -name '*.cxx' \
	   -o -name '*.h' \) \
	-print0 \
	| xargs -0 -P"$jobs" -n1 clang-format --dry-run --Werror
