# Task: /fork with Prompt Argument

## Target
User Story: 04-fork-with-prompt.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- project/agent-process-model.md (Fork with prompt section)

## Pre-read Source (patterns)
- src/commands.c (READ & MODIFY - cmd_fork, argument parsing)
- src/conversation.h (READ - message handling, ik_conversation_add_user_msg)
- src/llm.h (READ - LLM call trigger, ik_llm_start_streaming)

Note: These source files will be created/modified in fork-cmd-basic.md. conversation.h and llm.h should already exist from earlier work.

## Pre-read Tests (patterns)
- tests/unit/commands/cmd_fork_test.c (MODIFY - add prompt argument tests)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- fork-cmd-basic.md complete (cmd_fork exists and basic /fork works)
- conversation.h exists with ik_conversation_add_user_msg function
- llm.h exists with ik_llm_start_streaming function

## Task
Extend `/fork` to accept an optional prompt argument. When provided, the prompt is added as a user message and triggers an LLM call.

**Command:**
```
/fork "Research OAuth 2.0 patterns"
```

**Note:** Concurrency control (fork_pending) and transaction semantics are already implemented in fork-cmd-basic.md. This task extends the existing cmd_fork() to handle the prompt argument.

**Extended flow:**
1. Parse quoted prompt from args
2. Create child (same as basic fork - within existing transaction)
3. If prompt provided:
   - Append prompt as user message to child's history
   - Trigger LLM call on child

## TDD Cycle

### Red
1. Add tests to `tests/unit/commands/cmd_fork_test.c`:
   - Test /fork with quoted prompt extracts prompt
   - Test prompt appended as user message
   - Test LLM call triggered
   - Test /fork "" (empty prompt) treated as no prompt
   - Test /fork with unquoted text rejected

2. Run `make check` - expect test failures

### Green
1. Update `cmd_fork()` to parse prompt:
   ```c
   res_t cmd_fork(ik_repl_ctx_t *repl, const char *args)
   {
       // Extract prompt if present
       char *prompt = NULL;
       if (args && args[0] == '"') {
           // Parse quoted string...
       }

       // Create child (existing code)
       // ...

       // If prompt provided
       if (prompt && prompt[0] != '\0') {
           // Append as user message
           ik_conversation_add_user_msg(child->conversation, prompt);

           // Trigger LLM call
           CHECK(ik_llm_start_streaming(child));
       }

       return OK(NULL);
   }
   ```

2. Run `make check` - expect pass

### Refactor
1. Extract prompt parsing to helper function
2. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` verification after each TDD phase
- Use haiku sub-agents for running `make lint` verification
- Use haiku sub-agents for git commits after completing each TDD phase (Red, Green, Refactor)

## Post-conditions
- `make check` passes
- `make lint` passes
- `/fork "prompt"` parses prompt correctly
- Prompt becomes user message in child
- LLM call triggered when prompt provided
- Working tree is clean (all changes committed)
