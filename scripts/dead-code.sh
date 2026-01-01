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

# Filter out functions that are referenced elsewhere (grep-based sanity check)
# If a function name appears in src/ files beyond its definition, it's probably used
> /tmp/cflow_truly_dead.txt
while read -r fn; do
  # Count occurrences of the function name (as a word) in all source files
  # Exclude the definition line itself by looking for references
  count=$(grep -rw "$fn" src/ --include='*.c' --include='*.h' 2>/dev/null | wc -l)
  # If only 1-2 occurrences (definition + maybe declaration), it's likely dead
  # More than 2 suggests it's actually used somewhere
  if [ "$count" -le 2 ]; then
    echo "$fn" >> /tmp/cflow_truly_dead.txt
  fi
done < /tmp/cflow_orphans.txt
mv /tmp/cflow_truly_dead.txt /tmp/cflow_orphans.txt

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
