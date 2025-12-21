# Task: Create Shared Provider Utilities

**Layer:** 1
**Depends on:** provider-types.md

## Pre-Read

- **Skills:** `/load memory`, `/load errors`
- **Source:** `src/openai/http_handler.c`, `src/openai/sse_parser.c` (refactor from these)
- **Plan:** `scratch/plan/architecture.md`, `scratch/plan/configuration.md`

## Objective

Create shared HTTP client and SSE parser in `src/providers/common/` that all providers will use.

## Deliverables

1. Create `src/providers/common/http_client.h` and `http_client.c`
   - `ik_http_client_create()` - Create client for base URL
   - `ik_http_post()` - Non-streaming POST
   - `ik_http_post_stream()` - Streaming POST with SSE callback
   - Refactor from existing `src/openai/http_handler.c`

2. Create `src/providers/common/sse_parser.h` and `sse_parser.c`
   - `ik_sse_parser_create()` - Create parser
   - `ik_sse_parse_chunk()` - Parse SSE chunk
   - Move from existing `src/openai/sse_parser.c`

3. Create `src/providers/provider_common.c`
   - `ik_credentials_load()` - Load API key from env or credentials.json
   - `ik_provider_env_var()` - Get env var name for provider
   - `ik_provider_create()` - Factory dispatch to provider factories

## Reference

- `scratch/plan/architecture.md` - Directory structure
- `scratch/plan/configuration.md` - Credential loading

## Verification

- HTTP client can make POST requests
- SSE parser emits events correctly
- Credentials load from env var and file

## Postconditions

- [ ] HTTP client makes POST requests
- [ ] SSE parser emits events
- [ ] Credentials load from env and file
