## Objective

Build external tool discovery infrastructure and user commands. After completion,
ikigai discovers tools at startup, executes them via fork/exec with JSON protocol,
and provides /tool and /refresh commands.

Discovery scans three directories (using ik_paths_t from Phase 0):
- System: `ik_paths_get_tools_system_dir()` → `libexec/ikigai/` (dev) or `PREFIX/libexec/ikigai/` (installed)
- User: `ik_paths_get_tools_user_dir()` → `~/.ikigai/tools/`
- Project: `ik_paths_get_tools_project_dir()` → `$PWD/.ikigai/tools/`

Override precedence: Project > User > System. Missing/empty directories handled gracefully.

## Reference

- `rel-08/plan/architecture.md` - struct definitions, function signatures
- `rel-08/plan/tool-discovery-execution.md` - discovery protocol, execution flow

## Outcomes

1. `src/tool_registry.c/.h` - registry stores name, path, schema per tool
2. `src/tool_discovery.c/.h` - scans 3 dirs via ik_paths_t, spawns --schema, populates registry
3. `src/tool_external.c/.h` - fork/exec with JSON stdin/stdout, 30s timeout
4. `src/tool_wrapper.c/.h` - wraps results in tool_success/tool_failure envelope
5. `src/commands_tool.c/.h` - /tool lists tools, /tool NAME shows schema, /refresh reloads
6. `src/shared.h` has `tool_registry` field, `src/repl_init.c` calls discovery at startup
7. `src/repl_tool.c` uses registry lookup + external exec (replaces stub)

## Acceptance

- `make check` passes
- Discovery handles empty/missing directories gracefully
- Discovery finds executables and calls --schema
- `/tool` works (shows whatever tools exist)
- `/refresh` clears and repopulates registry
