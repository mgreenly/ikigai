# Inspiration and Influences

Structured Memory builds on ideas from multiple sources.

## Letta / MemGPT

**Source**: [MemGPT Research Paper](https://arxiv.org/abs/2310.08560), [Letta Platform](https://www.letta.com/)

### Core Concepts Adopted

**Memory Blocks**:
- Always-visible context sections
- Agent can self-edit via tools
- Structured by purpose (persona, human, custom)

**Self-Editing Memory**:
- Agent updates its own context
- Learning happens through memory modification
- Memory persists across conversations

**Tool-Based Management**:
- Memory operations exposed as function calls
- Agent decides when to update
- Explicit vs implicit memory management

### ikigai Extensions

Where ikigai extends Letta's concepts:

**Larger Blocks**:
- Letta: ~2k chars per block
- ikigai: 100k token budget (flexible per-block sizing)
- Enables complex, detailed knowledge storage

**Sliding Window**:
- Letta: Sends full conversation history
- ikigai: 90k sliding window with graceful eviction
- Better long-term scalability

**Active Compression**:
- Letta: Blocks have fixed size limits
- ikigai: `/compact` command for active compression
- Information density optimization

**Unified Storage**:
- Letta: Memory blocks separate from files
- ikigai: Same tools for blocks and files (`ikigai://` URIs)
- Simpler mental model

**Auto-Summary Layer**:
- Letta: Archival memory (vector DB)
- ikigai: Auto-summary + archival (PostgreSQL)
- Explicit index to what fell off window

---

## Cache Hierarchies (Computer Architecture)

**Source**: Computer systems design

### Concepts Adopted

**L1/L2/L3 Cache Analogy**:
```
CPU Registers     → Pinned Blocks (always visible, highest value)
L1 Cache          → Sliding Window (recent, fast access)
L2 Cache          → Auto-Summary (index to slower storage)
RAM               → Archival Memory (unlimited, slower access)
```

**Principles**:
- Different tiers, different purposes
- Smaller + faster vs larger + slower
- Information flows between tiers
- Eviction policies (LRU for sliding window)

**Budget Allocation**:
- Fixed total budget (context = cache space)
- Partition by access patterns
- Optimize hit rates

---

## Database Query Planning

**Source**: PostgreSQL, MySQL query optimizers

### Concepts Adopted

**Working Memory vs Disk**:
- working_mem (PostgreSQL): Fast, limited, in-memory operations
- Disk: Unlimited, slower, persistent
- Similar to sliding window vs archival

**Statistics and Summaries**:
- Database maintains statistics (row counts, distributions)
- Used to guide query planning
- Similar to auto-summary guiding `/recall` usage

**Explicit Control**:
- Users can ANALYZE tables, set statistics targets
- Manual tuning for better performance
- Similar to user curating pinned blocks

---

## Git and Version Control

**Source**: Git's design philosophy

### Concepts Adopted

**Complete History**:
- Git never loses data (reflog)
- ikigai never loses messages (archival)
- Safe to "rewrite history" (context) when full record exists

**Working Directory vs Repository**:
- Working directory: Current files (sliding window)
- Repository: All commits (archival)
- Clean separation of "current" vs "historical"

**Explicit Operations**:
- Git requires explicit add/commit (no auto-save)
- ikigai requires explicit `/remember` (no auto-extract)
- User control over what persists

---

## Document Knowledge Management

**Source**: Second brain, Zettelkasten, personal knowledge management

### Concepts Adopted

**Atomic Notes**:
- One concept per note
- Links between notes
- Similar to focused StoredAssets with cross-references

**Progressive Summarization**:
- Highlight important parts
- Summarize highlights
- Compress over time
- Similar to `/compact` and auto-summary aging

**Just-In-Time vs Just-In-Case**:
- Don't store everything "just in case"
- Surface relevant info "just in time"
- Similar to pinning relevant blocks, `/recall` for deep dives

---

## Operating System Memory Management

**Source**: Virtual memory, paging systems

### Concepts Adopted

**Paging**:
- Active pages in RAM (sliding window)
- Inactive pages swapped to disk (archival)
- Page faults bring data back (recall)

**Working Set**:
- Pages recently accessed (sliding window)
- Least recently used eviction (exchange boundaries)
- Working set size tuning (90k budget)

**Memory Mapped Files**:
- Files accessible like memory
- Similar to `ikigai://` URIs accessible like filesystem
- Unified interface, different backing store

---

## Information Retrieval Theory

**Source**: Search engines, document ranking

### Concepts Adopted

**Precision vs Recall Trade-off**:
- Pinned blocks: High precision (manually curated)
- Auto-summary: High recall (covers everything evicted)
- `/recall`: Full-text search (highest recall, lower precision)

**Query Expansion**:
- Auto-summary hints trigger broader searches
- "We discussed X" → agent suggests `/recall X`
- Similar to search engine "related searches"

**Relevance Feedback**:
- User pins/unpins blocks based on usefulness
- System learns what's valuable over time
- Similar to search result click feedback

---

## Claude's Extended Thinking

**Source**: Anthropic's extended thinking feature

### Concepts Adopted

**Explicit Reasoning Space**:
- Extended thinking: Dedicated space for model reasoning
- Auto-summary: Dedicated space for indexing
- Both partition context by purpose

**Cost-Benefit Analysis**:
- Extended thinking: Trade tokens for quality
- Structured Memory: Trade management complexity for capacity
- Both optimize total value, not just raw capacity

---

## Key Synthesis

Structured Memory doesn't invent new concepts. It synthesizes:

1. **Letta's self-editing memory blocks** (agent control)
2. **Cache hierarchies** (tiered storage)
3. **Database query planning** (statistics, explicit control)
4. **Git's history model** (complete record, working vs historical)
5. **Knowledge management** (atomic notes, progressive summarization)
6. **OS memory management** (paging, working sets)
7. **Information retrieval** (precision/recall, relevance)
8. **Extended thinking** (purpose-driven partitioning)

The innovation is **applying these patterns to LLM context windows** in a coherent system.

---

## Research Inspiration

**Papers that influenced design**:

- [MemGPT: Towards LLMs as Operating Systems](https://arxiv.org/abs/2310.08560)
  - Memory blocks, self-editing, OS analogy

- [Lost in the Middle: How Language Models Use Long Contexts](https://arxiv.org/abs/2307.03172)
  - Performance degradation, importance of structure

- [In-Context Retrieval-Augmented Language Models](https://arxiv.org/abs/2302.00083)
  - Limitations of pure RAG, need for hybrid

---

## Acknowledgments

Ideas evolve through conversation and experimentation. Structured Memory emerges from:

- Letta team's pioneering work on memory blocks
- Computer systems design (cache hierarchies, OS memory)
- Database engineering (query planning, statistics)
- Personal knowledge management communities
- Anthropic's research on long contexts and thinking

The synthesis is original. The components are well-established.

Standing on the shoulders of giants.
