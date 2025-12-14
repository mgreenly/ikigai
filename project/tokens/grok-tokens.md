# xAI Grok Token Counting

## Overview

xAI's Grok models use their own tokenizer, separate from OpenAI's despite having an OpenAI-compatible API. The tokenizer is open source and available for offline use.

## Tokenizer Details

- Algorithm: SentencePiece BPE (byte-pair encoding)
- Vocabulary size: 131,072 tokens
- Byte-fallback: Handles symbols, emojis, and special characters
- Multilingual: Optimized for multiple languages

## Important Note

**Different Grok models may use different tokenizers.** The same prompt may have different token counts across different Grok models.

## Offline Tokenization

### Tokenizer File

The tokenizer model is available in the open source Grok-1 release:

https://github.com/xai-org/grok-1/blob/main/tokenizer.model

### Python Usage

```python
import sentencepiece as spm

# Load the Grok tokenizer
sp = spm.SentencePieceProcessor()
sp.Load("tokenizer.model")

# Encode text to tokens
text = "Hello, world!"
tokens = sp.EncodeAsIds(text)
print(f"Token count: {len(tokens)}")
print(f"Token IDs: {tokens}")

# Decode back to text
decoded = sp.DecodeIds(tokens)
print(f"Decoded: {decoded}")
```

### Download Tokenizer

```bash
# Clone just the tokenizer file
curl -O https://raw.githubusercontent.com/xai-org/grok-1/main/tokenizer.model
```

## API Token Counting

### Tokenize Endpoint

xAI provides a tokenization API endpoint:

```python
import requests

response = requests.post(
    "https://api.x.ai/v1/tokenize",
    headers={
        "Authorization": "Bearer YOUR_API_KEY",
        "Content-Type": "application/json"
    },
    json={
        "text": "Hello, world!"
    }
)

tokens = response.json()
print(f"Token count: {len(tokens['tokens'])}")
```

### xAI Console

The xAI Console includes a built-in tokenizer visualization tool for exploring how text is tokenized.

## Context Windows

| Model | Context Length |
|-------|----------------|
| Grok-1 | 8K tokens |
| Grok-1.5 | 128K tokens |
| Grok-2 | Check latest docs |

## API Compatibility vs Tokenizer

xAI uses an OpenAI-compatible API format, but this does NOT mean they use the same tokenizer:

| Aspect | OpenAI | xAI |
|--------|--------|-----|
| API Format | OpenAI | OpenAI-compatible |
| Tokenizer | tiktoken | SentencePiece |
| Vocab Size | 100-200K | 131K |

The API format (`/v1/chat/completions`, etc.) is just an interface standard. Token counts will differ between providers.

## Pricing Considerations

Token-based pricing uses xAI's tokenizer, not OpenAI's. Always use xAI's tokenization tools for accurate cost estimation.

## Third-Party Tools

- [Lunary Grok Tokenizer](https://lunary.ai/grok-tokenizer) - Web-based token counter for Grok models

## Comparison with Other Providers

| Provider | Offline Library | Open Source | Vocab Size |
|----------|----------------|-------------|------------|
| OpenAI | tiktoken | Tokenizer only | 100-200K |
| Anthropic | None | No | ~65K |
| Google | SentencePiece | Tokenizer only | 256K |
| Meta | Full | Model + Tokenizer | 128-202K |
| **xAI** | SentencePiece | Tokenizer + Grok-1 | 131K |

## References

- [xAI Consumption & Rate Limits](https://docs.x.ai/docs/key-information/consumption-and-rate-limits)
- [Grok-1 GitHub Repository](https://github.com/xai-org/grok-1)
- [Grok-1 tokenizer.model](https://github.com/xai-org/grok-1/blob/main/tokenizer.model)
- [xAI Tutorial](https://docs.x.ai/docs/tutorial)
- [Grok-1.5 Announcement](https://x.ai/news/grok-1.5)
