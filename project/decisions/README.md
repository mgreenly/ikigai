# Architecture Decision Records

This directory contains architecture decision records (ADRs) documenting key design choices for the project.

## Core Technical Patterns

- [talloc-memory-management.md](talloc-memory-management.md) - Why talloc for Memory Management
- [mutex-based-logger.md](mutex-based-logger.md) - Why Mutex-Based Thread-Safe Logger
- [link-seams-mocking.md](link-seams-mocking.md) - Why Link Seams for External Library Mocking

## LLM Provider Integration

- [openai-api-format.md](openai-api-format.md) - Why Start with OpenAI API Format
- [superset-api-approach.md](superset-api-approach.md) - Why Superset API Approach

## Data Representation

- [json-yaml-projection.md](json-yaml-projection.md) - Why JSON-Based Projection with Dual Format Output

## Platform Components

- [fresh-webapp-framework.md](fresh-webapp-framework.md) - Why Fresh for Webapp Framework

---

Each ADR follows a consistent format:
- **Decision**: What was decided
- **Rationale**: Why this choice was made
- **Alternatives Considered**: Other options that were rejected
- **Trade-offs**: Pros and cons considered
