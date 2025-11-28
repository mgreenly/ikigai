# Why Superset API Approach?

**Decision**: Internal protocol will evolve to support the superset of all LLM provider features, not the common subset.

**Rationale**:
- **User choice**: Users can switch between models/providers without losing functionality
- **Feature exposure**: Advanced features (thinking tokens, extended context, etc.) are available when supported
- **Provider polyfill**: Individual provider adapters handle missing features through:
  - Ignoring unsupported parameters
  - Translating concepts (e.g., folding system prompt into messages for providers without native support)
  - Null operations where appropriate

**Alternative considered**: Common subset approach would artificially limit functionality to the least capable provider.
