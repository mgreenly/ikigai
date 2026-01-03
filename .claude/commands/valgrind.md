Run valgrind memory checks and fix issues using sub-agents.

**Usage:**
- `/valgrind` - Run make check-valgrind and dispatch sub-agents to fix memory issues

**Action:** Executes `make check-valgrind` and uses sub-agents with the coverage skillset to fix any memory errors (leaks, invalid access) found. Sub-agents run sequentially (one per file) and continue until all issues are resolved.

---

## Execution

### 1. Initial Check
Run `make check-valgrind`. If it passes, report success and stop.
If it fails, identify all files with memory errors.

### 2. Fix Issues With Sub-Agents

**Scope:** One sub-agent per file. Each sub-agent fixes ALL memory errors in its assigned file.

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
- The skillset to load: `coverage`
- The specific file to fix
- All memory errors found in that file
- Validation: run prerequisite commands before reporting done

**Sub-agent validation (run prerequisite commands):**
Before reporting done, the sub-agent must execute these commands in order:
1. `/lint`
2. `/check`
3. `/sanitize`
4. `/coverage`

Execute each command ONE AT A TIME using a sub-sub-agent:
- Tool: `Task`, `subagent_type`: `general-purpose`, `model`: `sonnet`
- Prompt: "Read `.claude/commands/<command>.md` and follow its execution instructions"
- Wait for each to complete before starting the next
- If any fails: sub-agent must fix before reporting done

### 3. Verify Results
After ALL sub-agents have completed, re-run `make check-valgrind`.
- If it passes: report success
- If issues remain: compare to initial count

### 4. Progress Evaluation
If issues remain after a full pass:
- **Forward progress** = fewer total memory errors than before this pass
- If progress: return to Step 1 with remaining issues
- If no progress: stop and report to user

Then wait for instructions.
