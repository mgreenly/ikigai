# Why talloc for Memory Management?

**Decision**: Use talloc hierarchical allocator instead of manual malloc/free or custom arena.

**Rationale**:
- **Automatic cleanup**: Parent context frees all children
- **Lifecycle match**: Request/response cycles map to context hierarchies naturally
- **Debugging**: Built-in leak detection and memory tree reporting
- **Battle-tested**: Used in Samba, proven reliable
- **Available**: Already in Debian repos (libtalloc-dev)

**Alternatives Considered**:
- **Manual malloc/free**: Rejected due to error-prone cleanup, especially with multiple error paths
- **Custom arena allocator**: Rejected as it would reinvent talloc's functionality without the debugging benefits

**Trade-offs**:
- **Pro**: Automatic cleanup prevents memory leaks
- **Pro**: Built-in debugging and leak detection
- **Pro**: Natural mapping to object lifecycles
- **Con**: External library dependency
- **Con**: Learning curve for developers unfamiliar with hierarchical allocators

**Pattern**: Connection context → message context → parsing allocations
