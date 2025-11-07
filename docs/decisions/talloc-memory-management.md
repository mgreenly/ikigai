# Why talloc for Memory Management?

**Decision**: Use talloc hierarchical allocator instead of manual malloc/free or custom arena.

**Rationale**:
- **Automatic cleanup**: Parent context frees all children
- **Lifecycle match**: Request/response cycles map to context hierarchies naturally
- **Debugging**: Built-in leak detection and memory tree reporting
- **Battle-tested**: Used in Samba, proven reliable
- **Available**: Already in Debian repos (libtalloc-dev)

**Trade-offs**: Dependency on external library, but the productivity gain outweighs this cost.

**Pattern**: Connection context → message context → parsing allocations
