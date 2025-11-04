#!/bin/bash
# Parse RLTest output to extract test counts
# RLTest is based on Python unittest, so it outputs similar format
# Usage: parse-test-results.sh <test_output_file> <output_json_file>

set -e

INPUT_FILE="${1}"
OUTPUT_FILE="${2:-test-results.json}"

# Initialize counters
PASSED=0
FAILED=0
SKIPPED=0
TOTAL=0

# Read input (either file or stdin)
if [ -n "$INPUT_FILE" ] && [ -f "$INPUT_FILE" ]; then
    TEST_OUTPUT=$(cat "$INPUT_FILE")
else
    TEST_OUTPUT=$(cat)
fi

# RLTest/unittest typically outputs:
# "Ran X tests in Y seconds"
# Then either:
# "OK" or "OK (skipped=Z)" or "FAILED (failures=W, errors=V)"

# Extract total number of tests
TOTAL=$(echo "$TEST_OUTPUT" | grep -iE "ran [0-9]+ test" | grep -oE "[0-9]+" | head -1 || echo "0")

# Check for OK status (all passed)
if echo "$TEST_OUTPUT" | grep -qiE "^\s*ok\s*$|^\s*ok\s*\(|^ok"; then
    # All tests passed
    # Extract skipped count if present
    SKIPPED=$(echo "$TEST_OUTPUT" | grep -iE "ok.*skipped" | grep -oE "skipped[=:][ ]*[0-9]+" | grep -oE "[0-9]+" | head -1 || echo "0")
    if [ "$TOTAL" != "0" ]; then
        PASSED=$((TOTAL - SKIPPED))
    fi
# Check for FAILED status
elif echo "$TEST_OUTPUT" | grep -qiE "^\s*failed\s*$|^\s*failed\s*\(|^failed"; then
    # Extract failure and error counts
    FAILED=$(echo "$TEST_OUTPUT" | grep -iE "failed.*failures" | grep -oE "failures?[=:][ ]*[0-9]+" | grep -oE "[0-9]+" | head -1 || echo "0")
    ERRORS=$(echo "$TEST_OUTPUT" | grep -iE "failed.*errors" | grep -oE "errors?[=:][ ]*[0-9]+" | grep -oE "[0-9]+" | head -1 || echo "0")
    FAILED=$((FAILED + ERRORS))
    
    # Extract skipped count if present
    SKIPPED=$(echo "$TEST_OUTPUT" | grep -iE "skipped" | grep -oE "skipped[=:][ ]*[0-9]+" | grep -oE "[0-9]+" | head -1 || echo "0")
    
    if [ "$TOTAL" != "0" ]; then
        PASSED=$((TOTAL - FAILED - SKIPPED))
    fi
fi

# If we still don't have counts, try counting test execution lines
if [ "$TOTAL" = "0" ] || ([ "$PASSED" = "0" ] && [ "$FAILED" = "0" ]); then
    # Try to count test method calls or results
    # Look for patterns like "test_xxx" or ".test_xxx" or "test_xxx ... ok" or "test_xxx ... FAIL"
    
    # Count passed tests (look for "ok" or "PASS" after test name)
    PASSED=$(echo "$TEST_OUTPUT" | grep -cE "(test_\w+.*\bok\b|test_\w+.*\bPASS\b|\[PASS\]|\[OK\])" || echo "0")
    
    # Count failed tests (look for "FAIL" or "ERROR" after test name)
    FAILED=$(echo "$TEST_OUTPUT" | grep -cE "(test_\w+.*\bFAIL\b|test_\w+.*\bERROR\b|\[FAIL\]|\[ERROR\])" || echo "0")
    
    # Count skipped tests
    SKIPPED=$(echo "$TEST_OUTPUT" | grep -cE "(test_\w+.*\bSKIP\b|\[SKIP\])" || echo "0")
    
    TOTAL=$((PASSED + FAILED + SKIPPED))
fi

# Ensure we have valid numbers
PASSED=${PASSED:-0}
FAILED=${FAILED:-0}
SKIPPED=${SKIPPED:-0}
TOTAL=${TOTAL:-$((PASSED + FAILED + SKIPPED))}

# Output JSON
cat > "$OUTPUT_FILE" <<EOF
{
  "passed": $PASSED,
  "failed": $FAILED,
  "skipped": $SKIPPED,
  "total": $TOTAL
}
EOF

echo "Parsed test results: Passed=$PASSED, Failed=$FAILED, Skipped=$SKIPPED, Total=$TOTAL"
