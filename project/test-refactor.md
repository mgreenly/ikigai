# Test Refactoring Strategy

## Problem

Achieving 100% test coverage costs 5-10x the development time of the feature itself. This tax is unsustainable.

## Constraint

100% coverage is mandatory. In an AI-developed codebase, tests are the verification mechanism. The AI proposes, tests validate. Without comprehensive coverage, unverified code paths allow bugs to hide undetected.

## Root Cause

The problem isn't the coverage requirement - it's that the code is expensive to test. Fixing tests treats the symptom. Fixing architecture addresses the root cause.

## Module Definition

Before assessing modules, define what a module is and what's expected of each.

### What Is a Module?

A module is a directory containing:
- **One .h file** - The public interface (the contract)
- **One or more .c files** - Implementation (split to stay under 16KB file limit)

The directory is the module boundary. The .h file defines what the module promises. The .c files are internal implementation details.

### Module Structure

**Module = directory. Contract = all .h files in the directory.**

Each .c file has a corresponding .h file. No aggregator header needed.

```
src/<area>/<module>/
├── foo.h           # Part of the contract
├── foo.c           # Implementation
├── bar.h           # Part of the contract
├── bar.c           # Implementation
└── internal.c      # Internal-only (no .h = not part of contract)
```

Example:
```
src/providers/common/
├── http_multi.h    # Part of the contract
├── http_multi.c
├── sse_parser.h    # Part of the contract
└── sse_parser.c
```

**Include path:** `#include "providers/common/http_multi.h"` (with `-I src/` compile flag)

**Convention:**
- Has `.h` file = public, part of the contract
- No `.h` file = internal implementation detail

### Test Structure

Tests mirror the source structure with 1:1 file mapping.

```
tests/unit/<area>/<module>/
├── foo_test.c             # Tests for foo.h
├── foo_2_test.c           # Additional tests if foo_test.c exceeds 16KB
├── bar_test.c             # Tests for bar.h
└── ...
```

**Unit test convention:**
- `<name>_1_test.c` maps to `<name>.c` (always numbered, even if only one file)
- Additional test files: `<name>_2_test.c`, `<name>_3_test.c`, etc.
- No renaming needed when splitting - just add the next number
- Each test file tests one source file's contract

**Integration test convention:**
- Named after what they're testing (feature, behavior, integration point)
- No 1:1 file mapping - tests span multiple modules
- Location: `tests/integration/`

### Current State

The codebase does not consistently follow this structure. Some code may accidentally align, but module boundaries are not formally defined.

### Migration

Restructuring into modules is part of this effort. For each module:

1. **Identify** - What code logically belongs together?
2. **Extract** - Move files into a module directory
3. **Define interface** - Create/refine the .h file as the public API
4. **Internalize** - Move non-public functions out of the header
5. **Update tests** - Reorganize tests to match module structure

This happens incrementally. One module at a time. No big-bang restructure.

### Module Documentation Requirements

Each module must have:

1. **Interface definition** - Public functions, their signatures, and behavior
 - What inputs are valid
 - What outputs are guaranteed
 - What errors can occur
 - What side effects exist

2. **Trust boundaries** - External dependencies this module uses
 - Which libraries/modules it calls
 - What assumptions it makes about their behavior
 - What it does NOT verify (trusts the dependency)

