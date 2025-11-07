# Future Roadmap

This document outlines potential directions for ikigai **after v1.0**. These are exploration areas, not commitments. The goal is to capture possibilities without over-planning.

**Note**: We're currently in early development (REPL terminal foundation). v1.0 is the long-term vision. This document looks even further ahead.

## v1.0 Vision

**What v1.0 aims to deliver** (not yet complete):
- Robust desktop AI coding agent
- Terminal UI with streaming responses
- Database-backed conversation history
- Multi-LLM provider support (OpenAI, Anthropic, Google)
- Local tool execution (files, shell, code analysis)

## Post-v1.0 Exploration Areas

### Multi-User Server Architecture

**Motivation**: Share AI agent capabilities across a team or organization.

**High-level concept**:
- Server mediates access to LLMs and shared knowledge
- Clients connect via WebSocket protocol
- Conversations stored centrally
- Tool execution remains local to client machines

**Key challenges**:
- Identity and authentication
- Concurrent connection handling
- Resource management and rate limiting
- Graceful shutdown with active streaming requests

**Prior work**: See `docs/archive/phase-1.md` and `docs/archive/phase-1-details.md` for early server architecture exploration.

### Organizational Memory and RAG

**Motivation**: Build shared knowledge from all team conversations.

**Concepts to explore**:
- Vector embeddings of conversation history
- Semantic search across all stored exchanges
- Context injection for relevant prior conversations
- Knowledge base that grows with usage

**Questions**:
- Storage backend: PostgreSQL with pgvector? Separate vector DB?
- Embedding model selection and hosting
- Privacy and access control for shared knowledge
- RAG retrieval strategies

### Enhanced Tool Capabilities

**Potential additions**:
- **Code analysis**: tree-sitter integration for AST parsing
- **Web search**: Integrate search APIs (server-side)
- **Documentation fetch**: Pull docs/manpages on demand
- **Git operations**: Status, diff, commit, branch management
- **Test execution**: Run tests, parse results, iterate
- **Build operations**: Compile, link, report errors

**Design principle**: Keep tool interface uniform. Client shouldn't distinguish between local and remote tools.

### Collaborative Features

**Ideas**:
- Share conversation sessions between users
- Review/comment on AI-generated changes
- Team-wide conversation search
- Notification system for relevant updates

**Requires**: Multi-user server foundation.

### Protocol Enhancements

**Potential improvements**:
- Bidirectional communication (server-initiated messages)
- Multiple concurrent operations per connection
- Progress reporting for long-running operations
- Partial result streaming for tools

**Note**: Current v1.0 is direct API calls. WebSocket protocol development would come with multi-user server work.

### Storage and Retrieval

**Areas to explore**:
- Efficient conversation search
- Export/import of conversation history
- Backup and restore strategies
- Data retention policies
- Privacy controls (what gets stored where)

### UI Enhancements

**Terminal client improvements**:
- Syntax highlighting for code blocks
- Diff visualization for file changes
- Multi-pane layout (conversation + file view)
- Inline images/diagrams
- Rich formatting (tables, lists, emphasis)

**Alternative interfaces**:
- Web UI for browsing conversation history
- IDE integration (LSP, editor plugins)
- Mobile companion app (view-only)

## Non-Goals

Things explicitly **NOT** in scope:

- **Production-grade infrastructure**: This is an experimentation platform
- **Sandboxing/security**: Full trust model (user's machine, user's responsibility)
- **Enterprise features**: SSO, audit logs, compliance tooling
- **Scale targets**: Not optimizing for thousands of users
- **Model hosting**: We call external APIs, not hosting our own LLMs

## Approach

For post-v1.0 work:
1. **Use v1.0 heavily**: Identify pain points and missing capabilities through real usage
2. **Experiment quickly**: Try ideas, measure results, iterate
3. **Stay focused**: Don't spread work across too many directions at once
4. **Plan minimally**: Defer detailed design until ready to build
5. **Keep it fun**: This is a learning and experimentation platform

---

**Remember**: v1.0 comes first. These are just possibilities for later exploration.
