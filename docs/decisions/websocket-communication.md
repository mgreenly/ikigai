# Why WebSocket for Client-Server Communication?

**Decision**: Use WebSocket with JSON messages instead of HTTP/2 or gRPC.

**Rationale**:
- Bidirectional streaming needed for LLM responses and tool delegation
- Minimal framing overhead (2-6 bytes per message)
- Native browser support enables future web clients without protocol translation
- Simpler than gRPC for this use case (don't need protobuf complexity)
- libulfius provides robust WebSocket implementation

**Trade-offs**: WebSocket requires connection state management, but that's acceptable given our conversation-centric design.
