# Multi-Provider Abstraction Design

**Release:** rel-07
**Status:** Design Phase
**Last Updated:** 2025-12-19

## Overview

This design implements multi-provider AI API support for ikigai, enabling seamless integration with Anthropic (Claude), OpenAI (GPT/o-series), and Google (Gemini) through a unified internal abstraction layer.

## Core Principles

1. **Lazy Everything** - No provider initialization or credential validation until first use
2. **Zero Pre-Configuration** - App starts with no credentials; errors surface when features are used
3. **Unified Abstraction** - All providers implement identical vtable interface
4. **No Remnants** - Existing OpenAI code refactored into new abstraction; no dual code paths
5. **Provider Parity** - OpenAI is just another provider, no special treatment

## Design Documents

### Architecture & Structure

- **[architecture.md](architecture.md)** - System architecture, vtable pattern, directory structure, module organization
- **[provider-interface.md](provider-interface.md)** - Vtable interface specification, lifecycle, common utilities

### Data Formats

- **[request-response-format.md](request-response-format.md)** - Internal superset format for requests and responses
- **[streaming.md](streaming.md)** - Normalized streaming event types and flow
- **[error-handling.md](error-handling.md)** - Error categories, mapping, retry strategies

### Transformation & Flow

- **[transformation.md](transformation.md)** - Request/response transformation pipeline per provider
- **[thinking-abstraction.md](thinking-abstraction.md)** - Unified thinking level mapping to provider-specific parameters

### Configuration & Storage

- **[configuration.md](configuration.md)** - config.json and credentials.json format, precedence rules
- **[database-schema.md](database-schema.md)** - Schema changes for provider/model/thinking storage

### Testing

- **[testing-strategy.md](testing-strategy.md)** - Mock HTTP pattern, fixture validation, test organization

## Implementation Order

Based on README.md decisions:

1. **Abstraction + OpenAI** - Build provider interface, refactor existing OpenAI to implement it
2. **Anthropic** - Validate abstraction handles different API shape
3. **Google** - Third provider confirms abstraction is solid

## Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Abstraction pattern | Vtable | Consistent with existing layer system, clean separation |
| Directory structure | `src/providers/{name}/` | Separate modules per provider |
| OpenAI refactor | Unified abstraction | No dual code paths, OpenAI uses same abstraction as others |
| Shared utilities | HTTP + SSE | Mechanical protocol-level code shared, semantic code per-provider |
| Initialization | Lazy on first use | Don't require credentials for unused providers |
| Credential validation | On API call | Provider API is source of truth |
| Default provider | Initial agent only | Session state takes over after first use |
| Database migration | Truncate + new columns | Clean slate, developer dogfoods onboarding |
| Thinking storage | Normalized + provider_data | Common field for summaries, opaque field for signatures |
| Transformation | Single-step in adapter | Adapter owns internal → wire format conversion |
| Streaming normalization | In adapter | Provider adapters emit normalized events |
| Tool call IDs | Preserve provider IDs | Generate UUIDs only for Google (22-char base64url) |
| Error preservation | Enriched errors | Store category + provider details for debugging |
| Testing | Mock HTTP layer | Consistent with existing pattern, validate with live tests |

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
