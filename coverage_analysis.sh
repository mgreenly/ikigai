#!/bin/bash

echo "=== Coverage Gap Analysis ==="
echo ""

# Extract uncovered lines for each file
awk '
/^SF:/ {
    if (file != "") {
        if (count > 0) {
            print file " (" count " uncovered lines):"
            print lines
            print ""
        }
    }
    file = $0
    sub(/^SF:/, "", file)
    count = 0
    lines = ""
}
/^DA:.*,0$/ {
    line_num = $0
    sub(/^DA:/, "", line_num)
    sub(/,0$/, "", line_num)
    if (lines != "") lines = lines ", "
    lines = lines line_num
    count++
}
END {
    if (file != "" && count > 0) {
        print file " (" count " uncovered lines):"
        print lines
    }
}
' coverage/coverage.info | grep -A1 "uncovered"
