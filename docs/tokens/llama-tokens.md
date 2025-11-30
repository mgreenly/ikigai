# Meta Llama Token Counting

## Overview

Meta's Llama models are open source, meaning full offline tokenization is available. The tokenizer files are published on HuggingFace and GitHub.

## Tokenizer Evolution

| Model | Vocab Size | Tokenizer Type | Notes |
|-------|------------|----------------|-------|
| Llama 1 | 32K | SentencePiece BPE | Original |
| Llama 2 | 32K | SentencePiece BPE | Same as Llama 1 |
| Llama 3/3.1/3.2/3.3 | 128K | tiktoken-based | 4x larger vocab |
| Llama 4 | 202K | tiktoken-based | 200 language support |

## Key Change: Llama 3+

Llama 3 switched from SentencePiece to a tiktoken-based tokenizer:

- 4x larger vocabulary (32K to 128K tokens)
- Up to 15% fewer tokens compared to Llama 2
- Improved multilingual support
- Byte-level BPE (no UNK tokens)

## Offline Tokenization

### Using HuggingFace Transformers

```python
from transformers import AutoTokenizer

# Load tokenizer (requires HuggingFace login and license acceptance)
tokenizer = AutoTokenizer.from_pretrained(
    "meta-llama/Meta-Llama-3-8B-Instruct"
)

# Encode text to tokens
text = "Hello, world!"
tokens = tokenizer.encode(text)
print(f"Token count: {len(tokens)}")
print(f"Token IDs: {tokens}")

# Decode back to text
decoded = tokenizer.decode(tokens)
print(f"Decoded: {decoded}")
```

### Download Tokenizer for Offline Use

```bash
# Install HuggingFace CLI
pip install huggingface_hub

# Login (required for Llama models)
huggingface-cli login

# Download tokenizer files
huggingface-cli download meta-llama/Meta-Llama-3-8B-Instruct \
    --include "original/*" \
    --local-dir ./llama3-tokenizer
```

### Using Native Llama Tokenizer

```python
# From Meta's llama3 repository
from llama.tokenizer import Tokenizer

tokenizer = Tokenizer(model_path="tokenizer.model")
tokens = tokenizer.encode("Hello, world!", bos=True, eos=False)
print(f"Token count: {len(tokens)}")
```

### Llama 2 (SentencePiece)

For Llama 2 and earlier:

```python
import sentencepiece as spm

sp = spm.SentencePieceProcessor()
sp.Load("tokenizer.model")

tokens = sp.EncodeAsIds("Hello, world!")
print(f"Token count: {len(tokens)}")
```

## Access Requirements

Llama models require license acceptance:

1. Visit https://huggingface.co/meta-llama/Meta-Llama-3-8B
2. Read and accept Meta's license agreement
3. Wait for approval (usually instant)
4. Use HuggingFace CLI or API with authentication

## Available Models

### Llama 3 Family

- `meta-llama/Meta-Llama-3-8B`
- `meta-llama/Meta-Llama-3-8B-Instruct`
- `meta-llama/Meta-Llama-3-70B`
- `meta-llama/Meta-Llama-3-70B-Instruct`

### Llama 4 Family

- `meta-llama/Llama-4-Scout-17B-16E` (109B total)
- `meta-llama/Llama-4-Maverick-17B-128E` (402B total)

## Token Efficiency

Llama 3's larger vocabulary improves efficiency:

- English text: ~15% fewer tokens than Llama 2
- Code: Improved tokenization of common patterns
- Multilingual: Better coverage for non-English languages

## File Structure

When downloading Llama tokenizer files:

```
original/
├── tokenizer.model    # tiktoken format (Llama 3+) or SentencePiece (Llama 2)
├── tokenizer.json     # Fast tokenizer format (optional)
└── special_tokens_map.json
```

## JavaScript/TypeScript

```javascript
// Using @xenova/transformers (browser/Node.js)
import { AutoTokenizer } from '@xenova/transformers';

const tokenizer = await AutoTokenizer.from_pretrained(
    'meta-llama/Meta-Llama-3-8B-Instruct'
);

const { input_ids } = await tokenizer("Hello, world!");
console.log(`Token count: ${input_ids.length}`);
```

## Comparison with Other Providers

| Provider | Offline Library | Open Source | License |
|----------|----------------|-------------|---------|
| OpenAI | tiktoken | Tokenizer only | MIT |
| Anthropic | None | No | N/A |
| Google | SentencePiece | Tokenizer only | Apache 2.0 |
| **Meta** | Full | Model + Tokenizer | Meta License |

## References

- [HuggingFace Llama3 Docs](https://huggingface.co/docs/transformers/en/model_doc/llama3)
- [HuggingFace Llama4 Docs](https://huggingface.co/docs/transformers/model_doc/llama4)
- [Meta Llama 3 GitHub](https://github.com/meta-llama/llama3)
- [Meta Llama Models GitHub](https://github.com/meta-llama/llama-models)
- [Llama 4 Announcement](https://ai.meta.com/blog/llama-4-multimodal-intelligence/)
