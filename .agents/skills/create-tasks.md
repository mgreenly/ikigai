# Create Tasks

## Description
Break down a complex body of work into a hierarchical series of task files using decimal numbering. This process is idempotent - you can run it multiple times to refine tasks based on iterative discussion.

## System Overview

This is **part of a two-phase task execution system**:

1. **Task Creation** (this prompt): An agent breaks down complex work into a series of numbered task files
2. **Task Execution** (separate prompt): A sub-agent reads and executes tasks sequentially in numerical order

**How it works:**
- Tasks are stored as markdown files in a designated folder
- Each task file contains structured instructions, prerequisites, success criteria, and outputs
- A task executor sub-agent processes tasks in order based on decimal numbering
- Tasks can pass outputs to subsequent tasks for complex workflows
- Tasks can be created once and then refined iteratively through discussion

**Your role:** You are creating or updating the task series that will be executed later by a sub-agent.

## Idempotent Task Creation

The task creation process supports iterative refinement:

1. **First run:** Creates initial task breakdown from discussion
2. **Subsequent runs:** Updates tasks based on new discussion context
3. **Preserves:** Task numbering, state.json (execution progress)
4. **Updates:** Task content, steps, success criteria based on discussion

**When updating tasks:**
- Read existing task files to understand current structure
- Identify what needs to change based on discussion
- Preserve task numbers to maintain execution state
- Rewrite task files with updated content
- Report what changed (created/updated/unchanged)

## Context Optimization Strategy

Creating detailed task files for complex projects can use significant context. To optimize:

1. **You identify high-level phases** (5-10 major tasks)
2. **You spawn sub-agents in parallel** (one per phase) to create detailed task files
3. **Sub-agents write task files directly and report success** with brief confirmation messages
4. **You verify all files were created** and initialize the task system

**Benefits:**
- Each sub-agent focuses on one task's details (smaller context per agent)
- Parallel execution saves time
- Parent agent context saved - no large markdown returns
- More efficient - files written once by sub-agents
- You orchestrate without getting bogged down in details
- Final task files are comprehensive and complete

## Decimal Numbering System Explained

The numbering system allows **unlimited hierarchical depth** with natural sorting:

**How it works:**
- Decimal points create hierarchy: `1.1` is a subtask of `1`
- You can insert tasks between existing ones by adding decimals
- Natural sort order: compare left-to-right, digit by digit

**Examples:**
```
1              (first top-level task)
1.1            (first subtask of 1, comes after 1)
1.1.1          (first sub-subtask of 1.1, comes after 1.1)
1.1.2          (second sub-subtask of 1.1)
1.2            (second subtask of 1, comes after all 1.1.x tasks)
2              (second top-level task)
2.1            (first subtask of 2)
```

**Insertion capability:**
If you have tasks `1` and `2`, you can insert:
- `1.1` between them (subtask of 1)
- `1.5` between `1.1` and `1.9`
- `1.1.1` to add detail under `1.1`

**Sorting rules:**
- `1` comes before `1.1` (parent before children)
- `1.1` comes before `1.2` (numerical order within same level)
- `1.1.1` comes before `1.2` (complete subtree before siblings)
- `1.9` comes before `1.10` (not "1.9" vs "1.1" - full decimal comparison)

**File naming with numbers:**
- Filename: `{number}-{short-name}.md`
- Examples: `1-setup.md`, `1.1-database.md`, `1.1.1-schema.md`
- The number prefix enables automatic sorting
- The name provides human readability

## Input Required

Before starting, you MUST receive:
1. **Task folder path**: Where to create the task files (e.g., `./tasks/feature-auth`)
2. **Problem description**: The work to be broken down into tasks

If either is missing, ask the user before proceeding.

## Task Numbering System

Use hierarchical decimal numbering:
- `1`, `2`, `3` - Top-level tasks
- `1.1`, `1.2`, `1.3` - Subtasks of task 1
- `1.1.1`, `1.1.2` - Sub-subtasks of task 1.1

