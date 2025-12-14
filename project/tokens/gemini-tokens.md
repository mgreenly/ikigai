# Google Gemini Token Counting

## Overview

Google Gemini supports both offline tokenization and API-based token counting. The tokenizer is based on SentencePiece BPE and is shared with the open-weights Gemma models.

## Offline Tokenization

### Tokenizer Details

- Algorithm: SentencePiece BPE (byte-pair encoding)
- Vocabulary size: 256,000 tokens
- Model file size: ~4.24 MB
- Shared between Gemini and Gemma models

### Vocabulary Files

The tokenizer files are publicly available via the Gemma release:

- `tokenizer.model` - Binary SentencePiece model
- `tokenizer.json` - Human-readable vocabulary list

Source: https://github.com/google/gemma_pytorch/blob/main/tokenizer/tokenizer.model

### Available Libraries

| Language | Library | Notes |
|----------|---------|-------|
| C++ | [SentencePiece](https://github.com/google/sentencepiece) | Official Google library |
| Python | `sentencepiece` (pip) | Official Python bindings |
| Go | [go-sentencepiece](https://github.com/eliben/go-sentencepiece) | Focused on Gemma/Gemini BPE |
| Java | DJL SentencePiece (`ai.djl.sentencepiece:sentencepiece`) | JNI wrapper |

### Python Example (Offline)

```python
import sentencepiece as spm

# Load the Gemma/Gemini tokenizer
sp = spm.SentencePieceProcessor()
sp.Load("tokenizer.model")

# Encode text to tokens
tokens = sp.EncodeAsIds("Hello, world!")
print(f"Token count: {len(tokens)}")
print(f"Tokens: {tokens}")

# Decode back to text
text = sp.DecodeIds(tokens)
print(f"Decoded: {text}")
```

### Go Example (Offline)

```go
import "github.com/eliben/go-sentencepiece"

// Load tokenizer
proc, err := sentencepiece.NewProcessorFromPath("tokenizer.model")
if err != nil {
    log.Fatal(err)
}

// Encode
tokens := proc.Encode("Hello, world!")
fmt.Printf("Token count: %d\n", len(tokens))
```

## API Token Counting

### Endpoint

The CountTokens API is available via the Gemini API.

### Cost

**Free** - No charge for calling countTokens.

### Rate Limits

- 3000 requests per minute (RPM)

### Python SDK Usage

```python
from google import genai

client = genai.Client()

# Simple text
result = client.models.count_tokens(
    model="gemini-2.0-flash",
    contents="Hello, world!"
)
print(result.total_tokens)
```

### With Conversation History

```python
result = client.models.count_tokens(
    model="gemini-2.0-flash",
    contents=[
        {"role": "user", "parts": [{"text": "Hi, how are you?"}]},
        {"role": "model", "parts": [{"text": "I'm doing well, thanks!"}]},
        {"role": "user", "parts": [{"text": "What can you help me with?"}]}
    ]
)
```

### Supported Content Types

- Text
- Images
- Video
- Audio
- PDF documents

## Token Basics

- 1 token ~ 4 characters
- 100 tokens ~ 60-80 English words

## Context Windows (2025)

| Model | Context Window |
|-------|----------------|
| Gemini 2.5 Flash | 1M tokens |
| Gemini 2.5 Pro | 2M tokens |
| Gemini 3 | Check latest docs |

## Vocabulary Structure

- Total pieces: 256,000 (IDs 0-255,999)
- Special/Control tokens: ~506 pieces
  - `<pad>` - Padding token
  - `<unused#>` - Reserved tokens
  - `<0x##>` - Byte code-point markers

## Comparison with Other Providers

| Provider | Offline Library | API Count | Cost |
|----------|----------------|-----------|------|
| OpenAI | tiktoken | N/A | N/A |
| Anthropic | None | Yes | Free |
| **Google** | SentencePiece | Yes | Free |

## References

- [Gemini Token Counting Docs](https://ai.google.dev/gemini-api/docs/tokens)
- [SentencePiece GitHub](https://github.com/google/sentencepiece)
- [go-sentencepiece](https://github.com/eliben/go-sentencepiece)
- [Gemma Tokenizer Model](https://github.com/google/gemma_pytorch/blob/main/tokenizer/tokenizer.model)
- [Dissecting Gemini's Tokenizer](https://dejan.ai/blog/gemini-toknizer/)
