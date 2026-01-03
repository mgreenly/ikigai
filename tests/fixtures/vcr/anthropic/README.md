# Anthropic API VCR Fixtures

This directory contains recorded HTTP responses from the Anthropic API for testing purposes.

## VCR-Style Testing

These fixtures follow the VCR (Video Cassette Recorder) pattern:
1. **Record** - Capture real API responses during verification tests
2. **Verify** - Assert response structure matches expectations
3. **Replay** - Use recorded responses during normal test runs

## Fixture Format

All fixtures use JSONL (JSON Lines) format where each line is a complete JSON object representing an SSE event from the Anthropic API.

Example structure:
```jsonl
{"type":"message_start","message":{"id":"msg_123","role":"assistant",...}}
{"type":"content_block_start","index":0,"content_block":{"type":"text",...}}
{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello"}}
...
```

## Fixture Files

- `stream_text_basic.jsonl` - Basic streaming text response
- `stream_text_thinking.jsonl` - Streaming with extended thinking enabled
- `stream_tool_call.jsonl` - Tool/function call response
- `error_401_auth.json` - Authentication error response
- `error_429_rate_limit.json` - Rate limit error (synthetic)

## Updating Fixtures

To capture fresh fixtures from the live API:

```bash
ANTHROPIC_API_KEY=<your-key> VERIFY_MOCKS=1 CAPTURE_FIXTURES=1 make verify-mocks-anthropic
```

**Note:** This makes real API calls and incurs costs.

## Security

Fixtures should never contain API keys. The capture process automatically redacts credentials before writing to files.

To verify no keys leaked, check that no credential patterns appear in fixture files.
