#!/bin/bash
# Parse Check XML output and emit 🟢/🔴 lines
# Usage: parse-check-xml.sh <xml-file>

xml_file="$1"
[ ! -f "$xml_file" ] && exit 0

# Use awk to parse the XML
awk '
BEGIN { RS="<test "; FS="\n" }
NR > 1 {
    result = ""; id = ""; path = ""; fn = ""; msg = ""

    # Extract result attribute
    if (match($0, /result="([^"]+)"/, a)) result = a[1]

    # Extract child elements
    for (i=1; i<=NF; i++) {
        if (match($i, /<id>([^<]+)</, a)) id = a[1]
        if (match($i, /<path>([^<]+)</, a)) path = a[1]
        if (match($i, /<fn>([^<]+)</, a)) fn = a[1]
        if (match($i, /<message>([^<]+)</, a)) msg = a[1]
    }

    if (id != "") {
        loc = (path != "" ? path "/" : "") fn
        # Decode XML entities in message
        gsub(/&apos;/, "'\''", msg)
        gsub(/&quot;/, "\"", msg)
        gsub(/&lt;/, "<", msg)
        gsub(/&gt;/, ">", msg)
        gsub(/&amp;/, "\\&", msg)

        if (result == "success") {
            print "🟢 " id ":" loc ": " msg
        } else if (result == "failure") {
            print "🔴 " id ":" loc ": " msg
        }
    }
}' "$xml_file"
