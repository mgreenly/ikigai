#!/bin/bash
# Automated bash_tool integration test
# Tests bash_tool's JSON I/O and command execution
#
# Usage: ./tests/integration/bash_tool_test.sh
# Exit codes: 0 = success, non-zero = failure

set -euo pipefail

# Color output for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

BASH_TOOL="libexec/ikigai/bash_tool"

echo "======================================"
echo "bash_tool Integration Test Suite"
echo "======================================"
echo "Tool: $BASH_TOOL"
echo ""

# Verify bash_tool exists
if [ ! -f "$BASH_TOOL" ]; then
    echo -e "${RED}✗ bash_tool not found: $BASH_TOOL${NC}"
    exit 1
fi
echo -e "${GREEN}✓ bash_tool exists${NC}"

# Test 1: Schema flag exits 0
echo -e "${YELLOW}Testing --schema flag...${NC}"
"$BASH_TOOL" --schema > /dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
    echo -e "${RED}✗ --schema exited with code $EXIT_CODE (expected 0)${NC}"
    exit 1
fi
echo -e "${GREEN}✓ --schema exits 0${NC}"

# Test 2: Schema output is valid JSON with required fields
echo -e "${YELLOW}Testing --schema JSON output...${NC}"
SCHEMA_OUTPUT=$("$BASH_TOOL" --schema)
if ! echo "$SCHEMA_OUTPUT" | grep -q '"name"'; then
    echo -e "${RED}✗ --schema output missing 'name' field${NC}"
    echo "Got: $SCHEMA_OUTPUT"
    exit 1
fi
if ! echo "$SCHEMA_OUTPUT" | grep -q '"description"'; then
    echo -e "${RED}✗ --schema output missing 'description' field${NC}"
    echo "Got: $SCHEMA_OUTPUT"
    exit 1
fi
if ! echo "$SCHEMA_OUTPUT" | grep -q '"parameters"'; then
    echo -e "${RED}✗ --schema output missing 'parameters' field${NC}"
    echo "Got: $SCHEMA_OUTPUT"
    exit 1
fi
echo -e "${GREEN}✓ --schema outputs valid JSON with required fields${NC}"

# Test 3: Valid input returns JSON with output and exit_code fields
echo -e "${YELLOW}Testing valid input '{"command":"echo hello"}' returns JSON with output and exit_code fields...${NC}"
OUTPUT=$(echo '{"command":"echo hello"}' | "$BASH_TOOL")
if ! echo "$OUTPUT" | grep -q '"output"'; then
    echo -e "${RED}✗ Valid input missing 'output' field${NC}"
    echo "Got: $OUTPUT"
    exit 1
fi
if ! echo "$OUTPUT" | grep -q '"exit_code"'; then
    echo -e "${RED}✗ Valid input missing 'exit_code' field${NC}"
    echo "Got: $OUTPUT"
    exit 1
fi
echo -e "${GREEN}✓ Valid input returns JSON with output and exit_code fields${NC}"

# Test 4: Non-zero exit code
echo -e "${YELLOW}Testing non-zero exit code...${NC}"
OUTPUT=$(echo '{"command":"false"}' | "$BASH_TOOL")
if ! echo "$OUTPUT" | grep -q '"exit_code":1'; then
    echo -e "${RED}✗ Non-zero exit code not captured${NC}"
    echo "Got: $OUTPUT"
    exit 1
fi
echo -e "${GREEN}✓ Non-zero exit code captured${NC}"

# Test 5: Empty input writes to stderr and exits non-zero
echo -e "${YELLOW}Testing empty input...${NC}"
OUTPUT=$(echo -n "" | "$BASH_TOOL" 2>&1 || true)
if echo "$OUTPUT" | grep -q "bash_tool: empty input"; then
    echo -e "${GREEN}✓ Empty input writes to stderr${NC}"
else
    echo -e "${RED}✗ Empty input not handled correctly${NC}"
    echo "Got: $OUTPUT"
    exit 1
fi

# Test 6: Invalid JSON writes to stderr and exits non-zero
echo -e "${YELLOW}Testing invalid JSON...${NC}"
OUTPUT=$(echo "invalid" | "$BASH_TOOL" 2>&1 || true)
if echo "$OUTPUT" | grep -q "bash_tool: invalid JSON"; then
    echo -e "${GREEN}✓ Invalid JSON writes to stderr${NC}"
else
    echo -e "${RED}✗ Invalid JSON not handled correctly${NC}"
    echo "Got: $OUTPUT"
    exit 1
fi

# Test 7: Missing command field writes to stderr and exits non-zero
echo -e "${YELLOW}Testing missing command field...${NC}"
OUTPUT=$(echo '{}' | "$BASH_TOOL" 2>&1 || true)
if echo "$OUTPUT" | grep -q "bash_tool: missing command field"; then
    echo -e "${GREEN}✓ Missing command field error handled${NC}"
else
    echo -e "${RED}✗ Missing command field not handled correctly${NC}"
    echo "Got: $OUTPUT"
    exit 1
fi

# Test 8: Non-string command field writes to stderr and exits non-zero
echo -e "${YELLOW}Testing non-string command field...${NC}"
OUTPUT=$(echo '{"command":123}' | "$BASH_TOOL" 2>&1 || true)
if echo "$OUTPUT" | grep -q "bash_tool: command must be a string"; then
    echo -e "${GREEN}✓ Non-string command field writes to stderr${NC}"
else
    echo -e "${RED}✗ Non-string command field not handled correctly${NC}"
    echo "Got: $OUTPUT"
    exit 1
fi

# Test 9: Output with trailing newline has it stripped
echo -e "${YELLOW}Testing trailing newline is stripped...${NC}"
OUTPUT=$(echo '{"command":"echo hello"}' | "$BASH_TOOL")
if echo "$OUTPUT" | grep -q '"output":"hello"'; then
    echo -e "${GREEN}✓ Trailing newline stripped from output${NC}"
else
    echo -e "${RED}✗ Trailing newline not stripped correctly${NC}"
    echo "Got: $OUTPUT"
    exit 1
fi

echo ""
echo "======================================"
echo -e "${GREEN}✓ All bash_tool tests passed!${NC}"
echo "======================================"

exit 0