Natural sort order: `1 → 1.1 → 1.1.1 → 1.1.2 → 1.2 → 2 → 2.1`

**File naming**: `{number}-{short-name}.md`
- Examples: `1-setup.md`, `1.1-database.md`, `1.1.1-schema.md`
- Use lowercase with hyphens for the name portion
- Keep names short (2-4 words max)

## Task File Structure

Each task file MUST follow this exact markdown structure:

```markdown
# Task: [Short Title]

## Description
[Exactly 2 sentences describing the goal and why it matters]

## Context
[Overall context about what needs to be done, relevant files, integration points, etc.]

## Prerequisites
- [List any required prior tasks by number, e.g., "Task 1.1 must be completed"]
- [List any environment requirements, e.g., "DuckDB must be installed"]
- [If none: write "None"]

## Steps

### 1. [Step Name]
**Documentation:**
- Read `.agents/skills/default.md` for project context
- [Other relevant documentation files]

**Context:**
- [Step-specific context and considerations]

**Execute:**
- [Specific actions to take]

**Success Criteria:**
- [Measurable outcomes for this step]

### 2. [Next Step Name]
[Same structure...]

## Outputs
- `output_name`: Description of what this produces for later tasks
- `another_output`: Path or value available to subsequent tasks
- [If none: write "None"]

## Post-Work
- **Action**: [wait | notify | continue]
- **Message**: [What to communicate to user or context for next task]

## On Failure
- **Retry**: [yes | no]
- **Block subsequent tasks**: [yes | no]
- **Fallback**: [Alternative action or "None"]
```

**Important:** Each step within a task will be executed by its own sub-agent, so steps must be self-contained with all necessary documentation references and context.

## Standard Task Templates

### Code Development Task (TDD Workflow)

When creating tasks for new code development (features, modules, functions), use this standard 15-step workflow with clear Sonnet/Haiku delegation:

**Step 1: Write Failing Tests** (Sonnet - code generation)
- **Documentation:** default.md, test code style, naming conventions, include ordering
- **Context:** Write comprehensive test cases covering happy path, error cases, and edge conditions
- **Execute:** Create/modify test files with thorough test coverage
- **Success Criteria:** Tests compile successfully, cover all planned scenarios

**Step 2: Verify Tests Fail** (Haiku - verification)
- **Documentation:** default.md
- **Context:** Confirm tests fail as expected before implementation exists
- **Execute:** Run `make check`, verify tests fail with expected error messages (not crashes)
- **Success Criteria:** Tests fail correctly, no segfaults or unexpected errors

**Step 3: Implement Code** (Sonnet - code generation)
- **Documentation:** default.md, naming conventions (docs/naming.md), memory management (docs/memory.md), error handling (docs/error_handling.md), include ordering
- **Context:** Write implementation to pass all tests
- **Execute:** Create/modify source files, implement functionality following project conventions
- **Success Criteria:** Code follows ik_MODULE_THING conventions, proper memory/error patterns

**Step 4: Verify Tests Pass** (Haiku - verification with retry loop)
- **Documentation:** default.md
- **Context:** Confirm all tests pass; if failures occur, loop back to step 3 (max 3 iterations)
- **Execute:** Run `make check`, report results; if failures, return error details to step 3
- **Success Criteria:** All tests pass with exit code 0, or max retries exhausted (then escalate)

**Step 5: Refactor** (Sonnet - code analysis and modification)
- **Documentation:** default.md, naming conventions, avoid over-engineering
- **Context:** Improve code clarity and eliminate redundancy without changing behavior
- **Execute:** Eliminate duplication, extract helpers if needed (avoid premature abstraction), run `make fmt`
- **Success Criteria:** No duplication, focused functions, code formatted

**Step 6: Verify Refactor** (Haiku - verification)
- **Documentation:** default.md
- **Context:** Confirm refactoring didn't break tests
- **Execute:** Run `make check`, verify all tests still pass
- **Success Criteria:** All tests pass with exit code 0

