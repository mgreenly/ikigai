Run lint checks and fix issues using sub-agents.

**Usage:**
- `/lint` - Run make lint and dispatch sub-agents to fix lint issues

**Action:** Executes `make lint` and uses sub-agents with the coverage skillset to fix any lint errors found. Sub-agents run sequentially (one per file) and continue until all issues are resolved.

---

## Execution

### 1. Initial Check
Run `make lint`. If it passes, report success and stop.
If it fails, identify all files with lint errors.

### 2. Fix Issues With Sub-Agents

**Scope:** One sub-agent per file. Each sub-agent fixes ALL lint errors in its assigned file.

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
- All lint errors found in that file
- **File size violations:** If file exceeds size limit, the preferred approach is to split it into multiple files using logical structure separation (e.g., group related tests, separate helper functions, split by functionality)
- Validation: run `make lint` before reporting done (no prerequisites)

### 3. Verify Results
After ALL sub-agents have completed, re-run `make lint`.
- If it passes: report success
- If issues remain: compare to initial count

### 4. Progress Evaluation
If issues remain after a full pass:
- **Forward progress** = fewer total lint errors than before this pass
- If progress: return to Step 1 with remaining issues
- If no progress: stop and report to user

Then wait for instructions.
