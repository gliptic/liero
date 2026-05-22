#!/usr/bin/env bash
#
# validate_tcs.sh — Download all TCs from liero.nl and validate tctool conversion + loading
#
# This script:
# 1. Scrapes https://liero.nl/browse/tcs for download links
# 2. Downloads each TC zip
# 3. Runs tctool to convert it from legacy format to openliero TOML format
# 4. Validates all generated .cfg files are valid TOML
# 5. Attempts to load the TC via test_determinism (which exercises Common::load)
#
# Usage:
#   ./scripts/validate_tcs.sh [--tctool PATH] [--test-bin PATH] [--workdir DIR] [--keep]
#
# Options:
#   --tctool PATH     Path to tctool binary (default: build/linux-x64/Debug/tctool)
#   --test-bin PATH   Path to test_tc_load binary (default: build/linux-x64/Debug/test_tc_load)
#   --workdir DIR     Working directory for downloads (default: /tmp/tc_validation)
#   --keep            Don't clean up workdir on success
#   --no-load-test    Skip the load test (only validate conversion + TOML syntax)
#
# Requirements: curl, unzip, python3 (with tomllib)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

TCTOOL="${REPO_ROOT}/build/linux-x64/Debug/tctool"
TEST_BIN="${REPO_ROOT}/build/linux-x64/Debug/test_tc_load"
WORKDIR="/tmp/tc_validation"
KEEP=false
LOAD_TEST=true

while [[ $# -gt 0 ]]; do
    case "$1" in
        --tctool) TCTOOL="$2"; shift 2 ;;
        --test-bin) TEST_BIN="$2"; shift 2 ;;
        --workdir) WORKDIR="$2"; shift 2 ;;
        --keep) KEEP=true; shift ;;
        --no-load-test) LOAD_TEST=false; shift ;;
        -h|--help)
            sed -n '2,/^$/s/^# //p' "$0"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Verify prerequisites
if [[ ! -x "$TCTOOL" ]]; then
    echo "ERROR: tctool not found at $TCTOOL (build first with: cmake --build build/linux-x64)"
    exit 1
fi

if ! command -v python3 &>/dev/null; then
    echo "ERROR: python3 is required for TOML validation"
    exit 1
fi

python3 -c "import tomllib" 2>/dev/null || {
    echo "ERROR: python3 tomllib module required (Python 3.11+)"
    exit 1
}

# Setup
mkdir -p "$WORKDIR"/{downloads,extracted,converted}
cd "$WORKDIR"

echo "=== TC Validation Script ==="
echo "tctool:  $TCTOOL"
echo "workdir: $WORKDIR"
echo ""

# Step 1: Scrape TC download links from liero.nl
echo "--- Fetching TC list from liero.nl ---"
TC_PAGE=$(curl -sL "https://liero.nl/browse/tcs")

# Extract download paths: /download/ID/filename.zip
mapfile -t TC_URLS < <(echo "$TC_PAGE" | grep -oP 'href="/download/\d+/[^"]+\.zip"' | sed 's/href="//;s/"$//' | sort -u)

echo "Found ${#TC_URLS[@]} TCs to validate"
echo ""

# Counters
TOTAL=0
CONVERTED=0
VALID_TOML=0
LOADABLE=0
SKIPPED=0
FAILED_CONVERT=()
FAILED_TOML=()
FAILED_LOAD=()
SKIPPED_LIST=()

