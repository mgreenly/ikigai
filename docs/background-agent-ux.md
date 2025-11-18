# Background Agents: Parallel Task Execution with Memory Documents

## The Concept

When a coding agent performs research, refactoring, or testing, the entire conversation transcript stays in your foreground context—consuming tokens on work that may not be relevant to your next task. Additionally, these tasks block your progress: you can't continue working while the agent completes long-running work.

**Background agents** solve both problems by running tasks in isolated processes that produce distilled **memory documents** instead of raw transcripts. These memory documents:

- Are **permanent** database artifacts, not session-scoped
- Survive `/rewind`, `/clear`, and session restarts
- Enter your context **only when you explicitly pull them in**
- Accumulate as a **persistent knowledge base** about your codebase

This complements `/mark` and `/rewind` for surgical context management, giving you complete control over what enters your context window.

**Key insight**: You can spawn expensive background tasks, clear your entire session, and later pull in just the results you need—building permanent knowledge without context pollution.

## How It Works

**Core principles:**
- **Explicit control**: You spawn tasks and pull in results when ready
- **Status visibility**: Status line shows activity without interruption
- **Pause-and-ask**: Background agents request clarification when needed
- **Distilled output**: Memory documents are structured findings, not transcripts
- **Pattern-based composability**: Simple glob patterns for batch operations

**Flow**: Spawn background task → runs in isolation → produces memory document → store in database → pull into context when needed

## Quick Start

**Spawn a background task:**
```
/background Research OpenAI streaming API - how it works, message format, error handling
```

**Check status:**
```
[Status line: BG: 1 running]           # Task is running
[Status line: 📬 1]                    # Result is ready
```

**Review results:**
```
/inbox list                            # Show all tasks
/inbox show 1                          # Preview memory document
```

**Use the results:**
```
/inbox merge 1                         # Pull into context permanently
```

Or reference temporarily:
```
User: Implement streaming using #mem-124
(Memory doc is loaded just for this request, then removed from context)
```

## Examples

### Example 1: Simple Background Research

User wants to research OpenAI streaming while continuing other work.

```
User: /background Research OpenAI streaming API - how it works, message format, error handling

[Status line: BG: 1 running]

User: Now let's work on config validation

Agent: Looking at src/config.c...

[User and agent work on config validation]
[Status line changes to: 📬 1]

User: /inbox show 1

Agent: Memory Document: "OpenAI Streaming API Research"

       Summary:
       - Uses Server-Sent Events (SSE) with 'data:' prefix
       - Streaming endpoint: POST /v1/chat/completions with stream=true
       - Returns delta chunks: {"choices":[{"delta":{"content":"text"}}]}
       - Special events: [DONE] signals completion

       Key Implementation Points:
       - Need SSE parser (line-based, handles 'data:' prefix)
       - Accumulate deltas to build full message
       - Our HTTP client in src/protocol.c already supports streaming

       [Full details - 847 tokens]

User: /inbox merge 1

Agent: Memory document merged. Now that I understand OpenAI streaming,
       I can integrate it with our HTTP infrastructure. Proceed?
```

### Example 2: Background Task Needs Clarification

Background task encounters ambiguity and pauses for input.

```
User: /background Research how libulfius handles request body parsing for large payloads

[Status line: BG: 1 running]
[Status line changes to: ⏸️ 1 waiting]

User: /inbox show 1

Agent: Memory Document: "Request Body Parsing Research" [INCOMPLETE]

       Progress so far:
       - Found libulfius supports both streaming and buffered parsing

       QUESTION: Which parsing approach should I focus on?

       Options:
       1. Buffered parsing (simpler, good for typical use)
       2. Streaming parsing (complex, needed for large contexts)
       3. Both (comprehensive but more research time)

User: /inbox respond 1 3

[Status line: BG: 1 running]
[Later, status line changes to: 📬 1]

User: /inbox show 1

Agent: Memory Document: "Request Body Parsing Research"

       Summary:
       Libulfius supports both buffered and streaming request body parsing.

       Buffered: Simple, automatic, works for typical messages
       Streaming: Complex, callback-based, handles huge payloads

       Recommendation: Start with buffered, add streaming later if needed.

       [Full details with code examples - 1,247 tokens]
```

### Example 3: Multiple Parallel Tasks

User spawns several research tasks while implementing a feature.