**Step 7: Achieve Coverage** (Sonnet - test generation)
- **Documentation:** default.md coverage section, finding gaps with grep, LCOV exclusions
- **Context:** Add tests to reach 100% coverage
- **Execute:** Run `make coverage`, identify gaps with grep/lcov, write additional tests, repeat until 100%
- **Success Criteria:** Ready for coverage verification

**Step 8: Verify Coverage** (Haiku - verification)
- **Documentation:** default.md coverage section
- **Context:** Confirm 100% line, function, and branch coverage achieved
- **Execute:** Run `make coverage`, parse output for coverage percentages
- **Success Criteria:** 100.0% line/function/branch coverage

**Step 9: Check Lint** (Haiku - verification)
- **Documentation:** default.md, 16KB file size limit
- **Context:** Check for lint violations and file size issues
- **Execute:** Run `make lint`, capture any errors or warnings
- **Success Criteria:** Either passes cleanly, or reports specific issues for step 10

**Step 10: Fix Lint Issues** (Sonnet - code modification, conditional)
- **Documentation:** default.md, 16KB file size limit, complexity checks
- **Context:** Address lint failures from step 9 (skip if no issues)
- **Execute:** Fix lint errors; if files >16KB split into focused modules, verify tests pass after splits
- **Success Criteria:** All lint issues resolved, files under 16KB, tests still pass

**Step 11: Re-verify Coverage** (Haiku - verification)
- **Documentation:** default.md coverage section
- **Context:** Confirm coverage still 100% after lint fixes
- **Execute:** Run `make coverage`, verify percentages
- **Success Criteria:** Either 100% maintained, or report gaps for step 12

**Step 12: Fix Coverage Gaps** (Sonnet - test generation, conditional)
- **Documentation:** default.md coverage section
- **Context:** Add tests if coverage dropped during lint fixes (skip if still 100%)
- **Execute:** Identify new gaps, write tests to restore 100% coverage
- **Success Criteria:** Coverage back to 100%

**Step 13: Sanitizer Validation** (Haiku - verification)
- **Documentation:** default.md test execution and pre-commit requirements
- **Context:** Run sanitizer builds to detect memory errors, data races, undefined behavior
- **Execute:** Run `make BUILD=sanitize check && make BUILD=tsan check`, verify clean output
- **Success Criteria:** No sanitizer errors, no memory leaks, no data races
- **Note:** If errors found, task fails - report to user for manual fix

**Step 14: Draft Commit Message** (Sonnet - message composition)
- **Documentation:** default.md git configuration and commit policy
- **Context:** Create clear, concise commit message following project conventions
- **Execute:** Analyze changes, write commit message (focus on "why" not "what"), NO attribution lines, NO emoji, NO Co-Authored-By
- **Success Criteria:** Commit message drafted, ready for step 15

**Step 15: Create Commit** (Haiku - command execution)
- **Documentation:** default.md git configuration and commit policy
- **Context:** Stage files and create commit using message from step 14
- **Execute:** Run `git add <files>`, `git commit -m "<message>"` using exact message from step 14
- **Success Criteria:** Commit created successfully, follows conventions

**When to use this template:**
- Implementing new features
- Adding new modules or functions
- Significant code changes that require testing

**Customization:**
- Add context about feature requirements, integration points, relevant existing files
- Adjust documentation references based on specific needs
- Add feature-specific validation steps if needed

## Breaking Down Work

**Guidelines for task decomposition:**

1. **Start with major phases** (top-level numbers 1, 2, 3...)
   - Setup/preparation
   - Core implementation (use TDD template if coding)
   - Testing/validation
   - Deployment/finalization

2. **Break phases into logical steps** (1.1, 1.2, 1.3...)
   - Each should be completable in one focused session
   - Group related operations together
   - Use standard templates (like TDD workflow) when applicable

3. **Add detail where complexity exists** (1.1.1, 1.1.2...)
   - Multi-step processes need breakdown
   - Don't over-decompose simple tasks

4. **Consider dependencies**
   - Tasks should flow logically
   - Earlier tasks provide outputs needed by later ones
   - Mark prerequisites explicitly

