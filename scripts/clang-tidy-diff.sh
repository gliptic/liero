#!/usr/bin/env bash
# Run clang-tidy on lines changed in HEAD relative to a base git ref.
# Mirrors the .github/workflows/clang-tidy.yml job, so a clean run here
# means a clean run in CI.
#
# Usage:
#   scripts/clang-tidy-diff.sh <build-dir> [base-ref]
#
#   build-dir  Directory containing compile_commands.json
#              (e.g. build/linux-x64).
#   base-ref   Git ref to diff against (default: origin/master).

set -euo pipefail

if [ "$#" -lt 1 ]; then
	cat <<'EOF' >&2
usage: clang-tidy-diff.sh <build-dir> [base-ref]
EOF
	exit 2
fi

build_dir="$1"
base_ref="${2:-origin/master}"

if [ ! -f "${build_dir}/compile_commands.json" ]; then
	echo "error: ${build_dir}/compile_commands.json not found" >&2
	echo "       configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
	exit 1
fi

# Both Ubuntu and Homebrew (after `brew link llvm`) put a clang-tidy-diff
# wrapper on PATH alongside clang-tidy itself.
diff_cmd=""
for cmd in clang-tidy-diff clang-tidy-diff.py; do
	if command -v "$cmd" >/dev/null 2>&1; then
		diff_cmd="$cmd"
		break
	fi
done

if [ -z "$diff_cmd" ]; then
	echo "error: clang-tidy-diff not on PATH" >&2
	echo "       install clang-tidy (apt/brew) or add llvm's bin/ to PATH" >&2
	exit 1
fi

if ! jobs=$(nproc 2>/dev/null); then
	jobs=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

output=$(git diff -U0 "${base_ref}"...HEAD -- 'src/*' \
	| "$diff_cmd" \
		-p1 \
		-path "$build_dir" \
		-j"$jobs" \
		-regex '.*\.(cpp|hpp|h|cc|cxx)$' \
		-quiet)

echo "$output"

if echo "$output" | grep -E "^[^[:space:]]+:[0-9]+:[0-9]+: (warning|error):" >/dev/null; then
	echo "clang-tidy reported issues on changed lines" >&2
	exit 1
fi
