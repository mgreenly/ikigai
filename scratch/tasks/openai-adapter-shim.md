# Task: Create OpenAI Adapter Shim

**Phase:** 2 - Adapter Shim
**Depends on:** 01-provider-types, 02-shared-utilities

## Objective

Create a thin adapter that wraps existing `src/openai/` code behind the new vtable interface. This allows incremental migration without breaking existing functionality.

## Deliverables

1. Create `src/providers/openai/adapter_shim.c`
   - Implement `ik_openai_vtable` using existing `src/openai/` functions
   - `ik_openai_shim_send()` - Wraps existing client send
   - `ik_openai_shim_stream()` - Wraps existing streaming
   - Transform between internal format and existing OpenAI format

2. Create `src/providers/openai/openai.h`
   - `ik_openai_create()` - Factory function using shim

3. Update `src/providers/provider_common.c`
   - Add OpenAI case to `ik_provider_create()`

## Key Insight

The shim is temporary. It allows existing tests to pass while we build the abstraction. Will be replaced in Phase 6.

## Reference

- `scratch/plan/architecture.md` - Migration Strategy section

## Verification

- All existing OpenAI tests pass through shim
- Can create OpenAI provider via `ik_provider_create()`
