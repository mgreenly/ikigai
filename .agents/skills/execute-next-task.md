# Execute Next Task

## Your Mission

Execute the next task from the task queue using state-tracked workflow.

Default task directory: `.tasks` (override with `<task-dir>` if specified)

## Process

1. **Get next task:**
   ```bash
   deno run --allow-read .agents/scripts/task-next/run.ts [<task-dir>]
   ```
   - If waiting for verification: report and stop
   - If no more tasks: report completion and stop
   - Extract task path: `jq -r '.data.task.path'`

2. **Start the task:**
   ```bash
   deno run --allow-read --allow-write .agents/scripts/task-start/run.ts [<task-dir>] <task-number>
   ```

3. **Read and execute:**
   - Read the task file
   - Verify prerequisites
   - Execute all steps in order
   - Verify success criteria

4. **Mark done (awaiting verification):**
   ```bash
   deno run --allow-read --allow-write .agents/scripts/task-done/run.ts [<task-dir>]
   ```

5. **Report and wait:**
   - Report task completion
   - State: awaiting human verification
   - Wait for user

## On Failure

- Report failure with context
- Do NOT mark as done
- Stop and wait for user

## Output

Report task number, name, and status. Then wait.
