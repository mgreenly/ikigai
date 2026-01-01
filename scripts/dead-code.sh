#!/bin/bash
# Find orphaned functions: defined in src/**/*.c but not reachable from main()
#
# Known false positives (function pointer dispatch):
# - *_destructor  : talloc destructors (passed to talloc_set_destructor)
# - *_            : wrapper/mock functions (called via macros)
#
# Verified false positives are stored in .claude/data/dead-code-false-positives.txt
# and excluded from output. These are functions that failed compile after deletion.
#
# This script traces into command handlers by parsing src/commands.c

set -euo pipefail

# Verified false positives file (functions that failed compile after deletion)
FALSE_POSITIVES_FILE=".claude/data/dead-code-false-positives.txt"

# Collect all source files (excluding vendor)
mapfile -t SRC_FILES < <(find src -name '*.c' -not -path 'src/vendor/*' | sort)

# Generate call graph from main()
cflow --no-preprocess --main main "${SRC_FILES[@]}" 2>/dev/null > /tmp/cflow_callgraph.txt

# Extract reachable functions from main
grep -oP '[a-zA-Z_][a-zA-Z0-9_]*\(\) <[^>]+at src/[^>]+>' /tmp/cflow_callgraph.txt | \
  grep -oP '^[a-zA-Z_][a-zA-Z0-9_]*' | LANG=C sort -u > /tmp/cflow_reachable.txt

# Extract command handlers from src/commands.c and trace into each
handlers=$(grep -oP 'ik_cmd_[a-z_]+' src/commands.c | sort -u)
for handler in $handlers; do
  cflow --no-preprocess --main "$handler" "${SRC_FILES[@]}" 2>/dev/null | \
    grep -oP '[a-zA-Z_][a-zA-Z0-9_]*\(\) <[^>]+at src/[^>]+>' | \
    grep -oP '^[a-zA-Z_][a-zA-Z0-9_]*' >> /tmp/cflow_reachable.txt
done

# Deduplicate reachable functions
LANG=C sort -u /tmp/cflow_reachable.txt -o /tmp/cflow_reachable.txt

# Get all functions defined in src/**/*.c (excluding vendor)
ctags -x --c-kinds=f "${SRC_FILES[@]}" 2>/dev/null | awk '{print $1}' | LANG=C sort -u > /tmp/cflow_defined.txt

# Find unreachable functions
LANG=C comm -23 /tmp/cflow_defined.txt /tmp/cflow_reachable.txt > /tmp/cflow_unreachable.txt

# Filter out known false positives:
# - *_destructor    : talloc destructors (passed to talloc_set_destructor)
# - *_              : wrapper/mock functions (called via macros)
# - talloc_zero_for_error : called via ERR() macro in error.h
grep -vE '_destructor$|_$|^talloc_zero_for_error$' /tmp/cflow_unreachable.txt > /tmp/cflow_orphans.txt || true

# Filter out verified false positives (functions that failed compile after deletion)
if [ -f "$FALSE_POSITIVES_FILE" ]; then
  # Create temp file with functions to exclude
  grep -vxFf "$FALSE_POSITIVES_FILE" /tmp/cflow_orphans.txt > /tmp/cflow_orphans_filtered.txt || true
  mv /tmp/cflow_orphans_filtered.txt /tmp/cflow_orphans.txt
fi

# Filter out functions that are used anywhere in src/ (beyond definition/declaration)
# A function is "used" if it appears in any context other than:
#   - Its definition line (return_type func_name(...))
#   - Its declaration in a header file
> /tmp/cflow_orphans_filtered.txt
while read -r fn; do
  # Get all occurrences in src/, excluding:
  #   - Header files (.h) - declarations
  #   - Definition lines (start with return type, contain function name with open paren)
  # If anything remains, the function is used somewhere
  if grep -rn "\b${fn}\b" "${SRC_FILES[@]}" 2>/dev/null | \
     grep -v "\.h:" | \
     grep -vE "^[^:]+:[0-9]+:\s*(static\s+)?(const\s+)?(struct\s+\w+\s*\*?|enum\s+\w+|void|int|bool|char|res_t|size_t|ssize_t|int32_t|uint32_t|int64_t|uint64_t|ik_\w+_t)\s*\*?\s*${fn}\s*\(" | \
     grep -q .; then
    : # Skip - function is used somewhere
  else
    echo "$fn" >> /tmp/cflow_orphans_filtered.txt
  fi
done < /tmp/cflow_orphans.txt
mv /tmp/cflow_orphans_filtered.txt /tmp/cflow_orphans.txt

# Output results (format: function:file:line)
count=$(wc -l < /tmp/cflow_orphans.txt)
if [ "$count" -eq 0 ]; then
  echo "No orphaned functions found."
else
  echo "# Orphaned functions: $count"
  echo "# format: function:file:line"
  while read -r fn; do
    loc=$(ctags -x --c-kinds=f "${SRC_FILES[@]}" 2>/dev/null | grep "^${fn}\s" | awk '{print $4 ":" $3}')
    echo "${fn}:${loc}"
  done < /tmp/cflow_orphans.txt
fi

# Cleanup
rm -f /tmp/cflow_callgraph.txt /tmp/cflow_reachable.txt /tmp/cflow_defined.txt /tmp/cflow_unreachable.txt /tmp/cflow_orphans.txt /tmp/cflow_orphans_filtered.txt