3. **Invariants** - Conditions that must always hold
 - Preconditions (caller's responsibility)
 - Postconditions (module's guarantees)
 - State invariants (always true between calls)

4. **Test strategy** - How this module is verified
 - What's tested at the contract level
 - What's tested at the unit level
 - What's excluded from coverage and why

### Module Expectations

- Public API is stable and documented
- Internal implementation can change without breaking tests
- Dependencies are explicit (no hidden globals)
- Error handling is defined at the interface level

## Proposed Modules

First pass at module breakdown. ~289 files total in src/. Organized into a two-level hierarchy.

```
src/
├── vendor/                     # EXTERNAL - excluded from coverage
│   ├── fzy/
│   └── yyjson/
├── foundation/
│   ├── error/                  # Error types, panic handling
│   ├── util/                   # Arrays, formatting, file utils
│   └── wrapper/                # Library abstractions (talloc, curl, libpq, etc.)
├── infra/
│   ├── config/                 # Configuration, credentials, paths
│   ├── terminal/               # Terminal context, raw mode
│   └── logger/                 # Logging, debug output
├── ui/
│   ├── input/                  # Input parsing, text buffer, cursor
│   ├── ansi/                   # ANSI escape sequences
│   ├── scrollback/             # Scrollback buffer, layout
│   ├── layer/                  # Composable UI layers
│   └── render/                 # Render context, viewport, completion
├── db/                         # PostgreSQL persistence (already organized)
│   ├── connection
│   ├── migration
│   ├── session
│   ├── agent
│   ├── message
│   └── ...
├── providers/
│   ├── common/                 # HTTP, SSE parsing (already exists)
│   ├── core/                   # Provider abstraction, vtable, factory
│   ├── anthropic/              # Anthropic API (already exists)
│   ├── google/                 # Google Gemini API (already exists)
│   └── openai/                 # OpenAI API (already exists)
├── domain/
│   ├── msg/                    # Message format
│   ├── conversation/           # History, marks
│   ├── tool/                   # Tool registry, discovery, execution
│   └── mail/                   # Inter-agent messaging
├── core/
│   ├── agent/                  # Agent context, state
│   ├── repl/                   # REPL loop, actions, events
│   └── commands/               # Slash command dispatch
└── client.c                    # Entrypoint
```

**8 top-level areas, 20 submodules**

---

### vendor/ (EXTERNAL - exclude from coverage)

Third-party vendored code. Do not test.

**Current location:** `src/vendor/`
**Contents:** yyjson, fzy
**Status:** Already isolated. Exclude from coverage entirely.

---

### foundation/ (3 submodules)

Low-level building blocks everything depends on.

#### foundation/error
Error types and panic handling.

**Files:** error.h/.c, panic.h/.c

#### foundation/util
Generic utilities with no domain knowledge.

**Files:** array, byte_array, line_array, format, pp_helpers, uuid, file_utils, tmp_ctx

#### foundation/wrapper
Abstraction layer over external libraries for testability.

**Files:** wrapper_talloc, wrapper_stdlib, wrapper_posix, wrapper_pthread, wrapper_json, wrapper_curl, wrapper_postgres, json_allocator
**Trust boundaries:** talloc, libc, pthreads, yyjson, curl, libpq

---

### infra/ (3 submodules)

Infrastructure and dependency injection.

#### infra/config
Configuration loading and credential management.

**Files:** config, credentials, paths

#### infra/terminal
Terminal context and raw mode management.

**Files:** terminal

#### infra/logger
Structured logging and debug output.

**Files:** logger, debug_pipe, debug_log

*(Note: shared.h/.c is the DI root - lives at src/ level or becomes infra/shared)*

---

### ui/ (5 submodules)

Terminal UI: input handling and rendering.

#### ui/input
Terminal input parsing and text buffer.

**Files:** input, input_escape, input_xkb, input_buffer/* (core, cursor, layout, multiline)
**Note:** Merges current input*.c and input_buffer/ directory

#### ui/ansi
ANSI escape sequence generation.

**Files:** ansi

#### ui/scrollback
Scrollback buffer with layout pre-computation.

**Files:** scrollback, scrollback_layout, scrollback_render, scrollback_utils

#### ui/layer
Composable UI layer abstraction.

**Files:** layer, layer_wrappers, layer_scrollback, layer_input, layer_separator, layer_spinner, layer_completion

#### ui/render
Render context, viewport, and display coordination.

**Files:** render, render_cursor, scroll_detector, event_render, completion, fzy_wrapper

---

### db/ (1 module)

PostgreSQL persistence layer.

**Current location:** `src/db/`
**Files:** connection, migration, session, agent, agent_row, agent_replay, agent_zero, message, replay, pg_result, mail
**Status:** Already a directory. Well-organized.
**Trust boundaries:** libpq (PostgreSQL)

---

### providers/ (4 submodules)

LLM provider integrations. Already well-structured.

#### providers/common
Shared HTTP and SSE parsing.

**Files:** error_utils, http_multi, sse_parser
**Trust boundaries:** curl

#### providers/core
Provider abstraction (vtable, factory, request/response).

**Files:** provider, provider_types, provider_vtable, provider_stream, factory, stubs, request, request_tools, response
**Note:** Currently in providers/ root, would become providers/core/

#### providers/anthropic
Anthropic API implementation.

**Status:** Already a directory.
**Trust boundaries:** Anthropic API

#### providers/google
Google Gemini API implementation.

**Status:** Already a directory.
**Trust boundaries:** Google API

#### providers/openai
OpenAI API implementation.

**Status:** Already a directory.
**Trust boundaries:** OpenAI API

---

### domain/ (4 submodules)

Domain concepts: messages, conversation, tools.

#### domain/msg
Canonical message format (database-compatible).

**Files:** msg, message

#### domain/conversation
History and checkpoints.

**Files:** history, history_io, marks

#### domain/tool
Tool discovery, registration, and execution.

**Files:** tool, tool_call, tool_registry, tool_discovery, tool_external, tool_wrapper, tool_arg_parser, tools/bash/main

#### domain/mail
Inter-agent messaging.

**Files:** mail/msg, commands_mail, commands_mail_helpers

---

### core/ (3 submodules)

Application core: agent management, REPL, commands.

#### core/agent
Agent context and state management.

**Files:** agent, agent_state, agent_messages, agent_provider

#### core/repl
REPL event loop, actions, and coordination.

**Files:** repl, repl_init, repl_callbacks, repl_event_handlers, repl_tool, repl_agent_mgmt, repl_navigation, repl_viewport, repl/agent_restore, repl_actions, repl_actions_*

#### core/commands
Slash command dispatch and implementations.

**Files:** commands, commands_basic, commands_fork, commands_fork_args, commands_fork_helpers, commands_kill, commands_mark, commands_model, commands_tool, commands_agent_list

---

### client.c (ENTRYPOINT)

Main program entry. Single file, not a module.

---

**Summary: 8 areas, ~20 submodules**

| Area | Submodules | Status |
|------|------------|--------|
| vendor | - | Already isolated, exclude |
| foundation | error, util, wrapper | Needs extraction |
| infra | config, terminal, logger | Needs extraction |
| ui | input, ansi, scrollback, layer, render | Partial (input_buffer exists) |
| db | - | Already a directory |
| providers | common, core, anthropic, google, openai | Mostly done |
| domain | msg, conversation, tool, mail | Needs extraction |
| core | agent, repl, commands | Needs extraction |

## Strategy

Module-by-module contract definition and test strategy assessment. Incremental, prioritized by pain.

### Per-Module Process

For each module, document:

1. **Contract** - Public API, behaviors, error conditions, invariants
2. **Trust boundaries** - External dependencies this module relies on (yyjson, PostgreSQL, talloc, libc). Trusted dependencies don't need their internals tested.
3. **Coverage policy** - What's excluded and why:
 - Trusted external library code
 - Defensive branches that can't be triggered (PANIC after impossible states)
 - Generated code
4. **Test strategy** - What kind of tests this module needs:
 - Contract tests (test the interface, not internals)
 - Unit tests (test specific functions)
 - Integration tests (test interaction with dependencies)
5. **Testability assessment** - Is the current code structure testable?
 - If yes: Apply new test strategy
 - If no: Assess refactoring cost/benefit
6. **Refactoring decision** - Refactor now, defer, or accept current structure

### Possible Outcomes Per Module

- **Best case**: Refactor + new test strategy = dramatically cheaper coverage
- **Middle case**: No refactor + new test strategy = reduced test burden
- **Minimum case**: No change = documented decisions, explicit policy

### Prioritization

Start with modules that have:
- Highest test burden (most iterations/escalations in coverage harness)
- Frequent changes (high ROI on reduced friction)

## Testability Patterns

When refactoring is warranted, target these patterns:

- **Pure core, thin shell** - Business logic in pure functions. I/O at edges only.
- **Dependency injection** - Pass dependencies in, don't reach for globals.
- **Small functions** - Single responsibility, fewer branches, fewer test cases.
- **Explicit state** - State passed through parameters, not hidden in globals.

## Coverage Exclusion Principle

"Trust boundaries define test scope. External libraries are trusted. Test the integration, not the dependency."

When calling yyjson, test:
- Your code calls it correctly
- Your code handles errors from it
- Your code processes its output correctly

Don't test:
- That yyjson parses valid JSON
- That yyjson rejects invalid JSON
- yyjson's internal edge cases

## Status

Planning phase. Module breakdown complete: 8 areas, ~20 submodules. No modules assessed yet.

## Next Steps

1. ~~Define what a module is for this project~~ DONE
2. ~~Identify, define, and count all modules in the codebase~~ DRAFT (31 modules, needs refinement)
3. Review and refine module boundaries
4. Select first module (high pain, frequent changes)
5. Document contract and trust boundaries for that module
6. Assess current test burden
7. Decide: refactor, new test strategy, or defer
