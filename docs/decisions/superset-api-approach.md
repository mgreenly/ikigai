# Why Superset API Approach?

**Decision**: Internal protocol will evolve to support the superset of all LLM provider features, not the common subset.

**Rationale**:
- **User choice**: Users can switch between models/providers without losing functionality
- **Feature exposure**: Advanced features (thinking tokens, extended context, etc.) are available when supported
- **Provider polyfill**: Individual provider adapters handle missing features through:
  - Ignoring unsupported parameters
  - Translating concepts (e.g., folding system prompt into messages for providers without native support)
  - Null operations where appropriate

**Alternatives Considered**:
- **Common subset approach**: Limit API to features supported by all providers. Rejected because it would artificially constrain functionality to the least capable provider, preventing users from accessing advanced features when available.

**Trade-offs**:
- **Pro**: Users can access all features of any provider
- **Pro**: Switching providers doesn't lose functionality
- **Pro**: New provider features can be added without breaking existing code
- **Pro**: Provider-specific optimizations possible
- **Con**: More complex provider adapters (must handle feature translation/polyfilling)
- **Con**: API surface grows as new providers add unique features
- **Con**: Some features may behave differently across providers
