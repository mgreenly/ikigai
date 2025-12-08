Run ThreadSanitizer tests and fix failures using sub-agents.

**Usage:**
- `/tsan` - Run check-tsan and dispatch sub-agents to fix failures

**Action:** Executes `make check-tsan` and uses sub-agents with the coverage persona to fix any ThreadSanitizer errors (data races, thread leaks, lock order violations) found. Sub-agents run sequentially (one per file) and continue until all issues are resolved.

---

## Execution

### 1. Initial Check
Run `make check-tsan`. If it passes, report success and stop.
If it fails, identify all files with ThreadSanitizer errors (data races, deadlocks, etc.).

### 2. Fix Issues With Sub-Agents

**Scope:** One sub-agent per file. Each sub-agent fixes ALL ThreadSanitizer errors in its assigned file.

**Sequence:** Run sub-agents ONE AT A TIME. Do not spawn the next until the current one completes.

**CRITICAL:** While a sub-agent is running, do NOTHING. Do not read files, edit files, run commands, or perform any other actions. Changes made by the parent agent while a sub-agent is running may conflict with the sub-agent's work and cause failures.

For each file with issues:
1. Spawn a sub-agent for that file
2. Wait for completion — perform no other actions
3. Review the sub-agent's report
4. Move to the next file

**Tool Configuration:**
- Tool: `Task`
- `subagent_type`: `general-purpose`
- `model`: `sonnet`

**Prompt must include:**
- The persona to load: `coverage`
- The specific file to fix
- All ThreadSanitizer errors found in that file
- Validation: run prerequisite commands before reporting done

**Sub-agent validation (run prerequisite commands):**
Before reporting done, the sub-agent must execute these commands in order:
1. `/lint`
2. `/check`

Execute each command ONE AT A TIME using a sub-sub-agent:
- Tool: `Task`, `subagent_type`: `general-purpose`, `model`: `sonnet`
- Prompt: "Read `.agents/commands/<command>.md` and follow its execution instructions"
- Wait for each to complete before starting the next
- If any fails: sub-agent must fix before reporting done

### 3. Verify Results
After ALL sub-agents have completed, re-run `make check-tsan`.
- If it passes: report success
- If issues remain: compare to initial count

### 4. Progress Evaluation
If issues remain after a full pass:
- **Forward progress** = fewer total ThreadSanitizer errors than before this pass
- If progress: return to Step 1 with remaining issues
- If no progress: stop and report to user

Then wait for instructions.
