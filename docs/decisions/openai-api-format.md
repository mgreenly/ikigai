# Why Start with OpenAI API Format?

**Decision**: Phase 1 uses OpenAI Chat Completions API format directly. Server acts as thin proxy that injects credentials.

**Rationale**:
- OpenAI was first to market with modern chat completion APIs
- Most widely adopted format in the ecosystem
- Well-documented and stable
- Provides good feature baseline (streaming, temperature, max_tokens, etc.)

This allows rapid Phase 1 implementation while establishing the foundation for provider abstraction in Phase 2.
