---
name: cflow
description: Find functions unreachable from main() entry point
---

# cflow

Static call graph analysis to identify dead code or test-only functions.

## Quick Check

```bash
# Generate call graph from main()
cflow --no-preprocess --main main src/client.c src/*.c 2>/dev/null > /tmp/callgraph.txt

# Get all defined functions
ctags -x --c-kinds=f src/*.c 2>/dev/null | awk '{print $1}' | sort -u > /tmp/defined.txt

# Get functions reachable from main() (defined in src/)
grep -oP '[a-zA-Z_][a-zA-Z0-9_]*\(\) <[^>]+at src/[^>]+>' /tmp/callgraph.txt | \
  grep -oP '^[a-zA-Z_][a-zA-Z0-9_]*' | sort -u > /tmp/reachable.txt

# Show unreachable
comm -23 /tmp/defined.txt /tmp/reachable.txt
```

## Codebase-Specific Patterns

cflow cannot trace function pointers. These patterns appear "unreachable" but are valid:

| Pattern | Example | How to verify |
|---------|---------|---------------|
| Command handlers | `ik_cmd_*` | Registered in `src/commands.c` dispatch table |
| Wrapper functions | `*_` (trailing underscore) | MOCKABLE wrappers for test injection |
| Array utilities | `ik_array_*`, `ik_byte_array_*` | Generic API, may be partially used |

## Finding Test-Only Functions

The key smell: functions defined in `src/*.c` but only called from `tests/`.

```bash
for fn in $(comm -23 /tmp/defined.txt /tmp/reachable.txt); do
  src_refs=$(grep -roh "\b${fn}\b" src/*.c 2>/dev/null | wc -l)
  test_refs=$(grep -rl "\b${fn}\b" tests/ 2>/dev/null | wc -l)
  if [ "$src_refs" -le 1 ] && [ "$test_refs" -gt 0 ]; then
    echo "TEST-ONLY: $fn (src=$src_refs, tests=$test_refs)"
  fi
done
```

## Interpreting Results

**TEST-ONLY functions** fall into categories:

1. **Test infrastructure** (e.g., `ik_log_init`, `ik_log_shutdown`) - Legitimate test setup/teardown
2. **Debug utilities** (e.g., `ik_pp_*`) - Inspection helpers, may be vestigial
3. **Unwired features** - Production code that tests exercise but nothing calls

Category 3 is the cleanup target: either wire them up or delete both function and tests.

## DOT Graph Output

```bash
cflow --no-preprocess --format=dot --main main src/client.c src/*.c 2>/dev/null > callgraph.dot
dot -Tpng callgraph.dot -o callgraph.png  # if graphviz installed
```
