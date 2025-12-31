# Structured Memory

**Status:** Design discussion, not yet implemented

## Overview

Structured Memory is ikigai's approach to context window management. Rather than dumping messages randomly into available context, it partitions the 200k token budget by purpose, with each partition optimized for different characteristics and access patterns.

## The Problem

Context windows are finite. A 200k token limit sounds generous until you:
- Work on a project for weeks
- Reference multiple documentation sources
- Need persistent knowledge across sessions
- Want conversation history without losing old context

Traditional approaches fail:
- **Send everything**: Hits limits quickly, performance degrades
- **RAG only**: Passive retrieval, no learning, query lag
- **Manual pruning**: Tedious, error-prone, cognitive overhead

Structured Memory solves this through **purpose-driven allocation**.

## Architecture

Structured Memory divides the 200k context budget into four layers:

```
┌─────────────────────────────────────────────────────────┐
│ Pinned Blocks (100k) - Curated persistent knowledge     │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ Auto-Summary (10k) - Index to archival memory           │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ Sliding Window (90k) - Recent conversation              │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ Archival (unlimited) - Everything forever, searchable   │
└─────────────────────────────────────────────────────────┘
```

Each layer has distinct characteristics, management strategies, and value optimization approaches.

## Documentation

### Core Concepts

- **[layers.md](layers.md)** - Detailed description of each memory layer
- **[budget-allocation.md](budget-allocation.md)** - Token economics and allocation strategies
- **[operations.md](operations.md)** - Commands and workflows for memory management
- **[tools.md](tools.md)** - Unified tool interface with URI routing

### Implementation

- **[background-agents.md](background-agents.md)** - Auto-summary maintenance
- **[exchange-boundaries.md](exchange-boundaries.md)** - Atomic eviction units
- **[storage.md](storage.md)** - Database schema and persistence

### Context

- **[comparison.md](comparison.md)** - vs RAG, vs traditional context windows
- **[inspiration.md](inspiration.md)** - Letta and influences

## Key Principles

**Purpose-driven allocation**: Each layer serves a specific purpose with appropriate budget
**Active management**: User and agent collaborate to curate what's valuable
**Graceful degradation**: Information flows: conversation → blocks → summary → archival
**Unified interface**: Same tools work on filesystem and StoredAssets (ikigai:// URIs)
**Zero loss**: Everything stored forever in archival, searchable on-demand

## Quick Start (Future)

```bash
# Pin a StoredAsset (always visible)
/pin skills/ddd

# Extract learning from conversation
/remember error handling patterns

# Compact a block to free space
/compact blocks/project-decisions

# Search archival memory
/recall authentication discussion

# Swap blocks as focus changes
/unpin skills/research
/pin blocks/current-task
```

## Related Documents

- [stored-assets.md](../stored-assets.md) - ikigai:// URI scheme and database storage
- [context-management.md](../context-management.md) - /forget, /mark, /rewind commands
- [autonomous-agents.md](../autonomous-agents.md) - Background agents for summary maintenance

---

Structured Memory transforms context windows from a constraint into a managed resource.
