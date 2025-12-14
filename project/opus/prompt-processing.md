# Prompt Processing

**Status:** Early design discussion, not finalized.

## Overview

When an agent receives input (user message or sub-agent task prompt), the system pre-processes slash commands before sending content to the LLM. This enables deterministic context loading and task queuing without LLM round-trips.

## Processing Phases

```
/read project/design.md              ─┐
/push name=prep "Prep task"        │ Phase 1: Setup (before LLM)
/run                              ─┘

Implement the feature.            ─── Phase 2: Content (sent to LLM)
Use patterns from the docs.

/push name=tests "Write tests"    ─── Phase 3: Teardown (after LLM finishes)
```

### Phase 1: Leading Slash Commands (Setup)

Commands at the start of input, processed before the LLM sees anything.

**Allowed commands:**
- `/read FILE` - inject file contents as fake tool round-trip
- `/push [options] PROMPT` - queue a task immediately
- `/run [N]` - start queued tasks

**Processing:**
- Executed in order, top to bottom
- Each `/read` injects a fake `tool_use` + `tool_result` into conversation history
- `/push` adds to queue, `/run` starts execution
- Continues until first non-slash-command line

### Phase 2: Content (LLM Execution)

Everything after leading commands until trailing commands or end of input.

- Sent to LLM as user message
- Agent executes normally (tool use, reasoning, etc.)
- Agent can use `/check`, `/receive`, `/send` during execution

### Phase 3: Trailing Slash Commands (Teardown)

Commands after content, processed after the LLM finishes.

**Allowed commands:**
- `/push [options] PROMPT` - queue follow-up tasks

**Processing:**
- Executed after agent completes
- Useful for queuing dependent work

## The Fake Tool Round-Trip

`/read` doesn't call the LLM. It:
1. Reads the file client-side
2. Constructs a fake tool_use message: `{"tool": "read", "path": "..."}`
3. Constructs a fake tool_result message with file contents
4. Injects both into conversation history

The agent sees the file contents as if it had requested them, but no API call occurred.

**Error handling:** If the file doesn't exist, inject an error as the tool_result (same as real tool use would).

## No Recursive Expansion

If `/read foo.md` returns content containing `/read bar.md`, the nested command is NOT expanded. Only top-level prompt commands are processed.

## Examples

### Worker Task (Simple)

```
/read src/auth/README.md
/read project/auth-design.md
Implement JWT validation in src/auth/token.c.
Follow the patterns in the docs above.
Write tests in tests/unit/test_token.c.
Run make check to verify.
```

Timeline:
1. Two files injected into context
2. Agent implements, tests, verifies
3. Agent finishes, result delivered to parent

### Orchestrator Task (With Sub-Tasks)

```
/read project/requirements.md
/push name=research-a "Research approach A"
/push name=research-b "Research approach B"
/run
Analyze the requirements above.
Check research results with /check and incorporate findings.
Decide on best approach and document in DECISION.md.
/push name=implement "Implement the chosen approach from DECISION.md"
```

Timeline:
1. Requirements injected
2. Two research tasks queued
3. Both research tasks started
4. Agent analyzes, checks for research results, decides
5. Agent finishes
6. Implementation task queued (for parent to run later)

### Sequential Pipeline

```
/read project/task-spec.md
Complete Phase 1 of the task spec.
Verify all Phase 1 criteria pass.
/push name=phase2 "/read project/task-spec.md\nComplete Phase 2 of the task spec."
```

Timeline:
1. Spec injected
2. Agent completes Phase 1
3. Agent finishes
4. Phase 2 task queued (contains its own `/read`)

## Commands During Execution

The agent can use these commands during execution (via tool use):

- `/push` - queue additional tasks
- `/run` - start queued tasks
- `/check` - non-blocking check for results
- `/receive` - blocking wait for results
- `/send` - message another agent
- `/queue` - check queue status
- `/inbox` - check inbox status

These are NOT pre-processed; they execute as normal tool calls.

## Personas and Skills

Prompts can load personas/skills using `/read`:

```
/read .agents/personas/worker.md
/read .agents/skills/tdd-workflow.md
/read project/feature-spec.md
Implement the feature following TDD workflow.
```

The persona defines how the agent behaves. The skill provides domain knowledge. The spec provides the specific task.

### Persona Types

**Worker:**
- Reads what prompt says
- Does actual work
- Verifies post-conditions
- Returns result

**Task Builder:**
- Decomposes problems into discrete tasks
- Creates well-structured prompts
- Understands prerequisites, outputs, dependencies

**Supervisor:**
- Runs tasks sequentially or in parallel
- Handles failures: retry vs abort
- Knows transient vs fatal errors
- Aggregates results, reports to user

### Example Persona (Worker)

```markdown
# Worker Persona

You are a worker agent. Your job is to complete a specific task.

## Behavior

1. Read any files specified in your prompt
2. Verify pre-conditions are met
3. Do the work described
4. Verify post-conditions are met
5. Report results clearly

## On Failure

If pre-conditions aren't met, report what's missing.
If work fails, report the error with full context.
Do not retry - let your parent decide.

## Output

End with a clear summary:
- What was done
- What was verified
- Any issues encountered
```

### Example Persona (Supervisor)

```markdown
# Supervisor Persona

You orchestrate task execution. You decide what runs, handle failures, and report progress.

## Behavior

1. Push tasks to queue
2. Run tasks (sequential, parallel, or batched)
3. Collect results via /receive or /check
4. On failure, decide: retry (transient) or abort (fatal)
5. Report final status to user

## Failure Classification

**Transient (retry up to 3 times):**
- Network timeout
- Resource temporarily unavailable

**Fatal (abort immediately):**
- Code compilation error
- Test failure
- Missing file
- Pre-condition not met

## Output

Report each task: started, succeeded, failed.
On completion: summary of all tasks.
On abort: which task failed and why.
```

## Related

- [agents.md](agents.md) - Agent types
- [agent-queues.md](agent-queues.md) - Queue and inbox operations
- [architecture.md](architecture.md) - System architecture
