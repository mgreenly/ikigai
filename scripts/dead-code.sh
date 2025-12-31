#!/bin/bash
# Find orphaned functions: defined in src/*.c but not reachable from main()
#
# Known false positives (function pointer dispatch):
# - *_destructor  : talloc destructors (passed to talloc_set_destructor)
# - *_            : wrapper/mock functions (called via macros)
#
# This script traces into command handlers by parsing src/commands.c

set -euo pipefail

# Generate call graph from main()
cflow --no-preprocess --main main src/client.c src/*.c 2>/dev/null > /tmp/cflow_callgraph.txt

# Extract reachable functions from main
grep -oP '[a-zA-Z_][a-zA-Z0-9_]*\(\) <[^>]+at src/[^>]+>' /tmp/cflow_callgraph.txt | \
  grep -oP '^[a-zA-Z_][a-zA-Z0-9_]*' | LANG=C sort -u > /tmp/cflow_reachable.txt

# Extract command handlers from src/commands.c and trace into each
handlers=$(grep -oP 'ik_cmd_[a-z_]+' src/commands.c | sort -u)
for handler in $handlers; do
  cflow --no-preprocess --main "$handler" src/*.c 2>/dev/null | \
    grep -oP '[a-zA-Z_][a-zA-Z0-9_]*\(\) <[^>]+at src/[^>]+>' | \
    grep -oP '^[a-zA-Z_][a-zA-Z0-9_]*' >> /tmp/cflow_reachable.txt
done

# Deduplicate reachable functions
LANG=C sort -u /tmp/cflow_reachable.txt -o /tmp/cflow_reachable.txt

# Get all functions defined in src/*.c
ctags -x --c-kinds=f src/*.c 2>/dev/null | awk '{print $1}' | LANG=C sort -u > /tmp/cflow_defined.txt

# Find unreachable functions
LANG=C comm -23 /tmp/cflow_defined.txt /tmp/cflow_reachable.txt > /tmp/cflow_unreachable.txt

# Filter out known false positives:
# - *_destructor    : talloc destructors (passed to talloc_set_destructor)
# - *_              : wrapper/mock functions (called via macros)
# - talloc_zero_for_error : called via ERR() macro in error.h
grep -vE '_destructor$|_$|^talloc_zero_for_error$' /tmp/cflow_unreachable.txt > /tmp/cflow_orphans.txt || true

# Output results (format: function:file:line)
count=$(wc -l < /tmp/cflow_orphans.txt)
if [ "$count" -eq 0 ]; then
  echo "No orphaned functions found."
else
  echo "# Orphaned functions: $count"
  echo "# format: function:file:line"
  while read -r fn; do
    loc=$(ctags -x --c-kinds=f src/*.c 2>/dev/null | grep "^${fn}\s" | awk '{print $4 ":" $3}')
    echo "${fn}:${loc}"
  done < /tmp/cflow_orphans.txt
fi

# Cleanup
rm -f /tmp/cflow_callgraph.txt /tmp/cflow_reachable.txt /tmp/cflow_defined.txt /tmp/cflow_unreachable.txt /tmp/cflow_orphans.txt