```
User: /background Research how to safely execute shell commands in C
User: /background Research how other coding agents handle tool execution
User: /background Analyze our current HTTP patterns for reuse in tool execution

[Status line: BG: 3 running]

User: While those run, let's start sketching the tool execution interface

Agent: Looking at what we'll need for ik_tool_execute()...

[User and agent work on interface design]
[Status line updates: BG: 2 running, 📬 1]
[Status line updates: BG: 1 running, 📬 2]
[Status line updates: 📬 3]

User: /inbox list

Agent: Inbox:
       1. ✅ Safe Shell Execution Research (8 minutes ago)
       2. ✅ Tool Execution Patterns Research (3 minutes ago)
       3. ✅ HTTP Pattern Reuse Analysis (just now)

User: /inbox merge 1 2 3

Agent: All three memory documents merged.

       From Safe Shell Execution: Use fork/exec, not system()
       From Tool Execution Patterns: JSON schema, streaming output, clear states
       From HTTP Pattern Reuse: Can reuse callback patterns

       I can now implement ik_tool_execute() with these patterns. Proceed?
```

**Pattern matching alternative:**
```
# If you alias with a common prefix
/memory alias 1 tools/shell-execution
/memory alias 2 tools/execution-patterns
/memory alias 3 tools/http-reuse

# Merge all at once
/inbox merge tools/*

# Or reference in prompt
User: Implement tool execution using #mem-tools/*
```

### Example 4: Background Refactoring (Non-Research Task)

Background agents can do any task, not just research.

```
User: /background Refactor all error handling in src/openai/ to use ik_result_t consistently

[Status line: BG: 1 running]

User: Now let's implement the streaming endpoint

Agent: Looking at the streaming requirements...

[User and agent work on streaming]
[Status line changes to: 📬 1]

User: /inbox show 1

Agent: Memory Document: "OpenAI Error Handling Refactor"

       Changes Made:
       - 8 files refactored: client.c, stream.c, response.c, etc.
       - 26 functions converted to ik_result_t pattern
       - All callers updated, tests passing

       Test Results:
       - make check: PASS (all 47 tests)
       - make check-dynamic: PASS (no leaks)
       - make coverage: 100.0% maintained

       Files ready to review: [8 modified files with diffs]

User: /inbox merge 1

Agent: The refactoring looks good—all tests pass. Should I commit these changes?

User: Yes, commit them.
```

### Example 5: Persistence Across Sessions

The real power: memory documents survive session clears.

```
User: /background Research error handling patterns in libulfius
User: /background Analyze how talloc is used throughout our codebase
User: /background Document the protocol message format

[Status line: BG: 3 running]

User: Now let's work on the config module

[Long conversation, gets messy]

User: This is getting messy. /clear

[New clean session starts, status line shows: 📬 3]

User: Let's implement the HTTP streaming endpoint

Agent: I'll need to understand how libulfius handles streaming...

User: Hold on. /inbox list

Agent: Inbox:
       1. ✅ Error Handling Patterns in libulfius (12 minutes ago)
       2. ✅ Talloc Usage Analysis (8 minutes ago)
       3. ✅ Protocol Message Format Documentation (3 minutes ago)

User: /inbox merge 1

Agent: Memory document merged. Now I understand libulfius error handling.
       For the streaming endpoint, I can use ulfius_set_stream_response()...
```

**The power**: Spawn expensive tasks "fire and forget", then pull results into a clean context later. No token waste on task transcripts.

## Command Reference

### Background Tasks
```bash
/background <task description>          # Spawn a background task
```

### Inbox (Active Tasks)
```bash
/inbox list                             # Show all tasks (running, waiting, ready)
/inbox show <id>                        # Preview a memory document
/inbox merge <id>                       # Pull memory document into context
/inbox respond <id> <option>            # Answer a paused task's question
/inbox clear <id>                       # Remove from inbox (doc persists in DB)
/inbox clear-all                        # Clear all completed tasks
```

### Memory Documents (Permanent Store)
```bash
/memory list                            # List all memory documents
/memory list <pattern>                  # Filter by glob pattern
/memory show <id|alias|pattern>         # Show a memory document
/memory merge <id|alias|pattern>        # Pull into context
/memory delete <id|alias|pattern>       # Permanently delete
/memory search <query>                  # Search by content
/memory alias <id> <alias>              # Create an alias
/memory unalias <alias>                 # Remove an alias
```

**Pattern matching examples:**
```bash
/memory list "openai/*"                 # All OpenAI-related docs
/memory merge "openai/*"                # Merge all matching
/memory delete "draft/*"                # Delete all drafts
/memory show "*-errors"                 # All error-related docs
```

### Status Line Indicators
```bash
[BG: N running]                         # N background tasks executing
[📬 N]                                  # N results ready to review
[⏸️ N waiting]                          # N tasks paused for input
[BG: 2 running, 📬 1]                   # Combined status
```

## Advanced Features

### Memory Document References

Reference memory documents in prompts without permanently merging them into context:

```bash
#mem-<id>                               # By numeric ID
#mem-<alias>                            # By alias
#mem-<pattern>                          # By glob pattern (multiple docs)
```

