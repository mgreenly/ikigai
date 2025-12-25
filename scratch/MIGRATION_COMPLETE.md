# Legacy OpenAI Cleanup - Migration Complete

**Date:** 2025-12-25
**Status:** SUCCESS

## Summary

Successfully migrated from legacy OpenAI-specific conversation storage to
provider-agnostic message format. All three providers (Anthropic, OpenAI, Google)
working correctly with unified message interface.

## Changes

### Deletions
- src/openai/ directory: Legacy OpenAI-specific code
- src/providers/openai/shim.c, shim.h: Translation layer no longer needed
- Legacy unit tests referencing removed APIs
- Legacy integration tests using removed APIs (10 tests)
- Total: Significant code reduction

### Additions
- src/message.c/h: Message creation and DB conversion
- src/providers/openai/serialize.c/h: Shared OpenAI JSON serialization
- Updated test common file for new architecture

### Migrations
- REPL code: Uses new ik_message_create_* functions
- Provider code: Reads from agent->messages
- Restore code: Uses ik_message_from_db_msg()
- All tests: Updated to new API

### Bug Fixes
- Fixed talloc/yyjson context confusion in serialize.c
- Fixed talloc/yyjson context confusion in request_responses.c
- Removed orphaned legacy function declaration from wrapper_internal.h

## Verification

All success criteria met:
- src/openai/ directory deleted
- Shim layer deleted
- Agent struct uses ik_message_t **messages
- No legacy conversation calls in src/
- No legacy includes in src/
- Clean build succeeds
- All tests pass (make check)

## Architecture

New clean separation:
- REPL -> creates ik_message_t -> adds to agent->messages
- Providers -> read agent->messages -> build requests -> serialize to JSON
- Database -> stores in ik_msg_t -> restore converts to ik_message_t
- Each provider manages its own http_multi internally

No OpenAI coupling outside src/providers/openai/.

## Next Steps

Ready for commit and release.
