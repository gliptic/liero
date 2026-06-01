#!/usr/bin/env bash
# Run clang-format on lines changed in HEAD relative to a base git ref.
# Local convenience for "what would clang-format change on my PR?".
# CI runs the full tree via scripts/clang-format-all.sh — this script
# is a strict subset useful for faster iteration on a single branch.
#
# Usage:
#   scripts/clang-format-diff.sh [base-ref]
#
#   base-ref   Git ref to diff against (default: origin/master).

set -euo pipefail

base_ref="${1:-origin/master}"
clang_format="${CLANG_FORMAT:-clang-format}"

# git-clang-format ships with both Ubuntu's clang-format package and
# Homebrew's llvm formula, and is the only diff helper still present
# in recent LLVM releases.
if ! command -v git-clang-format >/dev/null 2>&1; then
	echo "error: git-clang-format not on PATH" >&2
	echo "       install clang-format (apt/brew) or add llvm's bin/ to PATH" >&2
	exit 1
fi

patch=$(git clang-format \
	--binary "$clang_format" \
	--diff \
	--extensions cpp,hpp,h,cc,cxx \
	"$base_ref" -- 'src/')

# git-clang-format prints one of these strings (and nothing else) when
# the changed lines are already correctly formatted.
case "$patch" in
	"" | "no modified files to format" | \
	"clang-format did not modify any files")
		exit 0
		;;
esac

echo "$patch"
echo "clang-format would change formatting on the lines above" >&2
exit 1
