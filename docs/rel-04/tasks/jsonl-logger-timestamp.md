# Task: JSONL Logger ISO 8601 Timestamps

## Target
Infrastructure: JSONL logging system

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/naming.md

### Pre-read Source (patterns)
- src/logger.c (existing timestamp implementation)

### Pre-read Tests (patterns)
- tests/unit/logger/basic_test.c (existing logger tests)

## Pre-conditions
- `make check` passes
- Task `jsonl-logger-core.md` completed
- `ik_log_debug()` uses stub timestamp

## Task
Replace the stub timestamp with proper ISO 8601 format including milliseconds and local timezone offset.

Format: `2025-12-05T12:34:56.789-05:00`

Components:
- Date: YYYY-MM-DD
- Time: HH:MM:SS.mmm (milliseconds)
- Timezone: ±HH:MM offset from UTC

## TDD Cycle

### Red
1. Add test in `tests/unit/logger/jsonl_timestamp_test.c`:
   - Test timestamp format matches ISO 8601 regex pattern
   - Test timestamp includes milliseconds (3 digits)
   - Test timestamp includes timezone offset (±HH:MM)
   - Test timestamp is current time (within 1 second tolerance)
2. Run `make check` - expect failure (stub timestamp doesn't match format)

### Green
1. Add `ik_log_format_timestamp()` internal function in `src/logger.c`:
   - Use `gettimeofday()` for microsecond precision
   - Use `localtime_r()` for local timezone
   - Format as `YYYY-MM-DDTHH:MM:SS.mmm±HH:MM`
   - Calculate timezone offset from `tm.tm_gmtoff`
2. Update `ik_log_debug()` to call `ik_log_format_timestamp()`
3. Run `make check` - expect pass

### Refactor
1. Verify timestamp buffer sizes are safe (no overflow)
2. Ensure naming matches conventions
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Timestamps use ISO 8601 with milliseconds and local timezone
- 100% test coverage for timestamp formatting
