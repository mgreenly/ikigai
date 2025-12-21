# Task: Delete OpenAI Adapter Shim

**Layer:** 6
**Model:** sonnet/none
**Depends on:** cleanup-openai-source.md, tests-openai.md

## Pre-Read

**Skills:**
- `/load source-code` - Verify adapter usage

**Source:**
- `src/providers/openai/adapter_shim.c` - Temporary shim to delete
- `src/providers/openai/adapter_shim.h` - Header to delete
- `src/repl.c` - Verify using native provider, not shim

## Objective

Delete the temporary adapter shim that bridged old OpenAI code to the new provider interface. This shim was only needed during migration; now that native implementation is complete, it can be removed.

## Files to Delete

| File | Purpose |
|------|---------|
| `src/providers/openai/adapter_shim.c` | Temporary bridge to old client |
| `src/providers/openai/adapter_shim.h` | Shim header |

## Behaviors

**Step 1: Verify shim is not used**
```bash
# Must return empty - no code should include the shim
grep -r 'adapter_shim' src/ | grep -v 'src/providers/openai/adapter_shim'
grep -r 'ik_openai_shim' src/
```

**Step 2: Verify native provider is wired**
```bash
# Should show provider vtable registration, not shim
grep -n 'ik_openai_provider' src/providers/factory.c
grep -n 'ik_provider_create' src/repl.c
```

**Step 3: Delete shim files**
```bash
rm -f src/providers/openai/adapter_shim.c
rm -f src/providers/openai/adapter_shim.h
```

**Step 4: Update Makefile**

Remove from source list:
```
src/providers/openai/adapter_shim.c
```

**Step 5: Verify build and tests**
```bash
make clean && make all
make check
```

## Postconditions

- [ ] `grep -r 'adapter_shim' src/` returns empty
- [ ] `adapter_shim.c` and `adapter_shim.h` deleted
- [ ] Makefile updated
- [ ] `make all` succeeds
- [ ] `make check` passes
