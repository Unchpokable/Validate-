#!/usr/bin/env bash
# ============================================================
#   Validate! - Build and Test Runner  (Bash / Linux / macOS)
#   Usage: ./scripts/build_and_test.sh [Debug|Release] [QtDir]
#
#   QtDir  Optional path to Qt installation directory.
#          Overrides the QTDIR environment variable.
#          When QtDir or QTDIR is set, VD_EXTENSION_QT_BASE
#          is enabled and the Qt lib/bin directory is added
#          to the loader path (PATH / LD_LIBRARY_PATH).
# ============================================================

set -u

# ── ANSI colors (disabled when not writing to a terminal) ────
if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
    CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; CYAN=''; BOLD=''; NC=''
fi

# ── Paths ─────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

BUILD_DIR="build"
BUILD_CONFIG="${1:-Debug}"

# ── Resolve Qt directory ──────────────────────────────────────
# Priority: explicit second argument > QTDIR env var
EFFECTIVE_QT_DIR="${2:-}"
if [ -z "$EFFECTIVE_QT_DIR" ] && [ -n "${QTDIR:-}" ]; then
    EFFECTIVE_QT_DIR="$QTDIR"
fi

FAILURES_FILE="$(mktemp)"
trap 'rm -f "$FAILURES_FILE"' EXIT

# ── Helpers ───────────────────────────────────────────────────
banner() {
    echo ""
    echo -e "${CYAN}  ============================================================${NC}"
    echo -e "${CYAN}    $*${NC}"
    echo -e "${CYAN}  ============================================================${NC}"
}

step() {
    echo -e "${YELLOW}[$1]${NC} $2"
}

# ── Header ────────────────────────────────────────────────────
QT_LABEL=""
[ -n "$EFFECTIVE_QT_DIR" ] && QT_LABEL="  Qt: $EFFECTIVE_QT_DIR"
banner "Validate! - Build & Test Runner   [Config: $BUILD_CONFIG]$QT_LABEL"
echo ""

# ── Step 1: CMake configure ───────────────────────────────────
step "1/3" "Checking CMake configuration..."
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    step "1/3" "Running cmake configure..."

    CMAKE_ARGS=("-B" "$BUILD_DIR" "-DCMAKE_BUILD_TYPE=$BUILD_CONFIG")
    if [ -n "$EFFECTIVE_QT_DIR" ]; then
        CMAKE_ARGS+=("-DVD_EXTENSION_QT_BASE=ON")
        # Pass VD_QT_DIR only when an explicit argument was given;
        # otherwise cmake picks up QTDIR from the environment on its own.
        if [ -n "${2:-}" ]; then
            CMAKE_ARGS+=("-DVD_QT_DIR=$EFFECTIVE_QT_DIR")
        fi
        echo "        Qt Base extension: ON"
    fi

    cmake "${CMAKE_ARGS[@]}"
    if [ $? -ne 0 ]; then
        echo -e "${RED}[ERROR] CMake configure failed.${NC}"
        exit 1
    fi
else
    echo "        Build directory found - skipping configure."
    [ -n "$EFFECTIVE_QT_DIR" ] && echo "        Hint: delete '$BUILD_DIR' and re-run to apply Qt flags."
    [ -z "$EFFECTIVE_QT_DIR" ] && echo "        (Delete '$BUILD_DIR' to force reconfigure)"
fi
echo ""

# ── Step 2: Build ─────────────────────────────────────────────
step "2/3" "Building all targets (config: $BUILD_CONFIG)..."

NPROC=4
command -v nproc  >/dev/null 2>&1 && NPROC=$(nproc)
command -v sysctl >/dev/null 2>&1 && NPROC=$(sysctl -n hw.ncpu 2>/dev/null || echo "$NPROC")

cmake --build "$BUILD_DIR" -j"$NPROC"
if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR] Build failed.${NC}"
    exit 1
fi
echo ""

# ── Resolve Qt library path for the test runner ───────────────
# Priority: EFFECTIVE_QT_DIR > VD_QT_DIR in CMakeCache.txt
QT_LIB_DIR=""
if [ -n "$EFFECTIVE_QT_DIR" ]; then
    # macOS: Frameworks/libs live in lib/; Linux: lib/
    QT_LIB_DIR="$EFFECTIVE_QT_DIR/lib"