**Examples:**
```
User: Implement streaming based on #mem-124
User: Refactor using patterns from #mem-openai/errors
User: Implement using all OpenAI research: #mem-openai/*
```

**How it works:**
- Reference expands to full content when sent to LLM
- Only the reference (`#mem-openai/streaming`) is stored in history
- Content is in context for that single request only
- After response, content is gone from context

**Compared to `/inbox merge`:**
- `/inbox merge 124`: Permanently in context until `/clear` or `/rewind`
- `#mem-124`: Temporarily in context for one request only

### Organizing with Aliases

Use `/` in aliases for conceptual organization:

```bash
/memory alias 124 openai/streaming
/memory alias 125 openai/errors
/memory alias 126 openai/rate-limits
/memory alias 127 libulfius/callbacks
/memory alias 128 ikigai/talloc-patterns
/memory alias 129 testing/coverage-strategy
```

**Then operate on categories:**
```bash
/memory list "openai/*"                 # Shows: 124, 125, 126
/memory merge "openai/*"                # Merge all OpenAI docs
/memory list "*/errors"                 # All error-related docs

# Reference entire categories
User: #mem-openai/* Implement complete OpenAI integration
(Expands all matching docs for this request)
```

**Typical workflow:**
1. Spawn task: `/background Research OpenAI streaming`
2. Task completes with ID 124
3. Use by ID initially: `#mem-124`
4. If reused often, create alias: `/memory alias 124 openai/streaming`
5. Build up related docs: `openai/errors`, `openai/rate-limits`, etc.
6. Reference all at once: `#mem-openai/*`

### Pattern Matching Philosophy

**Design principles:**
- Use well-understood glob patterns (`*`, `?`, `[...]`)
- No complex query languages or SQL WHERE clauses
- Natural fit with alias organization (`openai/*`, `testing/*`)
- Covers 95% of real-world batch operation needs

**Common workflows:**
```bash
/memory merge "openai/*"                # Pull in module research
/memory delete "draft/*"                # Clean up temporary docs
/memory list "*-patterns"               # Find best-practice docs
#mem-testing/*                          # Reference testing docs
```

**Future extensions** (if needed):
- Multiple patterns: `/memory merge "openai/*" "http/*"`
- Exclusions: `/memory merge "openai/*" --exclude "*-draft"`
- Tag-based selection: `/memory merge --tags streaming,http`

But start simple: glob patterns cover the essential cases.

## Design Principles

### Permanence & Session Independence
- Memory documents are database artifacts, not session-scoped
- Background tasks survive `/rewind`, `/clear`, and session restarts
- Build a permanent knowledge base about your codebase and libraries
- Pull in exactly what you need, when you need it

### Explicit Control
- User explicitly spawns tasks (`/background`)
- User explicitly pulls in results (`/inbox merge` or `#mem-*`)
- No automatic context pollution
- No surprises

### Context Efficiency
- Only merged or referenced documents enter foreground context
- Background task transcripts never pollute main conversation
- Temporary references (`#mem-*`) don't permanently grow context
- Clean slate workflows: `/clear` often, pull in just what's needed

### Pause-and-Ask
- Background tasks can request clarification when needed
- Clear indication in status line (⏸️)
- Well-framed questions with context and options
- No timeouts by default—tasks wait for user response

### Pattern-Based Composability
- Simple glob patterns for batch operations
- No complex query languages required
- Natural organization with hierarchical aliases
- Power user tool without over-engineering

## Use Cases

1. **Parallel work**: Spawn tasks at start of day, use results hours later
2. **Knowledge accumulation**: Build permanent understanding of codebase/libraries/patterns
3. **Clean slate workflows**: `/clear` frequently, pull in just what's needed
4. **Async execution**: Continue working while background tasks run
5. **Context hygiene**: Never pollute main context with completed work transcripts
6. **Modular research**: Build up category-organized docs, reference entire topics at once

## Implementation Notes

**Inbox vs Memory Store:**
- **Inbox**: Active view of running/recent tasks (ephemeral, per-session)
- **Memory Store**: Permanent database of all memory documents (persistent, global)
- Clearing inbox doesn't delete memory documents
- Memory documents accumulate as a permanent knowledge base

**Task Lifecycle:**
1. User spawns with `/background` → task starts running
2. Task completes → appears in inbox as ready (📬)
3. User views with `/inbox show` → preview the result
4. User merges with `/inbox merge` → enters context
5. User clears with `/inbox clear` → removed from inbox view
6. Memory document persists in database indefinitely

**Memory Document Retention:**
- Default: Keep forever (build knowledge base)
- User can `/memory delete` to remove permanently
- Future: Auto-cleanup policies (age, size limits, LRU)
- Future: Export/import for sharing between projects
