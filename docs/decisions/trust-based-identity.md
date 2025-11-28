# Why Trust-Based Identity?

**Decision**: Client identity is `hostname@username` with no authentication in initial phases.

**Rationale**:
- Target use case is internal development tool
- Focus on functionality over security in early phases
- Reduces implementation complexity during prototyping

**Threat model**: System assumes trusted network and trusted users. Not suitable for public deployment without adding authentication layer.

**Future consideration**: Authentication can be added in later phase without breaking protocol structure (add auth handshake before first exchange).
