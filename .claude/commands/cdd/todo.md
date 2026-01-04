---
description: Work through a todo file, tracking completed work.
---

**Requires:** `$CDD_DIR` environment variable must be set to the workspace directory.

Process the first incomplete item in `$ARGUMENTS`.

**Workflow:**
1. Read the todo file at `$ARGUMENTS`
2. Find first unchecked item (`- [ ]`)
3. Use the Task tool to run a sub-agent with `run_in_background=false` to complete the task
4. Mark it done (`- [x]`) in the source file
5. Append bullet to `$CDD_DIR/completed.md`: `- <task description> (from <filename>)`
6. Commit all changed files
7. Suggest the next incomplete item in the file

**Validation:**
- If `$ARGUMENTS` is empty: "Usage: /cdd:todo <filepath>"
- If file doesn't exist: "Error: File not found: <filepath>"
- If no incomplete items: "All items complete in <filepath>"
