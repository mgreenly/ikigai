#!/bin/bash
# Parse Check XML output and emit 🟢/🔴 lines
# Usage: parse-check-xml.sh <xml-file>
# Output format: 🟢/🔴 test_name:line: message

xml_file="$1"
[ ! -f "$xml_file" ] && exit 0

# Use awk to parse the XML
awk '
BEGIN { RS="<test "; FS="\n" }
NR > 1 {
    result = ""; id = ""; line = ""; msg = ""

    # Extract result attribute
    if (match($0, /result="([^"]+)"/, a)) result = a[1]

    # Extract child elements
    for (i=1; i<=NF; i++) {
        if (match($i, /<id>([^<]+)</, a)) id = a[1]
        if (match($i, /<line>([^<]+)</, a)) line = a[1]
        if (match($i, /<message>([^<]+)</, a)) msg = a[1]
    }

    if (id != "") {
        # Decode XML entities in message
        gsub(/&apos;/, "'\''", msg)
        gsub(/&quot;/, "\"", msg)
        gsub(/&lt;/, "<", msg)
        gsub(/&gt;/, ">", msg)
        gsub(/&amp;/, "\\&", msg)

        if (result == "success") {
            print "🟢 " id ":" line ": " msg
        } else if (result == "failure") {
            print "🔴 " id ":" line ": " msg
        }
    }
}' "$xml_file"