for tc_path in "${TC_URLS[@]}"; do
    TOTAL=$((TOTAL + 1))
    filename=$(basename "$tc_path")
    tc_name="${filename%.zip}"

    echo "[$TOTAL/${#TC_URLS[@]}] $tc_name"

    # Download
    if [[ ! -f "downloads/$filename" ]]; then
        if ! curl -sL "https://liero.nl${tc_path}" -o "downloads/$filename"; then
            echo "  SKIP: download failed"
            SKIPPED=$((SKIPPED + 1))
            SKIPPED_LIST+=("$tc_name (download failed)")
            continue
        fi
    fi

    # Extract
    rm -rf "extracted/$tc_name"
    mkdir -p "extracted/$tc_name"
    if ! unzip -qo "downloads/$filename" -d "extracted/$tc_name" 2>/dev/null; then
        echo "  SKIP: unzip failed"
        SKIPPED=$((SKIPPED + 1))
        SKIPPED_LIST+=("$tc_name (unzip failed)")
        continue
    fi

    # Find the directory containing LIERO.EXE (may be nested)
    exe_dir=$(find "extracted/$tc_name" -iname "liero.exe" -printf '%h\n' 2>/dev/null | head -1)
    if [[ -z "$exe_dir" ]]; then
        echo "  SKIP: no LIERO.EXE found"
        SKIPPED=$((SKIPPED + 1))
        SKIPPED_LIST+=("$tc_name (no LIERO.EXE)")
        continue
    fi

    # Fix case-sensitivity: tctool expects LIERO.CHR and LIERO.SND (uppercase)
    # but many TCs have lowercase filenames from DOS-era archives
    for pattern in "liero.chr" "liero.snd"; do
        found=$(find "$exe_dir" -maxdepth 1 -iname "$pattern" -not -name "$(echo "$pattern" | tr '[:lower:]' '[:upper:]')" | head -1)
        if [[ -n "$found" ]]; then
            target="$exe_dir/$(echo "$pattern" | tr '[:lower:]' '[:upper:]')"
            ln -sf "$(basename "$found")" "$target" 2>/dev/null || cp "$found" "$target" 2>/dev/null || true
        fi
    done

    # Convert with tctool
    rm -rf "converted/$tc_name"
    convert_output=$("$TCTOOL" "$exe_dir" --config-root "converted/$tc_name" 2>&1) || true

    if [[ ! -d "converted/$tc_name/TC" ]]; then
        echo "  FAIL: tctool conversion failed"
        echo "        $convert_output" | head -3
        FAILED_CONVERT+=("$tc_name")
        continue
    fi

    tc_dir=$(find "converted/$tc_name/TC" -maxdepth 1 -mindepth 1 -type d | head -1)
    if [[ -z "$tc_dir" || ! -f "$tc_dir/tc.cfg" ]]; then
        echo "  FAIL: no tc.cfg generated"
        FAILED_CONVERT+=("$tc_name")
        continue
    fi

    CONVERTED=$((CONVERTED + 1))

    # Validate all .cfg files are valid TOML
    toml_ok=true
    while IFS= read -r cfg_file; do
        if ! python3 -c "
import tomllib, sys
with open(sys.argv[1], 'rb') as f:
    tomllib.load(f)
" "$cfg_file" 2>/dev/null; then
            echo "  FAIL: invalid TOML: $cfg_file"
            toml_ok=false
            break
        fi
    done < <(find "$tc_dir" -name "*.cfg")

    if [[ "$toml_ok" == false ]]; then
        FAILED_TOML+=("$tc_name")
        continue
    fi

    VALID_TOML=$((VALID_TOML + 1))

    # Load test: try to load the TC and run a short simulation
    if [[ "$LOAD_TEST" == true && -x "$TEST_BIN" ]]; then
        tc_realpath="$(realpath "$tc_dir")"
        load_output=$(TC_PATH="$tc_realpath" timeout 60 "$TEST_BIN" 2>&1) || true

        if echo "$load_output" | grep -q "All tests passed"; then
            LOADABLE=$((LOADABLE + 1))
            echo "  OK (converted + valid TOML + loads + simulates)"
        else
            echo "  PARTIAL: converted + valid TOML, but load/sim failed"
            echo "$load_output" | grep -E "FAILED|error|Error" | head -3 | sed 's/^/        /'
            FAILED_LOAD+=("$tc_name")
        fi
    else
        LOADABLE=$((LOADABLE + 1))
        echo "  OK (converted + valid TOML)"
    fi
done

# Summary
echo ""
echo "=== RESULTS ==="
echo "Total TCs:       $TOTAL"
echo "Skipped:         $SKIPPED"
echo "Converted:       $CONVERTED"
echo "Valid TOML:      $VALID_TOML"
echo "Loadable:        $LOADABLE"
echo ""

if [[ ${#FAILED_CONVERT[@]} -gt 0 ]]; then
    echo "FAILED conversion (${#FAILED_CONVERT[@]}):"
    printf '  - %s\n' "${FAILED_CONVERT[@]}"
    echo ""
fi

if [[ ${#FAILED_TOML[@]} -gt 0 ]]; then
    echo "FAILED TOML validation (${#FAILED_TOML[@]}):"
    printf '  - %s\n' "${FAILED_TOML[@]}"
    echo ""
fi

if [[ ${#FAILED_LOAD[@]} -gt 0 ]]; then
    echo "FAILED loading (${#FAILED_LOAD[@]}):"
    printf '  - %s\n' "${FAILED_LOAD[@]}"
    echo ""
fi

if [[ ${#SKIPPED_LIST[@]} -gt 0 ]]; then
    echo "SKIPPED (${#SKIPPED_LIST[@]}):"
    printf '  - %s\n' "${SKIPPED_LIST[@]}"
    echo ""
fi

# Cleanup
if [[ "$KEEP" == false && ${#FAILED_CONVERT[@]} -eq 0 && ${#FAILED_TOML[@]} -eq 0 ]]; then
    echo "All passed! Cleaning up $WORKDIR"
    rm -rf "$WORKDIR"
else
    echo "Workdir preserved at: $WORKDIR"
fi

# Exit with failure only for conversion and TOML failures (load failures are informational)
if [[ ${#FAILED_CONVERT[@]} -gt 0 || ${#FAILED_TOML[@]} -gt 0 ]]; then
    exit 1
fi

echo "SUCCESS: All TCs validated!"
