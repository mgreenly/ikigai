# Fixed Issues

- `verify-providers.md` ordering in `order.json` is correct (moved to position 58, after all test creation tasks)
- `ik_provider_completion_t` type name avoids collision with existing `ik_http_completion_t` in `src/openai/client_multi.h`
- Callback type standardized to `ik_provider_completion_cb_t` across all task and plan files
- `ERR_AGENT_NOT_FOUND` case added to `error_code_str()` switch in `src/error.h`
- `verify-foundation.md` declares dependencies on all credential test tasks
- `order.json` structure is valid (no duplicates, all files exist, valid model/thinking values)
- All `/load` skill references in Pre-Read sections point to existing skills
- All `scratch/plan/*.md` references in Pre-Read sections point to existing files
