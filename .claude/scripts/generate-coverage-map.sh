#!/bin/bash
# Generate source file → test mapping from coverage data
# Output: .claude/data/source_tests.json
#
# Uses lcov to check which source files have actual executed lines.

set -e

BUILDDIR="${BUILDDIR:-build-coverage}"
OUTPUT_DIR=".claude/data"
OUTPUT_FILE="$OUTPUT_DIR/source_tests.json"
TMP_DIR=$(mktemp -d)

trap "rm -rf $TMP_DIR" EXIT

mkdir -p "$OUTPUT_DIR"

# Find all test binaries
TEST_BINARIES=$(find "$BUILDDIR/tests" -type f -executable -name "*_test" 2>/dev/null)
TOTAL=$(echo "$TEST_BINARIES" | wc -l)
COUNT=0

echo "Mapping $TOTAL test binaries to source files..."

# For each test, record which source files it actually executes
for test_bin in $TEST_BINARIES; do
    COUNT=$((COUNT + 1))

    # Extract test name (e.g., build-coverage/tests/unit/config/config_test -> config/config_test)
    test_name=$(echo "$test_bin" | sed "s|$BUILDDIR/tests/unit/||; s|$BUILDDIR/tests/integration/||; s|$BUILDDIR/tests/||")

    # Clear coverage data
    find "$BUILDDIR" -name "*.gcda" -delete 2>/dev/null || true

    # Run test (suppress output)
    if $test_bin >/dev/null 2>&1; then
        # Capture coverage with lcov (fast, single pass)
        info_file="$TMP_DIR/$COUNT.info"
        if lcov --capture --directory "$BUILDDIR" --output-file "$info_file" \
                --rc branch_coverage=0 --quiet --ignore-errors inconsistent,deprecated,negative \
                >/dev/null 2>&1; then

            # Extract source files with >0% line coverage from the .info file
            # SF: lines in .info file contain the source file paths
            # LF: total lines, LH: lines hit - we want LH > 0
            touched=$(awk '
                /^SF:/ { file = $0; sub(/^SF:/, "", file) }
                /^LH:/ {
                    lh = $0; sub(/^LH:/, "", lh)
                    if (lh > 0 && file ~ /\/src\/.*\.c$/ && file !~ /\/vendor\//) {
                        # Extract just src/... path (exclude vendor, headers)
                        match(file, /src\/.*/)
                        print substr(file, RSTART)
                    }
                }
            ' "$info_file" | sort -u)

            if [ -n "$touched" ]; then
                echo "$test_name" > "$TMP_DIR/$COUNT.test"
                echo "$touched" > "$TMP_DIR/$COUNT.sources"
            fi
            rm -f "$info_file"
        fi
    fi

    # Progress indicator
    printf "\r  [%d/%d] %s" "$COUNT" "$TOTAL" "$test_name"
done

echo ""
echo "Inverting mapping (source -> tests)..."

# Invert the mapping: source file -> tests that touch it
declare -A source_to_tests

for test_file in "$TMP_DIR"/*.test; do
    [ -f "$test_file" ] || continue
    base=$(basename "$test_file" .test)
    test_name=$(cat "$test_file")
    sources_file="$TMP_DIR/$base.sources"

    while IFS= read -r source; do
        [ -n "$source" ] || continue
        if [ -z "${source_to_tests[$source]}" ]; then
            source_to_tests[$source]="$test_name"
        else
            source_to_tests[$source]="${source_to_tests[$source]} $test_name"
        fi
    done < "$sources_file"
done

# Write JSON output
echo "{" > "$OUTPUT_FILE"
first=true
for source in $(echo "${!source_to_tests[@]}" | tr ' ' '\n' | sort); do
    tests="${source_to_tests[$source]}"
    # Convert space-separated tests to JSON array
    json_tests=$(echo "$tests" | tr ' ' '\n' | sort -u | sed 's/.*/"&"/' | paste -sd, -)

    if [ "$first" = true ]; then
        first=false
    else
        echo "," >> "$OUTPUT_FILE"
    fi
    printf '  "%s": [%s]' "$source" "$json_tests" >> "$OUTPUT_FILE"
done
echo "" >> "$OUTPUT_FILE"
echo "}" >> "$OUTPUT_FILE"

# Summary
source_count=$(grep -c '"src/' "$OUTPUT_FILE" || echo 0)
echo "Generated $OUTPUT_FILE"
echo "  Sources with tests: $source_count"
