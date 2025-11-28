# Execute Task (Automated Mode)

Execute a single task and return a structured JSON report for automated processing.

Default task directory: `.tasks` (override with `<task-dir>` if specified)

## Process

1. **Get next task:**
   ```bash
   deno run --allow-read .agents/scripts/task-next/run.ts [<task-dir>]
   ```
   - Extract task number: `jq -r '.data.task.number'`
   - Extract task path: `jq -r '.data.task.path'`

2. **Start the task:**
   ```bash
   deno run --allow-read --allow-write .agents/scripts/task-start/run.ts [<task-dir>] <task-number>
   ```

3. **Read task file and verify prerequisites**
   - Check that all prerequisite tasks are complete
   - Check environment requirements
   - If blocked, return status "blocked"

4. **Execute each step using a sub-agent**
   - For each step in the task file:
     a. Extract: Documentation, Context, Execute instructions, Success Criteria
     b. Determine appropriate model for step:
        - **Use Haiku** for simple verification steps:
          - Running make targets (make check, make lint, make coverage, make check-dynamic)
          - Verifying exit codes
          - Checking file existence
          - Parsing command output for pass/fail
        - **Use Sonnet** for complex steps:
          - Writing code (tests or implementation)
          - Refactoring code
          - Debugging failures
          - Making decisions about file structure
          - Analyzing coverage gaps and writing tests
     c. Spawn sub-agent with Task tool:
        - subagent_type="general-purpose"
        - model="haiku" or model="sonnet" based on step complexity
     d. Provide sub-agent with: all documentation to read, step context, execute instructions, success criteria
     e. Wait for sub-agent to complete
     f. Verify sub-agent met the step's success criteria
     g. If step failed: mark task as failed and stop
     h. If step succeeded: continue to next step
   - All steps must succeed for task to succeed

5. **Aggregate results from all steps**
   - Collect success criteria from all steps
   - Collect any outputs produced
   - Determine overall task status

6. **Collect outputs defined in task file**
   - Extract values or paths for named outputs
   - These will be available to subsequent tasks

7. **Mark task as done:**
   ```bash
   deno run --allow-read --allow-write .agents/scripts/task-done/run.ts [<task-dir>]
   ```

8. **Return ONLY the JSON report** - no other text, no markdown blocks, just raw JSON

## JSON Response Format

```json
{
  "task": "<task-number>",
  "status": "success|failed|blocked",
  "success_criteria": [
    {
      "criterion": "<exact text from task file>",
      "met": true,
      "verification": "<how you verified it - command and result>"
    }
  ],
  "outputs": {
    "<output-name>": "<value-or-path>"
  },
  "error": "<present only if status is failed or blocked>",
  "notes": "<optional context or warnings>"
}
```

## Status Values

- **success**: All success criteria met, task completed successfully
- **failed**: One or more criteria failed, task cannot complete
- **blocked**: Prerequisites not met or environment issue, cannot proceed

## Critical Rules

1. **Actually verify** - Don't assume success, run verification commands
2. **Be honest** - Report failures accurately, don't hide errors
3. **Raw JSON only** - Your final response must be ONLY the JSON, no markdown, no explanations
4. **Complete execution** - Execute all steps even if some fail (need complete picture)
5. **Always mark done** - Call task-done/run.ts before returning (even on failure)

## Example Success Response

{
  "task": "2",
  "status": "success",
  "success_criteria": [
    {
      "criterion": "All tests pass",
      "met": true,
      "verification": "Ran 'make check' - exit code 0, 156/156 tests passed"
    },
    {
      "criterion": "Coverage at 100%",
      "met": true,
      "verification": "Checked coverage/summary.txt - lines: 100.0%, functions: 100.0%, branches: 100.0%"
    }
  ],
  "outputs": {
    "test_results": "build/test-results.xml",
    "coverage_report": "coverage/summary.txt"
  },
  "notes": "Build completed in 45 seconds"
}

## Example Failure Response

{
  "task": "3",
  "status": "failed",
  "success_criteria": [
    {
      "criterion": "All unit tests pass",
      "met": false,
      "verification": "Ran 'make unit' - exit code 1, 154/156 tests passed, 2 failures"
    },
    {
      "criterion": "No memory leaks detected",
      "met": true,
      "verification": "Checked test output - no leak reports"
    }
  ],
  "outputs": {},
  "error": "2 unit tests failed: test_array_bounds, test_null_handling"
}

## Example Blocked Response

{
  "task": "2.3",
  "status": "blocked",
  "success_criteria": [],
  "outputs": {},
  "error": "Prerequisite task 2.1 not complete"
}
