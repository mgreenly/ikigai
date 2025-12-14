# Why Start with OpenAI API Format?

**Decision**: Use OpenAI Chat Completions API format as the baseline for LLM provider integration.

**Rationale**:
- **First to market**: OpenAI established the modern chat completion API pattern
- **Widely adopted**: Most widely used format in the ecosystem
- **Well-documented**: Comprehensive documentation and stable API
- **Feature baseline**: Provides essential features (streaming, temperature, max_tokens, etc.)
- **Foundation for abstraction**: Serves as baseline for superset API approach across multiple providers

**Alternatives Considered**:
- **Anthropic Messages API format**: Rejected as baseline because it came later, though its features will be included in the superset approach
- **Custom format**: Rejected to avoid reinventing established patterns and losing ecosystem familiarity

**Trade-offs**:
- **Pro**: Developers familiar with OpenAI API can use the tool immediately
- **Pro**: Well-tested and stable format
- **Pro**: Clear upgrade path to multi-provider support
- **Con**: OpenAI-specific features might not map cleanly to other providers (addressed by superset approach)
- **Con**: May bias design toward OpenAI patterns
