# Tokenizer Library Design

## Overview

A shared library (`libikigai-tokenizer.so`) providing unified token counting across multiple LLM providers. Designed for use by ikigai and packaged as a single `.deb` for Linux.

## Goals

1. Count tokens offline for supported providers
2. Provide consistent C API across all tokenizers
3. Single package with all vocabularies (~15-20MB total)
4. Fallback estimation for providers without published vocabularies

## Architecture

```
libikigai-tokenizer.so
│
├── Algorithms
│   ├── tiktoken BPE
│   │   └── Used by: OpenAI, Llama 3+
│   │
│   └── SentencePiece BPE
│       └── Used by: Gemini/Gemma, Llama 1-2, Grok
│
├── Vocabularies (embedded)
│   ├── openai-o200k     (~2MB)  - GPT-4o, GPT-5 family
│   ├── openai-cl100k    (~2MB)  - GPT-4, GPT-3.5-turbo
│   ├── gemini           (~4MB)  - Gemini, Gemma
│   ├── llama3           (~3MB)  - Llama 3, 3.1, 3.2, 3.3
│   ├── llama4           (~4MB)  - Llama 4
│   ├── llama2           (~1MB)  - Llama 1, 2
│   └── grok             (~2MB)  - Grok 1, 2
│
└── C API
    ├── tokenize()
    ├── count_tokens()
    ├── detokenize()
    └── get_token_info()
```

## Provider Support

| Provider | Algorithm | Vocabulary | Offline | Notes |
|----------|-----------|------------|---------|-------|
| OpenAI | tiktoken BPE | o200k_base, cl100k_base | Exact | Full support |
| Google | SentencePiece BPE | Gemma vocab (256K) | Exact | Shared with Gemma |
| Meta | tiktoken BPE | Llama 3/4 (128-202K) | Exact | Full support |
| Meta | SentencePiece BPE | Llama 1/2 (32K) | Exact | Legacy support |
| xAI | SentencePiece BPE | Grok vocab (131K) | Exact | Full support |
| Anthropic | BPE | Not published | **Estimate** | Uses cl100k_base fallback |

## Anthropic Fallback Strategy

Anthropic uses standard BPE but hasn't published their vocabulary. Known details:

- Vocabulary size: ~65K tokens
- ~70% overlap with OpenAI's cl100k_base
- Expected estimation error: 10-20%

The library will:
1. Use cl100k_base for Anthropic token counting
2. Return a status flag indicating estimate vs exact
3. Caller can decide whether to use API for ground truth

```c
TokenResult result = count_tokens("anthropic", "claude-sonnet-4", text);
if (result.flags & TOKEN_ESTIMATE) {
    // Result is approximate, call API if exact count needed
}
```

## C API Design

### Core Functions

```c
#include <ikigai-tokenizer.h>

// Initialize library (loads vocabularies)
int ikt_init(void);

// Cleanup
void ikt_cleanup(void);

// Count tokens
TokenResult ikt_count_tokens(
    const char *provider,    // "openai", "anthropic", "google", "meta", "xai"
    const char *model,       // "gpt-4o", "claude-sonnet-4", "gemini-2.0-flash", etc.
    const char *text,
    size_t text_len
);

// Tokenize (get token IDs)
TokenizeResult ikt_tokenize(
    const char *provider,
    const char *model,
    const char *text,
    size_t text_len
);

// Detokenize (token IDs to text)
char *ikt_detokenize(
    const char *provider,
    const char *model,
    const uint32_t *token_ids,
    size_t num_tokens
);

// Free allocated memory
void ikt_free(void *ptr);
```

### Result Structures

```c
typedef struct {
    size_t count;           // Token count
    uint32_t flags;         // TOKEN_EXACT or TOKEN_ESTIMATE
    const char *error;      // NULL if success
} TokenResult;

typedef struct {
    uint32_t *tokens;       // Array of token IDs
    size_t count;           // Number of tokens
    uint32_t flags;         // TOKEN_EXACT or TOKEN_ESTIMATE
    const char *error;      // NULL if success
} TokenizeResult;

// Flags
#define TOKEN_EXACT     0x00
#define TOKEN_ESTIMATE  0x01
```

