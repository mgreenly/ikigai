# Why Client-Side Tool Execution?

**Decision**: All tools execute on the client, even those that proxy through server (web search, RAG).

**Rationale**:
- **File access**: Client needs access to local filesystem for reading/editing code
- **Shell commands**: Must run in user's actual environment with their permissions
- **Uniform interface**: Client tool handler doesn't need to know which tools proxy to server
- **Full trust model**: No sandboxing complexity—user explicitly runs this tool

**Trade-offs**: Requires client to be more than just a dumb terminal. But this matches the "local development environment" use case.