5. **Think about failure scenarios**
   - What if a task fails?
   - Should it retry automatically?
   - Should it block all subsequent tasks or just dependents?

## Process

### 1. Check for Existing Tasks

First, check if the task folder exists and contains task files:

```bash
# Check if folder exists
ls <task-folder>

# If exists, read existing tasks to understand structure
ls <task-folder>/*.md
```

If tasks exist:
- You are **updating** based on discussion
- Read existing task files to understand current breakdown
- Preserve task numbers where appropriate
- Identify what needs to change

If no tasks exist:
- You are **creating** a new task series
- Proceed with fresh analysis

### 2. Analyze the Problem

- Identify major components/phases of work
- Note dependencies and order requirements
- Consider error cases and edge conditions
- Determine which phases should use standard templates (e.g., TDD workflow)
- If updating: compare current structure to what discussion suggests

### 3. Identify High-Level Task Phases

Create a list of top-level tasks (phases). For each phase, define:
- **Task number**: Sequential numbering (1, 2, 3...)
- **Task name**: Short descriptive name (2-4 words)
- **Phase type**: Code development (TDD), setup, testing, custom
- **Brief description**: What this phase accomplishes
- **Context**: Relevant files, integration points, requirements
- **Mode**: create or update (based on whether task file exists)
- **Update reason**: If updating, what needs to change

Example (creating new):
```
1. database-schema - Setup (create tables for users and sessions) - CREATE
2. password-hashing - TDD (implement hash/verify functions) - CREATE
3. session-management - TDD (create/validate/destroy sessions) - CREATE
4. auth-endpoints - TDD (implement login/logout/register) - CREATE
5. integration-tests - Testing (end-to-end auth flows) - CREATE
```

Example (updating existing):
```
1. database-schema - Setup (unchanged) - KEEP
2. password-hashing - TDD (switch to Argon2 instead of bcrypt) - UPDATE
3. session-management - TDD (unchanged) - KEEP
4. auth-endpoints - TDD (add rate limiting) - UPDATE
5. integration-tests - Testing (unchanged) - KEEP
```

### 4. Delegate Task Creation/Update to Sub-Agents

For each phase, spawn a sub-agent to create the detailed task file.

**Spawn all sub-agents in parallel** (one message with multiple Task tool calls) to optimize context usage.

**Prompt template for each sub-agent:**

```
{IF UPDATE: Read existing task file at <task-folder>/{number}-{name}.md}

Create or update a task file for the following phase:

Task Number: {number}
Task Name: {name}
Phase Type: {TDD|setup|testing|custom}
Description: {what this accomplishes}
Context: {relevant files, integration points, requirements}
Mode: {create|update}
Update Instructions: {if updating, what needs to change based on discussion}

{IF TDD: Use the standard 15-step TDD workflow template from create-tasks.md}
{IF setup/testing/custom: Define appropriate custom steps}
{IF UPDATE: Modify existing steps or add new ones as needed}

The task file must follow this structure:
- # Task: [title]
- ## Description (2 sentences)
- ## Context (overall context)
- ## Prerequisites (list or "None")
- ## Steps (numbered steps with Documentation/Context/Execute/Success Criteria subsections)
- ## Outputs (named outputs or "None")
- ## Post-Work (Action: wait, Message: ...)
- ## On Failure (Retry/Block/Fallback)

Each step must include:
- **Documentation:** List of files to read (always include .agents/skills/default.md)
- **Context:** Step-specific context
- **Execute:** Specific actions to take
- **Success Criteria:** Measurable outcomes

**Model Selection Guidance:**
Steps are executed by sub-agents. Simple verification steps (running make targets, checking exit codes) should use Haiku for speed and cost efficiency. Complex steps (writing code, refactoring, debugging) should use Sonnet for better reasoning. The execute-task-auto.md prompt will automatically select the model based on step complexity, but structure steps to make this clear:

- Simple verification: "Run make check and verify exit code 0"
- Complex work: "Implement authentication logic with proper error handling"

Your task is to CREATE or UPDATE and WRITE the task file directly. Do not return the markdown content.

**Process:**
1. If updating: Read existing task file and understand current structure
2. Design the complete task file following the structure above
3. Write the file to: {task-folder}/{number}-{name}.md (overwrites if exists)
4. Verify the file was written successfully
5. Return ONLY a brief success message: "Task file {number}-{name}.md {created|updated} with {N} steps."

IMPORTANT: Write the file yourself. Do NOT return markdown content to the parent agent.
This saves context and is more efficient.
```

