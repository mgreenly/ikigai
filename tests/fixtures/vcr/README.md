# VCR Test Fixtures

JSONL format HTTP recordings for deterministic tests.

## Format

Each fixture file contains one JSON object per line:
- `_request`: HTTP request metadata
- `_response`: HTTP response metadata
- `_body`: Complete response body (non-streaming)
- `_chunk`: Raw streaming chunk (one per curl callback)

## Recording

To re-record fixtures:
```bash
VCR_RECORD=1 make check
```

## Security

API keys are redacted automatically. Verify before committing:
```bash
grep -rE "Bearer [^R]" tests/fixtures/vcr/   # Should return nothing
grep -r "sk-" tests/fixtures/vcr/            # Should return nothing
grep -r "sk-ant-" tests/fixtures/vcr/        # Should return nothing
```

## Provider Directories

- `anthropic/` - Anthropic API fixtures
- `google/` - Google Gemini API fixtures
- `brave/` - Brave Search API fixtures
- `openai/` - OpenAI API fixtures (legacy and new)
