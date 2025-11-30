# C Library for OpenAI Tokenization

## Overview

This document explores the feasibility of creating a C library that implements OpenAI's tiktoken tokenization, enabling token counting compatible with GPT models.

## Background

OpenAI uses different tokenizers depending on the model:

| Tokenizer | Models | Vocabulary Size |
|-----------|--------|-----------------|
| o200k_base | GPT-5, GPT-5-mini, GPT-5-nano, GPT-4o | ~200k tokens |
| cl100k_base | GPT-4, GPT-3.5-turbo | ~100k tokens |
| p50k_base | Older Codex models | ~50k tokens |
| r50k_base | Older GPT-3 models | ~50k tokens |

## How tiktoken Works

1. **BPE (Byte Pair Encoding)** - Uses a vocabulary of ~100k-200k tokens
2. **Vocabulary storage** - Base64-encoded file mapping byte sequences to token IDs
3. **Core algorithm** - Greedily match the longest token from the vocabulary, repeat

## C Implementation Requirements

### Core Components

1. **Hash table** - Fast vocabulary lookups (byte sequence to token ID)
2. **UTF-8 handling** - Input text must be processed as bytes
3. **BPE merge logic** - Iteratively merge byte pairs according to vocabulary
4. **Regex pre-tokenization** - tiktoken splits text into chunks before BPE (this is the trickiest part)

### Existing Resources

- `tiktoken-rs` (Rust) - Reference implementation
- Vocabulary files publicly available in the tiktoken repo
- Regex patterns are documented in tiktoken source

### Complexity Estimate

| Component | Lines of C |
|-----------|------------|
| Core BPE encoder | 300-500 |
| Hash table + memory management | 200-300 |
| Regex pre-tokenization | ~200 (or use PCRE2) |
| **Total** | **800-1200** |

## Key Challenge: Pre-tokenization Regex

tiktoken uses a specific regex pattern to split text before applying BPE. For example, the pattern for cl100k_base handles:

- Contractions (e.g., `'s`, `'t`, `'re`)
- Words with optional leading/trailing spaces
- Numbers
- Non-whitespace/non-letter/non-number sequences

Options:
1. Implement the pattern manually in C
2. Use a regex library (PCRE2, RE2)
3. Port the regex logic from tiktoken-rs

## Design Decisions

### Vocabulary Loading

| Approach | Pros | Cons |
|----------|------|------|
| Embedded at compile time | No runtime files needed, single binary | Larger binary (~2-4MB per encoding) |
| Load from file at runtime | Smaller binary, can update vocab | Requires vocab file distribution |

### Operations to Support

- **Encode only** - Text to token IDs (required for counting)
- **Decode** - Token IDs back to text (optional, useful for debugging)
- **Count only** - Just return count without allocating token array

## References

- [tiktoken GitHub](https://github.com/openai/tiktoken)
- [tiktoken-rs](https://github.com/zurawiki/tiktoken-rs)
- [gpt-tokenizer (JavaScript)](https://github.com/niieani/gpt-tokenizer)
- [OpenAI Tokenizer Tool](https://platform.openai.com/tokenizer)