**After all sub-agents return:**
- Verify all sub-agents reported success
- Check that all task files exist/updated
- Verify numbering is sequential
- Note which tasks were created vs updated

### 5. Verify All Task Files Created/Updated

- Confirm the task folder exists
- Verify each task file was written/updated by the sub-agents
- Check naming follows pattern: `{number}-{name}.md`
- Ensure all expected files are present
- Track which were created vs updated

### 6. Initialize or Preserve State File

Check if `state.json` exists in the task folder:

**If state.json does NOT exist** (new task series):
Create `state.json`:
```json
{
  "current": null,
  "status": "pending"
}
```

**If state.json EXISTS** (updating tasks):
Leave it unchanged to preserve execution progress.

### 7. Create or Update Task Series Index

Write `README.md` in the task folder:
- List all tasks in order with brief descriptions
- Include overall goals and context
- Note any special dependencies or requirements

## Example Task Breakdown

**Problem**: "Implement user authentication system"

**Resulting task hierarchy**:
```
1-database-schema.md          (Setup users and sessions tables)
2-password-hashing.md          (TDD workflow: implement password hash/verify)
3-session-management.md        (TDD workflow: implement session create/validate/destroy)
4-token-generation.md          (TDD workflow: implement JWT token generation)
5-register-endpoint.md         (TDD workflow: implement user registration)
6-login-endpoint.md            (TDD workflow: implement login with session)
7-logout-endpoint.md           (TDD workflow: implement logout)
8-integration-tests.md         (End-to-end auth flow tests)
```

**Note:** Tasks 2-7 would each follow the standard TDD workflow (15 steps with Sonnet/Haiku delegation). Task 1 and 8 would have custom steps appropriate to their nature.

## Output

After creating/updating all task files, provide:
1. Summary of work identified
2. Number of sub-agents spawned
3. List of task files with status (created/updated/unchanged)
4. Total task count
5. Path to the task folder
6. State preservation status
7. Suggested next step

Example (creating new):
```
✅ Task series created: Authentication System

Phases identified: 5
Sub-agents spawned: 5 (in parallel)

Tasks created:
1. 1-database-schema.md - Setup database tables
2. 2-password-hashing.md - TDD: Password hash/verify (15 steps)
3. 3-session-management.md - TDD: Session lifecycle (15 steps)
4. 4-auth-endpoints.md - TDD: Login/logout/register (15 steps)
5. 5-integration-tests.md - End-to-end auth tests

Total: 5 tasks
Location: .tasks/feature-auth/
State: Initialized and ready

Next: /auto-tasks (automated) or /next-task (manual)
```

Example (updating existing):
```
✅ Task series updated: Authentication System

Changes based on discussion
Sub-agents spawned: 5 (in parallel)

Tasks updated:
- 2-password-hashing.md - Updated to use Argon2 instead of bcrypt
- 4-auth-endpoints.md - Added rate limiting step

Tasks unchanged:
- 1-database-schema.md
- 3-session-management.md
- 5-integration-tests.md

Total: 5 tasks
Location: .tasks/feature-auth/
State: Preserved (current execution state maintained)

Next: Review changes, continue discussion, or run /task-create again to refine further
Once satisfied: /auto-tasks or /next-task to execute
```

## Important Notes

- **Be specific in steps**: "Run `make test`" not "test the code"
- **Make success criteria measurable**: "All tests pass" not "code works"
- **Define outputs clearly**: Other tasks may depend on them
- **Consider the human**: Tasks marked `wait` should explain what the user needs to do
- **Don't over-engineer**: If a task is simple, keep it simple
