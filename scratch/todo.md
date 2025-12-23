# Plan vs Tasks Alignment - Index

Status: Split into 6 work areas

## Recommended Order

The numbering **does** reflect a logical execution order based on dependencies:

| # | Area | Why This Order | Items |
|---|------|----------------|-------|
| 1 | Architecture | Foundation - types, patterns, vtable | 4 |
| 2 | Provider Types | Depends on 1 - implements vtable per provider | 4 |
| 3 | Data Formats | Depends on 1,2 - request/response structures | 3 |
| 4 | Application | Depends on 1-3 - commands, config, REPL | 3 |
| 5 | Testing | Depends on 1-4 - tests the implementations | 6 |
| 6 | Cleanup | Last - only after everything works | 4 |

**Total: 24 items**

## Work Area Files

- [todo-1-architecture.md](todo-1-architecture.md) - vtable, request builder, provider lifecycle
- [todo-2-provider-types.md](todo-2-provider-types.md) - streaming tasks, thought signatures
- [todo-3-data-formats.md](todo-3-data-formats.md) - provider_data, rate limits, types
- [todo-4-application.md](todo-4-application.md) - model command, error handling
- [todo-5-testing.md](todo-5-testing.md) - contract tests, coverage, benchmarks
- [todo-6-cleanup.md](todo-6-cleanup.md) - Phase 2 removal preparation

## Priority Within Each Area

Each file has items marked with priority:
- **Critical** - blocks implementation, do first
- **High** - affects correctness
- **Medium** - improves quality
- **Low** - documentation, nice to have

## Cross-Area Dependencies

Some items connect across areas:
- 2.1 (Google thought signatures) + 3.1 (provider_data) - same feature
- 1.2 (thinking validation) + 5.2 (thinking tests) - implementation + test
- 6.* (cleanup) depends on 5.* (testing) passing
