# Task: Create Shared SSE Parser

**Model:** sonnet/thinking
**Depends on:** provider-types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load memory` - Talloc patterns
- `/load errors` - Result types

**Source:**
- `src/openai/sse_parser.h` - Reference for SSE parsing
- `src/openai/sse_parser.c` - Reference implementation

## Objective

Create `src/providers/common/sse_parser.h` and `sse_parser.c` - a shared Server-Sent Events parser that all providers will use for streaming responses. The parser accumulates incoming bytes and extracts complete SSE events delimited by blank lines.

## SSE Format Reference

Server-Sent Events format:
```
event: message
data: {"type":"content_block_delta",...}

event: message
data: {"type":"content_block_stop"}

data: [DONE]

```

- Events separated by blank lines (`\n\n`)
- Lines starting with `data: ` contain payload
- Lines starting with `event: ` specify event type (optional)
- `[DONE]` signals end of stream
- Multiple `data:` lines concatenated with newlines

## Interface

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_sse_parser_t` | buffer, len, cap | SSE parser with accumulation buffer |
| `ik_sse_event_t` | event (nullable string), data (nullable string) | Parsed SSE event |

### Functions to Implement

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_sse_parser_create` | `ik_sse_parser_t *(void *parent)` | Create new SSE parser, panics on OOM |
| `ik_sse_parser_feed` | `void (ik_sse_parser_t *parser, const char *data, size_t len)` | Feed data to parser, panics on OOM |
| `ik_sse_parser_next` | `ik_sse_event_t *(ik_sse_parser_t *parser, TALLOC_CTX *ctx)` | Extract next complete event, NULL if none |
| `ik_sse_event_is_done` | `bool (const ik_sse_event_t *event)` | Check if event is [DONE] marker |
| `ik_sse_parser_reset` | `void (ik_sse_parser_t *parser)` | Reset parser state, clear buffer |

## Behaviors

### Parser Creation

- Allocate parser with initial buffer capacity (4096 bytes)
- Buffer allocated via talloc on parser
- Initialize length to 0
- Panics on out-of-memory

### Feeding Data

- Append incoming bytes to internal buffer
- Grow buffer exponentially if needed (double capacity)
- Data does not need to be null-terminated
- No parsing happens during feed
- Panics on out-of-memory

### Extracting Events

- Search for `\n\n` delimiter in buffer
- If found, extract event text before delimiter
- Remove event and delimiter from buffer (shift remaining data)
- Parse event text line-by-line:
  - Lines starting with `event: ` set event type
  - Lines starting with `data: ` set/append data payload
  - Multiple data lines concatenated with newlines
  - Lines with `data:` (no space) treated as empty data
- Return parsed event allocated on provided context
- Return NULL if no complete event available

### Done Detection

- Check if event data equals `[DONE]` string
- Case-sensitive exact match
- Return true if match, false otherwise

### Reset

- Clear internal buffer
- Set length to 0
- Parser can be reused after reset

### Memory Management

- Parser owns internal buffer via talloc
- Events allocated on caller-provided context
- Buffer grows automatically, never shrinks
- All allocations cleaned up when parser freed

## Callback Integration Pattern

**IMPORTANT:** This is a **pull-based API**. The SSE parser does not invoke callbacks. Instead, the caller must loop and pull events using `ik_sse_parser_next()`.

### Integration with Streaming Providers

Streaming provider implementations (Anthropic, OpenAI, etc.) should follow this pattern:

1. **cURL write callback** receives chunks of data from the network
2. **Feed data** to the SSE parser using `ik_sse_parser_feed()`
3. **Pull events** in a loop by calling `ik_sse_parser_next()` until it returns NULL
4. **Process each event** by invoking your provider-specific event processor function

### Example Integration

```c
// Provider-specific streaming context
typedef struct {
    ik_sse_parser_t *parser;
    // ... other provider fields ...
} stream_context_t;

// cURL write callback - feeds data and pulls events
static size_t curl_write_callback(char *data, size_t size, size_t nmemb, void *userdata) {
    stream_context_t *ctx = userdata;
    size_t total = size * nmemb;

    // Feed incoming data to SSE parser
    ik_sse_parser_feed(ctx->parser, data, total);

    // Pull and process all available events
    ik_sse_event_t *event;
    while ((event = ik_sse_parser_next(ctx->parser, ctx)) != NULL) {
        // Check for stream termination
        if (ik_sse_event_is_done(event)) {
            talloc_free(event);
            break;
        }

        // Invoke provider-specific event processor
        // e.g., ik_anthropic_stream_process_event(ctx, event);
        process_sse_event(ctx, event);

        talloc_free(event);
    }

    return total;
}
```

### Key Points

- The **parser is passive** - it accumulates data and provides events on demand
- The **caller controls the loop** - streaming implementations must loop over `ik_sse_parser_next()`
- **No callbacks in the parser** - event processing happens in the caller's code
- **Memory ownership** - caller receives events on their provided context and must free them

## Directory Structure

```
src/providers/common/
├── sse_parser.h
└── sse_parser.c

tests/unit/providers/common/
└── sse_parser_test.c
```

## Test Scenarios

Create `tests/unit/providers/common/sse_parser_test.c`:

- Parser creation: Successfully create parser
- Empty buffer: next() returns NULL when no data fed
- Single event: Feed and extract one complete event
- Event with type: Parse event type and data correctly
- Multiple events: Feed multiple, extract in order
- Partial feed: Feed event in chunks, extract when complete
- Done marker: Correctly identify [DONE] event
- Not done: Non-DONE events return false
- Reset: Clear buffer, parser still works after reset
- Multi-line data: Multiple data lines concatenated with newlines

## Postconditions

- [ ] `src/providers/common/sse_parser.h` exists with API
- [ ] `src/providers/common/sse_parser.c` implements SSE parsing
- [ ] Makefile updated with new source/header
- [ ] Parser handles partial data (streaming chunks)
- [ ] Parser detects [DONE] marker
- [ ] Buffer grows dynamically
- [ ] Compiles without warnings
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: sse-parser.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).