### Model Resolution

The library maps model names to vocabularies:

```c
// These all resolve to o200k_base
ikt_count_tokens("openai", "gpt-5", text, len);
ikt_count_tokens("openai", "gpt-5-mini", text, len);
ikt_count_tokens("openai", "gpt-4o", text, len);

// These resolve to cl100k_base
ikt_count_tokens("openai", "gpt-4", text, len);
ikt_count_tokens("openai", "gpt-3.5-turbo", text, len);

// Anthropic uses cl100k_base fallback with ESTIMATE flag
ikt_count_tokens("anthropic", "claude-sonnet-4", text, len);
```

## Implementation Options

### Option 1: Pure C (Recommended)

- Implement BPE algorithm in C (~800-1200 lines)
- Use hash table for vocabulary lookups
- Embed vocabularies as static data or load from files
- No external dependencies

### Option 2: Wrap Existing Libraries

- Link against SentencePiece (C++)
- Port tiktoken core logic
- More complex build, C++ dependency

### Option 3: Rust with C FFI

- Use tiktoken-rs crate
- Expose C API via FFI
- Requires Rust toolchain

**Recommendation:** Option 1 (Pure C) for simplicity and minimal dependencies.

## Packaging

### Single .deb Package

```
libikigai-tokenizer_1.0.0_amd64.deb (~15-20MB)
│
├── /usr/lib/x86_64-linux-gnu/
│   └── libikigai-tokenizer.so.1.0.0
│
├── /usr/include/
│   └── ikigai-tokenizer.h
│
└── /usr/share/ikigai-tokenizer/
    └── vocabs/
        ├── openai-o200k.bin
        ├── openai-cl100k.bin
        ├── gemini.bin
        ├── llama3.bin
        ├── llama4.bin
        ├── llama2.bin
        └── grok.bin
```

### Why Single Package

1. Total size is small (~15-20MB)
2. Simpler installation: `apt install libikigai-tokenizer`
3. No version mismatch between lib and vocab packages
4. All vocabularies are permissively licensed (MIT, Apache 2.0)

## Vocabulary Sources

| Vocabulary | Source | License |
|------------|--------|---------|
| o200k_base | tiktoken package | MIT |
| cl100k_base | tiktoken package | MIT |
| Gemini/Gemma | google/gemma_pytorch | Apache 2.0 |
| Llama 3/4 | meta-llama repos | Meta License |
| Llama 2 | meta-llama repos | Meta License |
| Grok | xai-org/grok-1 | Apache 2.0 |

## Future Considerations

1. **Anthropic vocabulary release** - If published, add as exact tokenizer
2. **New providers** - Mistral, Cohere, etc. (most use SentencePiece)
3. **Vocabulary updates** - New model releases may need vocab updates
4. **Embedded vs file loading** - Could offer compile-time option

## References

- [OpenAI tiktoken](https://github.com/openai/tiktoken)
- [Google SentencePiece](https://github.com/google/sentencepiece)
- [Gemma tokenizer](https://github.com/google/gemma_pytorch/blob/main/tokenizer/tokenizer.model)
- [Meta Llama models](https://github.com/meta-llama/llama-models)
- [xAI Grok-1](https://github.com/xai-org/grok-1)

## Related Documentation

- [tiktoken-lib.md](tiktoken-lib.md) - C library feasibility for OpenAI
- [anthropic-tokens.md](anthropic-tokens.md) - Anthropic token counting
- [gemini-tokens.md](gemini-tokens.md) - Google Gemini token counting
- [llama-tokens.md](llama-tokens.md) - Meta Llama token counting
- [grok-tokens.md](grok-tokens.md) - xAI Grok token counting
- [usage-strategy.md](usage-strategy.md) - How ikigai uses token counting
