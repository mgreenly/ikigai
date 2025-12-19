Run coverage tests and fix gaps using sub-agents.

**Usage:**
- `/coverage` - Run make coverage and dispatch sub-agents to fix coverage gaps

**Action:** Uses a discovery sub-agent to find coverage gaps, then sequential fix sub-agents (one per file) to resolve them. Each fix sub-agent starts with fresh context and loads skills explicitly.

---

## Execution

### 1. Discover Coverage Gaps

Spawn a discovery sub-agent to identify files with coverage issues:

**Tool Configuration:**
- Tool: `Task`
- `subagent_type`: `general-purpose`
- `model`: `haiku`

**Prompt:**
```
Run `make coverage` and parse the output to find coverage gaps.

Return ONLY a JSON object in this exact format:
{
  "success": true or false (did make coverage pass?),
  "files": [
    {
      "file": "src/example.c",
      "gaps": ["line 42: branch never taken false", "line 87: function never called"]
    }
  ]
}

If coverage passes with no gaps, return: {"success": true, "files": []}
```

If the discovery returns `success: true` with empty files, report success and stop.

### 2. Fix Issues Sequentially

For each file in the discovery results, spawn a fix sub-agent:

**Tool Configuration:**
- Tool: `Task`
- `subagent_type`: `general-purpose`
- `model`: `sonnet`

**CRITICAL:** Run sub-agents ONE AT A TIME. Wait for each to complete before spawning the next.

**Prompt template:**
```
## Context

Use `/persona coverage` to load the coverage skill set.

## Task

Fix ALL coverage gaps in: {FILE_PATH}

Coverage gaps to fix:
{LIST_OF_GAPS}

## Validation

Before reporting done, run these commands and fix any failures:
1. `make lint`
2. `make check`

Report what you fixed and the final validation results.
```

### 3. Verify Results

After ALL fix sub-agents complete, run `make coverage` directly.
- If passes: report success
- If issues remain: compare to initial gap count

### 4. Progress Loop

If issues remain after a full pass:
- **Forward progress** = fewer total coverage gaps than before this pass
- If progress: return to Step 1 (discovery) with fresh sub-agent
- If no progress: stop and report remaining issues to user

Then wait for instructions.
