# Release: Fix Anthropic Thinking Block Signatures

## Summary

Fix HTTP 400 errors when continuing Anthropic conversations after tool calls with thinking enabled.

## Problem

Anthropic's API returns a cryptographic `signature` with each thinking block. When sending thinking blocks back in subsequent requests (e.g., after a tool call), this signature **must** be included. Without it, Anthropic returns HTTP 400.

The current implementation:
1. Parses thinking text but ignores the signature
2. Stores thinking blocks without signature field
3. Serializes thinking blocks back to Anthropic without signature
4. Anthropic rejects the malformed request

## Solution

1. Add `signature` field to thinking content blocks
2. Capture `signature_delta` events during streaming
3. Include thinking blocks (with signatures) in response builder
4. Serialize signatures when sending requests back to Anthropic
5. Handle `redacted_thinking` blocks (encrypted, opaque - must pass through unmodified)

## Scope

- `src/providers/provider.h` - Add signature field, redacted_thinking type
- `src/providers/anthropic/streaming*.c` - Capture signatures during streaming
- `src/providers/anthropic/request_serialize.c` - Serialize signatures
- `src/providers/request.c` - Update content block builders
- `src/providers/request_tools.c` - Update deep copy
- `src/agent_messages.c` - Update clone functions
- Tests for round-trip verification

## References

- `fix-anthropic-thinking-blocks.md` - Diagnosis and API documentation
- [Anthropic Extended Thinking Docs](https://docs.anthropic.com/en/docs/build-with-claude/extended-thinking)
