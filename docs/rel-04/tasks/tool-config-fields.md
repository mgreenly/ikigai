# Task: Add Tool Configuration Fields

## Target
User story: 02-single-glob-call (prerequisite for tool execution)

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/quality.md
- .agents/skills/coverage.md

### Pre-read Docs
- docs/naming.md
- docs/error_handling.md
- rel-04/README.md (Configuration section for field definitions)

### Pre-read Source (patterns)
- src/config.h (ik_cfg_t struct definition)
- src/config.c (create_default_config and ik_cfg_load patterns)

### Pre-read Tests (patterns)
- tests/unit/config/config_test.c (config loading tests with test fixtures)

## Pre-conditions
- `make check` passes
- `ik_cfg_t` struct exists with existing fields (openai_*, listen_*, db_*)
- `ik_cfg_load()` validates required fields and returns ERR() on missing/invalid

## Task
Add two new configuration fields to `ik_cfg_t` for tool execution limits:

1. `max_tool_turns` (int32_t, default: 50)
   - Maximum tool call iterations per user request
   - Prevents infinite loops when model makes repeated tool calls
   - Valid range: 1-1000

2. `max_output_size` (int64_t, default: 1048576 / 1MB)
   - Maximum bytes for tool output before truncation
   - Valid range: 1024-104857600 (1KB-100MB)

Follow the existing config pattern:
- Add fields to `ik_cfg_t` struct
- Add to `create_default_config()` with defaults
- Add validation in `ik_cfg_load()` (required, type check, range check)
- Error if fields missing from existing config files

## TDD Cycle

### Red
1. Add tests in `tests/unit/config/config_test.c`:
   - Test loading config with valid `max_tool_turns` and `max_output_size`
   - Test error when `max_tool_turns` missing
   - Test error when `max_output_size` missing
   - Test error when `max_tool_turns` out of range (0, 1001)
   - Test error when `max_output_size` out of range (1023, 104857601)
   - Test default config creation includes both fields
2. Create test config fixtures in `tests/fixtures/` as needed
3. Add fields to `ik_cfg_t` struct in `src/config.h`
4. Add stub validation in `ik_cfg_load()` that always returns ERR()
5. Run `make check` - expect assertion failures (tests expect success for valid config)

### Green
1. Update `create_default_config()` to include:
   - `max_tool_turns`: 50
   - `max_output_size`: 1048576
2. Update `ik_cfg_load()` to:
   - Extract `max_tool_turns` from JSON
   - Validate type is int, range 1-1000
   - Extract `max_output_size` from JSON
   - Validate type is int, range 1024-104857600
   - Copy values to `cfg->max_tool_turns` and `cfg->max_output_size`
3. Run `make check` - expect pass

### Refactor
1. Ensure error messages match existing pattern ("Missing X", "Invalid type for X", "X must be A-B, got C")
2. Verify naming follows docs/naming.md
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_cfg_t` has `max_tool_turns` and `max_output_size` fields
- `create_default_config()` writes both fields with defaults
- `ik_cfg_load()` validates both fields, errors on missing/invalid
- 100% test coverage for new code

## Notes
Existing production config files will fail to load after this change until users add the new fields. This is intentional - the error message tells them what's missing.
