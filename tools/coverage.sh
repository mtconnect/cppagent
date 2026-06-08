#!/usr/bin/env bash
#
# Generate an LLVM source-based code-coverage report for the cppagent project.
#
# Prerequisites: build the agent + tests with coverage instrumentation, e.g.
#
#   conan build . --build=missing -pr conan/profiles/macos \
#     -o "&:development=True" -o "&:coverage=True" -o "&:shared=True"
#
#   # or, with plain CMake:
#   cmake -S . -B <build> -DDEVELOPMENT=ON -DSHARED_AGENT_LIB=ON -DAGENT_ENABLE_COVERAGE=ON
#   cmake --build <build> -j
#
# Usage:
#   tools/coverage.sh [BUILD_DIR]
#
#   BUILD_DIR  Directory containing the instrumented build (with CMakeCache.txt).
#              May also be set via COVERAGE_BUILD_DIR. Defaults to ./build.
#
# Output:
#   <BUILD_DIR>/coverage_report.txt   per-file summary
#   <BUILD_DIR>/coverage_html/        browsable line-by-line HTML
#
# Env knobs:
#   REUSE_PROFILES=1   skip running ctest, reuse existing cov/*.profraw
#   CTEST_ARGS="..."   extra args passed to ctest (default: -j<ncpu>)

set -euo pipefail

BUILD_DIR="${1:-${COVERAGE_BUILD_DIR:-build}}"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  echo "error: '$BUILD_DIR' is not a CMake build directory (no CMakeCache.txt)." >&2
  echo "       Pass the instrumented build dir as the first argument or set COVERAGE_BUILD_DIR." >&2
  exit 1
fi

if ! grep -q "AGENT_ENABLE_COVERAGE:.*=ON" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
  echo "warning: AGENT_ENABLE_COVERAGE is not ON in '$BUILD_DIR'." >&2
  echo "         The report will be empty unless the build was instrumented." >&2
fi

# Resolve the LLVM tools (xcrun on macOS, otherwise expect them on PATH).
if command -v xcrun >/dev/null 2>&1; then
  LLVM_PROFDATA=(xcrun llvm-profdata)
  LLVM_COV=(xcrun llvm-cov)
else
  LLVM_PROFDATA=(llvm-profdata)
  LLVM_COV=(llvm-cov)
fi

BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
cd "$BUILD_DIR"

NCPU="$( (sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4) )"
CTEST_ARGS="${CTEST_ARGS:--j${NCPU}}"

# 1. Run the test suite, each process emitting its own raw profile.
if [ "${REUSE_PROFILES:-0}" != "1" ]; then
  rm -rf cov coverage.profdata coverage_html coverage_report.txt
  mkdir -p cov
  echo "=== Running ctest (instrumented) ==="
  # Don't abort the whole report if a test fails; we still want coverage.
  LLVM_PROFILE_FILE="$BUILD_DIR/cov/%p.profraw" ctest --output-on-failure $CTEST_ARGS || true
fi

shopt -s nullglob
PROFRAW=(cov/*.profraw)
if [ "${#PROFRAW[@]}" -eq 0 ]; then
  echo "error: no .profraw files were produced in $BUILD_DIR/cov." >&2
  echo "       Was the build instrumented and did any tests run?" >&2
  exit 1
fi
echo "=== Merging ${#PROFRAW[@]} raw profiles ==="
"${LLVM_PROFDATA[@]}" merge -sparse "${PROFRAW[@]}" -o coverage.profdata

# 2. Locate the instrumented object(s).
#    Shared build -> a single agent dylib/so. Static build -> all test binaries.
OBJECTS=()
while IFS= read -r lib; do OBJECTS+=("$lib"); done < <(
  find . \( -name 'libagent_lib.*' -o -name 'libmtconnect_agent*.dylib' \
            -o -name 'libagent_lib*.so' \) \
       ! -name '*.a' -type f 2>/dev/null | head -1)

if [ "${#OBJECTS[@]}" -eq 0 ]; then
  echo "=== No shared agent lib found; using test binaries (static build) ==="
  COV_ARGS=()
  first=""
  while IFS= read -r bin; do
    if [ -z "$first" ]; then first="$bin"; else COV_ARGS+=(-object "$bin"); fi
  done < <(find . -maxdepth 3 -name '*_test' -type f -perm -u+x 2>/dev/null)
  set -- "$first" "${COV_ARGS[@]}"
else
  echo "=== Coverage object: ${OBJECTS[0]} ==="
  set -- "${OBJECTS[0]}"
fi

IGNORE='(test_package|/build/|\.conan2|_deps|/usr/|gtest|googletest|/Xcode)'

# 3. Per-file summary (also saved to a file).
echo "=== Coverage summary ==="
"${LLVM_COV[@]}" report "$@" -instr-profile=coverage.profdata \
  -ignore-filename-regex="$IGNORE" | tee "$BUILD_DIR/coverage_report.txt"

# 4. Browsable HTML.
"${LLVM_COV[@]}" show "$@" -instr-profile=coverage.profdata \
  -format=html -output-dir="$BUILD_DIR/coverage_html" \
  -show-line-counts-or-regions -ignore-filename-regex="$IGNORE"

echo
echo "Text report: $BUILD_DIR/coverage_report.txt"
echo "HTML report: $BUILD_DIR/coverage_html/index.html"
