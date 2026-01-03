# Task: Fix /agents tree rendering with proper indentation

## Target
Bug fix: `/agents` command output shows UUIDs aligned in same column regardless of hierarchy depth, making parent-child relationships unclear.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

## Pre-read Source (patterns)
- READ: src/commands_agent_list.c (entire file - cmd_agents implementation)
- READ: src/scrollback.h (ik_scrollback_append_line)

## Pre-read Tests (patterns)
- READ: tests/unit/commands/cmd_agents_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes

## Task
Fix the `/agents` tree rendering to use tree-drawing characters and proper indentation so hierarchy is visually clear.

**Current (buggy) output:**
```
Agent Hierarchy:                          <- scrollback line 0
                                          <- scrollback line 1 (blank)
* wlT4G-jnTXaaLmMzn8maZg (running) - root <- scrollback line 2
  Mg23MVijSiWdBRdY6V7Uqw (running)        <- scrollback line 3
                                          <- scrollback line 4 (blank)
2 running, 0 dead                         <- scrollback line 5
```

Both UUIDs start at column 2 because:
- Root: `* ` (2 chars) then UUID
- Child: `  ` (2 chars indent) then UUID

The `*` marker "eats" the visual indentation.

**Expected output:**
```
Agent Hierarchy:

* wlT4G-jnTXaaLmMzn8maZg (running) - root
  +-- Mg23MVijSiWdBRdY6V7Uqw (running)
      +-- grandchild... (running)

2 running, 0 dead
```

**Key formatting rules:**
- depth 0: `[marker]UUID` where marker is `* ` (current) or `  ` (not current)
- depth 1: `  +-- UUID` (2 spaces + `+-- ` tree prefix)
- depth 2: `      +-- UUID` (6 spaces + `+-- ` tree prefix)
- depth N: `(2 + 4*(N-1)) spaces + "+-- " + UUID`
- The `*` current marker only appears for depth-0 agents (root agents)

**ASCII-safe tree characters:**
- Use `+-- ` for child connection (4 chars including trailing space)
- Do NOT use Unicode - stick to pure ASCII for terminal compatibility

## Bug Analysis

Looking at `src/commands_agent_list.c` lines 96-111:

```c
// Add indentation (2 spaces per level)
for (size_t d = 0; d < depth; d++) {
    line[offset++] = ' ';
    line[offset++] = ' ';
}

// Add current marker
bool is_current = strcmp(agent->uuid, repl->current->uuid) == 0;
if (is_current) {
    line[offset++] = '*';
    line[offset++] = ' ';
}
```

Problems:
1. Uses only 2 spaces per depth level - too small to distinguish hierarchy
2. No tree characters (`+--`) to visually show parent-child relationship
3. Current marker logic incomplete - non-current depth-0 agents get no prefix, breaking alignment

## TDD Cycle

### Red
1. Modify tests in `tests/unit/commands/cmd_agents_test.c`:

   **Update `test_agents_indentation_depth` to verify tree characters:**
   ```c
   // After running cmd_agents, verify scrollback content
   // Line 2 should be the root agent (lines 0,1 are header/blank)
   const char *text;
   size_t length;
   res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &text, &length);
   ck_assert(is_ok(&line_res));

   // Root should start with "* " for current marker
   ck_assert_msg(text[0] == '*', "Root should have * marker");

   // Line 3 should be the child agent with tree prefix
   line_res = ik_scrollback_get_line_text(repl->current->scrollback, 3, &text, &length);
   ck_assert(is_ok(&line_res));

   // Child should have tree prefix "+-- "
   ck_assert_msg(strstr(text, "+--") != NULL, "Child should have +-- tree prefix");

   // Child UUID should start after column 6 (2 spaces + 4 chars for "+-- ")
   // Verify indentation: child text should start with "  +-- "
   ck_assert_msg(strncmp(text, "  +-- ", 6) == 0, "Child should have '  +-- ' prefix");
   ```

   **Add new test `test_agents_root_alignment` to verify non-current root alignment:**
   ```c
   START_TEST(test_agents_root_alignment)
   {
       // Setup: current agent is NOT the root
       // Create a child, make it current, then run /agents
       // Verify root line starts with "  " (2 spaces, not "*")
       // This ensures all depth-0 agents align
   }
   END_TEST
   ```

2. Run `make check` - expect test failures

### Green
1. Modify `src/commands_agent_list.c` - replace indentation logic (around lines 96-111):

   ```c
   // Build line with indentation
   char line[256];
   size_t offset = 0;

   if (depth == 0) {
       // Root agent: use marker column
       bool is_current = strcmp(agent->uuid, repl->current->uuid) == 0;
       if (is_current) {
           line[offset++] = '*';
           line[offset++] = ' ';
       } else {
           line[offset++] = ' ';
           line[offset++] = ' ';
       }
   } else {
       // Child agent: add spaces for hierarchy + tree prefix
       // 2 spaces base indent
       line[offset++] = ' ';
       line[offset++] = ' ';

       // 4 spaces for each additional depth level above 1
       for (size_t d = 1; d < depth; d++) {
           memcpy(&line[offset], "    ", 4);
           offset += 4;
       }

       // Tree character
       memcpy(&line[offset], "+-- ", 4);
       offset += 4;
   }

   // Add full UUID (move is_current check above)
   size_t uuid_len = strlen(agent->uuid);
   memcpy(&line[offset], agent->uuid, uuid_len);
   offset += uuid_len;
   ```

2. Run `make check` - expect pass

### Refactor
1. Consider extracting constants:
   ```c
   #define MARKER_WIDTH 2      // "* " or "  "
   #define TREE_PREFIX "    "  // 4 spaces per depth
   #define TREE_CONNECTOR "+-- "
   ```
2. Run `make check` - verify still passing
3. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `make lint` passes
- Working tree is clean
- `/agents` output clearly shows hierarchy with tree characters
- Child UUIDs are visually indented relative to parent UUIDs
- Depth-0 agents all align (whether current or not)
- The `*` current-agent marker only appears for root agents
