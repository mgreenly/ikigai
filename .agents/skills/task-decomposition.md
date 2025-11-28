# Task Decomposition

## Description
Strategic skill for breaking down discussions and requirements into task-level work units and delegating detailed planning to task-architect sub-agents.

## Your Role as Task Strategist

You are the **task strategist** - you think in phases and tasks, not steps.

**What you do:**
1. Read phase documentation to understand objectives
2. Identify 5-10 major tasks needed to accomplish the phase
3. Spawn task-architect sub-agents in parallel to create detailed task files
4. Verify all task files were created
5. Initialize the task system

**What you don't do:**
- Create detailed step-by-step instructions (task-architects do this)
- Write test code or implementation details (executors do this)
- Get bogged down in syntax or file paths (task-architects handle this)

## Input Required

Before starting, you MUST have:
1. **Task folder path**: Where to create task files (e.g., `.tasks/feature-auth`)
2. **Phase/requirement description**: What needs to be accomplished
3. **Phase context**: Current project phase from docs/README.md

If any are missing, ask the user.

## Process

### 1. Understand the Context

Read relevant documentation:
- `docs/README.md` - Overall roadmap, completed work, documented phases
- Phase-specific docs if they exist (e.g., `docs/v1-llm-integration.md`)
- Related ADRs in `docs/decisions/` if applicable

**Understand:**
- What's already been accomplished (completed phases)
- What's documented in the roadmap (existing phases)
- What the discussion is about (may be new or existing phase)
- Integration points with existing systems
- Key components needed
- Success criteria for the work

### 2. Identify High-Level Tasks

Break the work into 5-10 major task phases. For each task:

**Define:**
- **Task number**: Sequential numbering (1, 2, 3...)
- **Task name**: Short descriptive name (2-4 words, kebab-case)
- **Phase type**: One of:
  - `TDD` - Code development following test-driven workflow
  - `setup` - Environment/infrastructure setup
  - `integration` - Connecting components together
  - `testing` - Validation and verification
  - `custom` - Unique workflow
- **Description**: 1-2 sentences about what this task accomplishes
- **Context**: Relevant files, integration points, prerequisites

**Example task list:**
```
1. openai-client (TDD) - Implement HTTP client with libcurl streaming for OpenAI API
   Context: Integrate with layer_cake architecture, handle streaming responses

2. conversation-state (TDD) - Create in-memory conversation management with checkpoint/rollback
   Context: Track messages, support /mark and /rewind commands

3. slash-commands (TDD) - Implement command parsing and execution (/clear, /mark, /rewind, /help)
   Context: Integrate with REPL input processing, modify conversation state

4. status-indicators (TDD) - Add loading spinner and error display to layer cake
   Context: Visual feedback during API calls, integrate with scrollback layer

5. integration (integration) - Wire all components together and verify end-to-end flow
   Context: User input → API call → streamed response → terminal display
```

### 3. Classify Task Types

**TDD tasks** (most common for feature development):
- Use standard 15-step workflow with Sonnet/Haiku delegation
- Write tests first, implement, refactor, coverage, lint, sanitizers, commit
- Examples: New modules, API clients, data structures, algorithms

**Setup tasks:**
- Database schema creation
- Configuration file setup
- Dependency installation
- Directory structure

**Integration tasks:**
- Connecting multiple components
- End-to-end workflow verification
- Interface adaptation

**Testing tasks:**
- Performance benchmarks
- Integration test suites
- Verification scenarios

**Custom tasks:**
- Anything that doesn't fit the above patterns
- Requires unique workflow

### 4. Spawn Task-Architect Sub-Agents

**IMPORTANT:** Spawn all sub-agents in PARALLEL (one message with multiple Task tool calls).

For each task, create a sub-agent with this prompt template:

