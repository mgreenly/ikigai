# Task: Delete Legacy OpenAI Source Files

**Layer:** 5
**Model:** sonnet/none
**Depends on:** openai-core.md, openai-send-impl.md

## Pre-Read

**Skills:**
- `/load makefile` - Build system structure

**Source:**
- `src/openai/` - Legacy files to delete (list below)
- `src/providers/openai/` - New implementation (must exist and compile)
- `Makefile` - Build configuration to update

## Objective

Delete the 9 legacy source files in `src/openai/` after verifying the new provider implementation compiles and no code outside `src/providers/openai/` references them.

## Files to Delete

| File | Replacement |
|------|-------------|
| `src/openai/client.c` | `src/providers/openai/client.c` |
| `src/openai/client.h` | `src/providers/openai/client.h` |
| `src/openai/client_multi.c` | `src/providers/openai/adapter.c` |
| `src/openai/client_multi_callbacks.c` | `src/providers/openai/streaming.c` |
| `src/openai/client_multi_request.c` | `src/providers/openai/request.c` |
| `src/openai/client_msg.c` | `src/providers/openai/request.c` |
| `src/openai/client_serialize.c` | `src/providers/openai/request.c` |
| `src/openai/tool_choice.c` | `src/providers/common/tool_choice.c` |
| `src/openai/tool_choice.h` | `src/providers/common/tool_choice.h` |

**Note:** `http_handler.c` and `sse_parser.c` were already migrated to `src/providers/common/` in earlier tasks.

## Behaviors

**Step 1: Verify no external references**
```bash
# Must return empty (no references outside providers/openai/)
grep -r '#include.*"openai/' src/ | grep -v 'src/providers/openai/'
grep -r 'ik_openai_' src/ | grep -v 'src/providers/openai/'
```

**Step 2: Verify new implementation compiles**
```bash
make clean && make all
# Must succeed with no errors
```

**Step 3: Delete legacy files**
```bash
rm -f src/openai/client.c src/openai/client.h
rm -f src/openai/client_multi.c src/openai/client_multi_callbacks.c
rm -f src/openai/client_multi_request.c
rm -f src/openai/client_msg.c src/openai/client_serialize.c
rm -f src/openai/tool_choice.c src/openai/tool_choice.h
```

**Step 4: Update Makefile**

Remove these from `SRCS` or source list:
```
src/openai/client.c
src/openai/client_multi.c
src/openai/client_multi_callbacks.c
src/openai/client_multi_request.c
src/openai/client_msg.c
src/openai/client_serialize.c
src/openai/tool_choice.c
```

**Step 5: Verify build still works**
```bash
make clean && make all
# Must succeed
```

## Postconditions

- [ ] `grep -r '#include.*"openai/' src/ | grep -v 'src/providers/'` returns empty
- [ ] All 9 files listed above are deleted
- [ ] Makefile no longer references deleted files
- [ ] `make clean && make all` succeeds
- [ ] `src/openai/` directory is empty or only contains migrated files
