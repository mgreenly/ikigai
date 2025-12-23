# Multi-Provider Abstraction Design

**Release:** rel-07
**Status:** Design Phase
**Last Updated:** 2025-12-22

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL HTTP operations
MUST be non-blocking:

- Use curl_multi (NOT curl_easy)
- Expose fdset() for select() integration
- Expose perform() for incremental processing
- NEVER block the main thread

Reference: `src/openai/client_multi.c`

## Overview

This design implements multi-provider AI API support for ikigai, enabling seamless integration with Anthropic (Claude), OpenAI (GPT/o-series), and Google (Gemini) through a unified internal abstraction layer.

## Core Principles

1. **Async Everything** - All HTTP operations are non-blocking via curl_multi
2. **Lazy Everything** - No provider initialization or credential validation until first use
3. **Zero Pre-Configuration** - App starts with no credentials; errors surface when features are used
4. **Unified Abstraction** - All providers implement identical async vtable interface
5. **No Remnants** - Old OpenAI code deleted after new abstraction proven; clean end state
6. **Provider Parity** - OpenAI is just another provider, no special treatment

## Coexistence-Then-Removal Approach

This release REPLACES old OpenAI code, tests, and fixtures through a phased migration:

**Phase 1: Coexistence**
- Build new `src/providers/openai/` alongside existing `src/openai/`
- Both implementations exist temporarily
- Switch application to use new implementation
- Verify new implementation works

**Phase 2: Removal**
- Delete old `src/openai/` directory and all implementation files
- Delete old unit/integration tests for OpenAI client
- Delete old fixtures in non-VCR format (`tests/fixtures/openai/`, etc.)

**End State:**
- **Code:** New provider abstraction in `src/providers/` (including `src/providers/openai/`)
- **Tests:** VCR-based tests only
- **Fixtures:** VCR JSONL cassettes in `tests/fixtures/vcr/`

**Key principle:** Temporary dual code paths during migration, but NO remnants in the end state.

## Design Documents

### Architecture & Structure

- **[01-architecture/overview.md](01-architecture/overview.md)** - System architecture, vtable pattern, directory structure, module organization
- **[01-architecture/provider-interface.md](01-architecture/provider-interface.md)** - Vtable interface specification, lifecycle, common utilities

### Data Formats

- **[02-data-formats/request-response.md](02-data-formats/request-response.md)** - Internal superset format for requests and responses
- **[02-data-formats/streaming.md](02-data-formats/streaming.md)** - Normalized streaming event types and flow
- **[02-data-formats/error-handling.md](02-data-formats/error-handling.md)** - Error categories, mapping, retry strategies

### Provider Types & Transformations

- **[03-provider-types.md](03-provider-types.md)** - Request/response transformation pipeline per provider, unified thinking level mapping to provider-specific parameters, tool call handling

### Application Layer

- **[04-application/commands.md](04-application/commands.md)** - `/model` and `/fork` command behavior, provider inference, argument parsing
- **[04-application/configuration.md](04-application/configuration.md)** - config.json and credentials.json format, precedence rules
- **[04-application/database-schema.md](04-application/database-schema.md)** - Schema changes for provider/model/thinking storage

### Testing

- **[05-testing/strategy.md](05-testing/strategy.md)** - Mock HTTP pattern, fixture validation, test organization
- **[05-testing/vcr-cassettes.md](05-testing/vcr-cassettes.md)** - VCR fixture format, record/playback modes, credential redaction

## Implementation Order

Based on README.md decisions:

1. **Abstraction + OpenAI** - Build provider interface, refactor existing OpenAI to implement it
2. **Anthropic** - Validate abstraction handles different API shape
3. **Google** - Third provider confirms abstraction is solid

## Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| HTTP layer | curl_multi (async) | Required for select()-based event loop |
| Abstraction pattern | Async Vtable | fdset/perform/info_read pattern for event loop integration |
| Directory structure | `src/providers/{name}/` | Separate modules per provider |
| OpenAI refactor | Unified abstraction | No dual code paths, OpenAI uses same abstraction as others |
| Shared utilities | http_multi + SSE | curl_multi wrapper shared, semantic code per-provider |
| Initialization | Lazy on first use | Don't require credentials for unused providers |
| Credential validation | On API call | Provider API is source of truth |
| Default provider | Initial agent only | Session state takes over after first use |
| Database migration | Truncate + new columns | Clean slate, developer dogfoods onboarding |
| Thinking storage | Normalized + provider_data | Common field for summaries, opaque field for signatures |
| Transformation | Single-step in adapter | Adapter owns internal → wire format conversion |
| Streaming normalization | In adapter (during perform) | Provider adapters emit normalized events via callbacks |
| Tool call IDs | Preserve provider IDs | Generate UUIDs only for Google (22-char base64url) |
| Error preservation | Enriched errors | Store category + provider details for debugging |
| Testing | Mock curl_multi layer | Test async behavior, validate with live tests |

## Migration Impact

**Existing Users (developer only):**
- Database truncated (agents, messages tables)
- Fresh start with new schema
- Dogfood the onboarding experience
- No model selected → error with instructions → set model → set credentials → works

**New Users:**
- Start app with zero configuration
- Guided errors at each step
- Only need credentials for providers they use

## References

- [rel-07/README.md](../README.md) - Requirements and high-level design
- [rel-07/findings/](../findings/) - Provider API research
- [project/](../../project/) - Architecture documentation
