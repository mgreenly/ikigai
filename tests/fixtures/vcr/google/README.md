# Google Gemini API VCR Fixtures

This directory contains recorded HTTP responses from the Google Gemini API for testing purposes.

## VCR-Style Testing

These fixtures follow the VCR (Video Cassette Recorder) pattern:
1. **Record** - Capture real API responses during verification tests
2. **Verify** - Assert response structure matches expectations
3. **Replay** - Use recorded responses during normal test runs

## Fixture Format

All fixtures use JSONL (JSON Lines) format where each line is a complete JSON object representing an SSE data event from the Google API.

Example structure:
```jsonl
{"candidates":[{"content":{"parts":[{"text":"Hello"}]}}]}
{"candidates":[{"content":{"parts":[{"text":" world"}]}}]}
{"candidates":[{"finishReason":"STOP"}],"usageMetadata":{"promptTokenCount":5,...}}
```

## Fixture Files

- `stream_text_basic.jsonl` - Basic streaming text response
- `stream_text_thinking.jsonl` - Streaming with thinking mode (Gemini 2.5+)
- `stream_tool_call.jsonl` - Function calling response
- `error_401_auth.json` - Authentication error response

## Updating Fixtures

To capture fresh fixtures from the live API:

```bash
GOOGLE_API_KEY=<your-key> VERIFY_MOCKS=1 CAPTURE_FIXTURES=1 make verify-mocks-google
```

**Note:** This makes real API calls and incurs costs.

## Security

Fixtures should never contain API keys. The capture process uses API keys only in URL parameters (not in response bodies).

To verify no keys leaked, check that no credential patterns appear in fixture files.