elif [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    CACHED_QT="$(grep '^VD_QT_DIR:PATH=' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2-)"
    [ -n "$CACHED_QT" ] && QT_LIB_DIR="$CACHED_QT/lib"
fi

if [ -n "$QT_LIB_DIR" ] && [ -d "$QT_LIB_DIR" ]; then
    echo "  Qt libs: $QT_LIB_DIR"
    export PATH="$QT_LIB_DIR/../bin:$PATH"
    if [[ "$OSTYPE" == "darwin"* ]]; then
        export DYLD_LIBRARY_PATH="$QT_LIB_DIR:${DYLD_LIBRARY_PATH:-}"
    else
        export LD_LIBRARY_PATH="$QT_LIB_DIR:${LD_LIBRARY_PATH:-}"
    fi
    echo ""
fi

# ── Locate test binary directory ──────────────────────────────
# Single-config generators (Makefile, Ninja): build/tests/
# Multi-config generators (Ninja MC):         build/tests/<Config>/
BIN_DIR=""
for candidate in "$BUILD_DIR/tests/$BUILD_CONFIG" "$BUILD_DIR/tests"; do
    if ls "$candidate"/test_* >/dev/null 2>&1; then
        BIN_DIR="$candidate"
        break
    fi
done

if [ -z "$BIN_DIR" ]; then
    echo -e "${RED}[ERROR] Could not locate test binaries under $BUILD_DIR/tests/.${NC}"
    echo        "        Ensure the build succeeded for config '$BUILD_CONFIG'."
    exit 1
fi

# ── Step 3: Run tests ─────────────────────────────────────────
# Discover all test_* binaries automatically — no hardcoded list.
mapfile -t SUITE_BINS < <(find "$BIN_DIR" -maxdepth 1 -name 'test_*' -type f | sort)

if [ ${#SUITE_BINS[@]} -eq 0 ]; then
    echo -e "${YELLOW}[WARN] No test binaries found in: $BIN_DIR${NC}"
    echo        "       Ensure the build succeeded for config '$BUILD_CONFIG'."
    exit 1
fi

step "3/3" "Running test suites (${#SUITE_BINS[@]} found)..."
echo ""

declare -a SUITE_NAMES=()
declare -a SUITE_PASSED=()
declare -a SUITE_FAILED=()
declare -a SUITE_STATUS=()

TOTAL_PASSED=0
TOTAL_FAILED=0

for EXE in "${SUITE_BINS[@]}"; do
    SUITE="$(basename "$EXE" .exe)"
    SUITE="${SUITE##*/}"

    pad_len=$(( 52 - ${#SUITE} ))
    pad=$(printf '%*s' "$pad_len" '' | tr ' ' '-')
    echo -e "  ${CYAN}-- Suite: $SUITE $pad${NC}"
    echo ""

    if [ ! -x "$EXE" ]; then
        echo -e "  ${RED}[ERROR] Not executable: $EXE${NC}"
        SUITE_NAMES+=("$SUITE"); SUITE_PASSED+=(0); SUITE_FAILED+=(0); SUITE_STATUS+=("MISSING")
        echo ""
        continue
    fi

    SUITE_OUT="$(mktemp)"

    set +e
    "$EXE" --gtest_color=yes 2>&1 | tee "$SUITE_OUT" | sed 's/^/  /'
    EXE_EXIT="${PIPESTATUS[0]}"
    set -u

    echo ""

    SUITE_P=$(grep '  PASSED  ] ' "$SUITE_OUT" 2>/dev/null \
        | sed 's/.*PASSED  ] \([0-9][0-9]*\).*/\1/' | head -1)
    SUITE_P="${SUITE_P:-0}"

    SUITE_F=$(grep '  FAILED  ] ' "$SUITE_OUT" 2>/dev/null \
        | grep ' listed below' \
        | sed 's/.*FAILED  ] \([0-9][0-9]*\).*/\1/' | head -1)
    SUITE_F="${SUITE_F:-0}"

    grep '  FAILED  ] ' "$SUITE_OUT" 2>/dev/null \
        | grep -v ' listed below' \
        | grep -v ' ms)' \
        | sed "s/.*FAILED  ] /  [FAIL] $SUITE :: /" \
        >> "$FAILURES_FILE" || true

    rm -f "$SUITE_OUT"

    STATUS="PASS"
    if [ "$EXE_EXIT" -ne 0 ] || [ "${SUITE_F:-0}" -gt 0 ]; then
        STATUS="FAIL"
    fi

    SUITE_NAMES+=("$SUITE")
    SUITE_PASSED+=("$SUITE_P")
    SUITE_FAILED+=("$SUITE_F")
    SUITE_STATUS+=("$STATUS")

    TOTAL_PASSED=$(( TOTAL_PASSED + SUITE_P ))
    TOTAL_FAILED=$(( TOTAL_FAILED + SUITE_F ))
done

# ── Final summary ─────────────────────────────────────────────
banner "FINAL SUMMARY"
echo ""

printf "  %-26s  %8s  %8s  %s\n" "Suite" "Passed" "Failed" "Status"
printf "  %s\n" "------------------------------------------------------------"

for i in "${!SUITE_NAMES[@]}"; do
    NAME="${SUITE_NAMES[$i]}"
    P="${SUITE_PASSED[$i]}"
    F="${SUITE_FAILED[$i]}"
    S="${SUITE_STATUS[$i]}"

    case "$S" in
        PASS)    COLOR="$GREEN"  ;;
        MISSING) COLOR="$YELLOW" ;;
        *)       COLOR="$RED"    ;;
    esac

    printf "${COLOR}  %-26s  %8s  %8s  %s${NC}\n" "$NAME" "$P" "$F" "$S"
done

printf "  %s\n" "------------------------------------------------------------"

TOTAL_COLOR="$GREEN"
[ "$TOTAL_FAILED" -gt 0 ] && TOTAL_COLOR="$RED"
printf "${TOTAL_COLOR}  %-26s  %8s  %8s${NC}\n" "TOTAL" "$TOTAL_PASSED" "$TOTAL_FAILED"

if [ -s "$FAILURES_FILE" ]; then
    echo ""
    echo -e "  ${RED}Failed tests:${NC}"
    while IFS= read -r line; do
        echo -e "  ${RED}${line}${NC}"
    done < "$FAILURES_FILE"
fi

echo ""
echo -e "${CYAN}  ============================================================${NC}"
echo ""

[ "$TOTAL_FAILED" -gt 0 ] && exit 1
exit 0