```
You are a task-architect creating a detailed task file.

Read `.agents/skills/task-system.md` for task system architecture.
Read `.agents/skills/create-tasks.md` for detailed task file structure and templates.

Create a task file for:

**Task Number:** {number}
**Task Name:** {name}
**Task Type:** {TDD|setup|integration|testing|custom}
**Description:** {what this accomplishes}
**Context:** {relevant files, integration points, requirements}

{IF TDD:}
Use the standard 15-step TDD workflow template with Sonnet/Haiku delegation:
1. Write Failing Tests (Sonnet)
2. Verify Tests Fail (Haiku)
3. Implement Code (Sonnet)
4. Verify Tests Pass (Haiku - with retry loop to step 3, max 3 times)
5. Refactor (Sonnet)
6. Verify Refactor (Haiku)
7. Achieve Coverage (Sonnet)
8. Verify Coverage (Haiku)
9. Check Lint (Haiku)
10. Fix Lint Issues (Sonnet - conditional)
11. Re-verify Coverage (Haiku)
12. Fix Coverage Gaps (Sonnet - conditional)
13. Sanitizer Validation (Haiku)
14. Draft Commit Message (Sonnet)
15. Create Commit (Haiku)

{IF setup/integration/testing/custom:}
Define appropriate custom steps based on the task type.

{ENDIF}

Each step must include:
- **Documentation:** Files to read (always include `.agents/skills/default.md`)
- **Context:** Step-specific context and considerations
- **Execute:** Specific actions to take
- **Success Criteria:** Measurable outcomes

Follow the task file structure from create-tasks.md:
- # Task: [title]
- ## Description (exactly 2 sentences)
- ## Context (overall context)
- ## Prerequisites (list or "None")
- ## Steps (numbered with Documentation/Context/Execute/Success Criteria)
- ## Outputs (named outputs or "None")
- ## Post-Work (Action: wait|notify|continue, Message: ...)
- ## On Failure (Retry: yes|no, Block: yes|no, Fallback: ...)

**Your task:**
1. Design the complete task file following the structure
2. Write the file to: {task-folder}/{number}-{name}.md
3. Verify the file was written successfully
4. Return ONLY: "Task file {number}-{name}.md created with {N} steps"

IMPORTANT: Write the file yourself. Do NOT return markdown content.
```

**Why parallel execution:**
- Each sub-agent works independently
- Saves time (not sequential)
- Reduces parent agent context usage
- More efficient overall

### 5. Verify Task Creation

After all sub-agents return:

**Check:**
- All sub-agents reported success
- All expected task files exist in the task folder
- File naming follows `{number}-{name}.md` pattern
- Numbering is sequential (1, 2, 3, ...)

**If any failed:**
- Identify which task files are missing
- Spawn replacement sub-agents for failed tasks
- Verify again

### 6. Initialize Task System

Create `state.json` in the task folder:
```json
{
  "current": null,
  "status": "pending"
}
```

Create `README.md` in the task folder:
```markdown
# {Phase Name} - Task Series

## Overview
{Brief description of what this phase accomplishes}

## Phase Context
{Reference to docs/README.md phase and related documentation}

## Tasks

1. **{number}-{name}.md** - {brief description}
2. **{number}-{name}.md** - {brief description}
...

Total: {N} tasks

## Execution

**Automated:** `/auto-tasks` - Execute all tasks with sub-agent orchestration
**Manual:** `/next-task` - Execute tasks one at a time with human verification

## Dependencies

{Any prerequisites or setup required before starting}

## Success Criteria

{How to know this phase is complete}
```

### 7. Report Summary

Provide a concise summary:

```
✅ Task series created: {Work Name}

Context: {brief description of how this relates to project roadmap}
Tasks identified: {N}
Task-architects spawned: {N} (in parallel)

Tasks created:
1. {number}-{name}.md - {brief description} ({type})
2. {number}-{name}.md - {brief description} ({type})
...

Location: {task-folder}/
State: Initialized and ready

Next: /auto-tasks (automated) or /next-task (manual)
```

