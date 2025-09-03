#!/bin/bash

# IQ Lab Test Runner
# Runs unit and integration tests for image-related components

set -e  # Exit on any error

echo "🧪 IQ Lab Test Suite"
echo "==================="
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to run a test and report result
run_test() {
    local test_name=$1
    local test_exe=$2

    echo -n "Running $test_name... "

    if [ -f "$test_exe" ]; then
        if ./$test_exe > /dev/null 2>&1; then
            echo -e "${GREEN}✅ PASSED${NC}"
            return 0
        else
            echo -e "${RED}❌ FAILED${NC}"
            return 1
        fi
    else
        echo -e "${YELLOW}⚠️  NOT FOUND${NC}"
        return 2
    fi
}

echo "📦 Compiling tests..."
echo

# Compile unit tests
echo "Compiling unit tests..."

# Compile PNG unit test
gcc -std=c11 -Wall -Wextra -Werror -O2 -g \
    -DSTB_IMAGE_WRITE_IMPLEMENTATION \
    -I../src \
    tests/unit/test_img_png.c \
    src/viz/img_png.c \
    -o tests/unit/test_img_png.exe

# Compile PPM unit test
gcc -std=c11 -Wall -Wextra -Werror -O2 -g \
    -I../src \
    tests/unit/test_img_ppm.c \
    src/viz/img_ppm.c \
    -o tests/unit/test_img_ppm.exe

# Compile draw_axes unit test
gcc -std=c11 -Wall -Wextra -Werror -O2 -g \
    -DSTB_IMAGE_WRITE_IMPLEMENTATION \
    -I../src \
    tests/unit/test_draw_axes.c \
    src/viz/draw_axes.c \
    src/viz/img_png.c \
    -o tests/unit/test_draw_axes.exe

echo "Compiling integration tests..."

# Compile integration test
gcc -std=c11 -Wall -Wextra -Werror -O2 -g \
    -DSTB_IMAGE_WRITE_IMPLEMENTATION \
    -I../src \
    tests/integration/test_iqls_pipeline.c \
    src/iq_core/io_iq.c \
    src/iq_core/io_sigmf.c \
    src/iq_core/fft.c \
    src/iq_core/window.c \
    src/viz/img_png.c \
    src/viz/img_ppm.c \
    src/viz/draw_axes.c \
    -lm \
    -o tests/integration/test_iqls_pipeline.exe

echo
echo "🧪 Running Tests..."
echo

# Track test results
passed=0
failed=0
skipped=0

echo "Unit Tests:"
echo "-----------"

run_test "PNG Unit Test" "tests/unit/test_img_png.exe"
case $? in
    0) ((passed++)) ;;
    1) ((failed++)) ;;
    2) ((skipped++)) ;;
esac

run_test "PPM Unit Test" "tests/unit/test_img_ppm.exe"
case $? in
    0) ((passed++)) ;;
    1) ((failed++)) ;;
    2) ((skipped++)) ;;
esac

run_test "Draw Axes Unit Test" "tests/unit/test_draw_axes.exe"
case $? in
    0) ((passed++)) ;;
    1) ((failed++)) ;;
    2) ((skipped++)) ;;
esac

echo
echo "Integration Tests:"
echo "------------------"

run_test "Pipeline Integration Test" "tests/integration/test_iqls_pipeline.exe"
case $? in
    0) ((passed++)) ;;
    1) ((failed++)) ;;
    2) ((skipped++)) ;;
esac

echo
echo "📊 Test Summary:"
echo "================"
echo "Passed:  $passed"
echo "Failed:  $failed"
echo "Skipped: $skipped"
echo

if [ $failed -eq 0 ]; then
    echo -e "${GREEN}🎉 All tests completed successfully!${NC}"
    exit 0
else
    echo -e "${RED}💥 Some tests failed. Please check the output above.${NC}"
    exit 1
fi
