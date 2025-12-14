# Anthropic Claude Token Counting

## Overview

Unlike OpenAI which publishes tiktoken with vocabulary files, Anthropic does not publicly release their tokenizer. Token counting must be done via API.

## Token Counting API

### Endpoint

```
POST https://api.anthropic.com/v1/messages/count_tokens
```

### Cost

**Free** - Token counting does not incur token costs. Only rate limited based on your API tier.

### Rate Limits

- Subject to requests per minute limits based on usage tier
- Token counting and message creation have separate, independent rate limits

### Python SDK Usage

```python
import anthropic

client = anthropic.Anthropic()

response = client.messages.count_tokens(
    model="claude-sonnet-4-5",
    messages=[
        {"role": "user", "content": "Hello, Claude"}
    ]
)

print(response.input_tokens)
```

### With System Prompt and Tools

```python
response = client.messages.count_tokens(
    model="claude-sonnet-4-5",
    system="You are a helpful assistant.",
    messages=[
        {"role": "user", "content": "What can you help me with?"}
    ],
    tools=[
        {
            "name": "get_weather",
            "description": "Get the weather for a location",
            "input_schema": {
                "type": "object",
                "properties": {
                    "location": {"type": "string"}
                },
                "required": ["location"]
            }
        }
    ]
)
```

### Supported Content Types

- Text messages
- System prompts
- Tools and function definitions
- Images
- PDFs
- Extended thinking blocks

### Important Notes

- Token count is an **estimate** - actual usage may differ by a small amount
- Counts may include system-optimized tokens that don't affect billing
- All Claude models (Opus, Sonnet, Haiku) use the same tokenizer

## Tokenizer Technical Details

- Algorithm: BPE (byte-pair encoding)
- Vocabulary size: ~65K tokens (plus 5 special tokens)
- ~70% overlap with GPT-4's cl100k_base vocabulary

## Offline Alternatives

### Not Recommended

1. **Legacy TypeScript package** (`@anthropic-ai/tokenizer`)
   - Only accurate for pre-Claude 3 models
   - Deprecated, not maintained

2. **tiktoken cl100k_base approximation**
   - Provides rough ballpark estimates only
   - Not accurate for billing predictions

### Why No Offline Tokenizer

Anthropic has not published:
- Vocabulary files
- Merge rules
- Tokenizer implementation for Claude 3+

This means creating an accurate offline tokenizer is not currently feasible.

## Strategy

For applications needing token counts:

1. **Pre-flight validation**: Use the free `count_tokens` API before sending expensive requests
2. **Caching**: Cache token counts for repeated content
3. **Estimation**: For rough estimates only, use tiktoken's cl100k_base (expect ~10-20% variance)

## References

- [Anthropic Token Counting Docs](https://platform.claude.com/docs/en/build-with-claude/token-counting)
- [anthropic-tokenizer-typescript (deprecated)](https://github.com/anthropics/anthropic-tokenizer-typescript)