## Task Naming Conventions

**File naming:** `{number}-{name}.md`
- Numbers: Sequential (1, 2, 3, ...) or hierarchical (1.1, 1.2, 2.1)
- Names: kebab-case, 2-4 words, descriptive
- Examples: `1-openai-client.md`, `2-conversation-state.md`

**Task names should:**
- Describe the deliverable (not the action)
- Be concise (2-4 words)
- Use technical terms from domain
- Avoid generic words like "implement" or "create" in the name

**Good names:**
- `openai-client` (deliverable: OpenAI API client)
- `conversation-state` (deliverable: conversation state management)
- `slash-commands` (deliverable: slash command system)

**Avoid:**
- `implement-api` (too generic)
- `write-openai-code` (action-focused, not deliverable-focused)
- `api-client-with-streaming-and-error-handling` (too long)

## Handling Complexity

**If a task is too large:**
- Break into subtasks using hierarchical numbering
- Example: `1-openai-client.md` becomes:
  - `1.1-http-client.md`
  - `1.2-json-parsing.md`
  - `1.3-streaming-handler.md`

**When to use subtasks:**
- Task has 10+ steps
- Task spans multiple distinct concerns
- Task has clear sub-phases

**Avoid over-decomposition:**
- Don't create subtasks for simple sequences
- Keep related work together when possible
- Balance granularity with cohesion

## Common Patterns

### New Feature Development
Typical task breakdown:
1. Core functionality (TDD)
2. Integration with existing systems (integration)
3. UI/UX components (TDD)
4. End-to-end tests (testing)
5. Documentation (custom)

### Infrastructure Setup
Typical task breakdown:
1. Environment preparation (setup)
2. Schema/structure creation (setup)
3. Configuration (setup)
4. Verification (testing)

### Refactoring/Cleanup
Typical task breakdown:
1. Analysis and planning (custom)
2. Extract components (TDD for each)
3. Update integration points (integration)
4. Remove old code (custom)
5. Verification (testing)

## Delegation Best Practices

**What to tell task-architects:**
- Clear task number and name
- Definitive phase type (TDD, setup, etc.)
- High-level description and objectives
- Relevant documentation references
- Integration context

**What NOT to tell them:**
- Specific file paths or line numbers
- Implementation approaches
- Code syntax or structure
- Step-by-step procedures

**Let task-architects figure out:**
- Which documentation to reference in each step
- Exact success criteria
- How to structure steps
- Model selection (Haiku vs Sonnet) for steps

## Troubleshooting

**Sub-agent returns markdown instead of writing file:**
- Prompt must clearly say "Write the file yourself"
- Emphasize "Do NOT return markdown content"
- State expected output: "Return ONLY: 'Task file...'"

**Task breakdown doesn't seem right:**
- Re-read project documentation for context
- Verify understanding of work objectives from discussion
- Consider if you've misunderstood the requirements
- Ask user for clarification

**Too many or too few tasks:**
- Aim for 5-10 tasks for the work
- Each task should be completable independently
- Tasks should be cohesive (not arbitrary splits)
- Consider if the work scope needs adjustment

## Related Skills

- `phase-planner.md` - Understanding project phases
- `task-system.md` - Task system architecture
- `create-tasks.md` - Detailed task file creation (for task-architects)

## Summary

As a task strategist using task-decomposition:
1. Read project documentation to understand context and roadmap
2. Identify 5-10 major tasks (deliverables) based on discussion
3. Classify each task type (TDD, setup, integration, testing, custom)
4. Spawn task-architect sub-agents in parallel with clear instructions
5. Verify all task files created successfully
6. Initialize task system (state.json, README.md)
7. Report summary with next steps

You focus on **what** needs to be done. Task-architects focus on **how** to do it.

The roadmap in docs/README.md provides context but doesn't constrain you. You can define new work based on discussions.
