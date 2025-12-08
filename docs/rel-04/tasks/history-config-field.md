# Task: Add history_size Config Field

## Target
Feature: Command History - Configuration

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md

### Pre-read Docs
- docs/error_handling.md
- docs/return_values.md

### Pre-read Source (patterns)
- src/config.h (ik_cfg_t structure)
- src/config.c (ik_cfg_load implementation, JSON parsing patterns)

### Pre-read Tests (patterns)
- tests/unit/config/load_test.c (config loading test patterns)

## Pre-conditions
- `make check` passes
- Config loading works for existing fields
- yyjson library is integrated

## Task
Add `history_size` field to configuration structure and JSON loading logic. This field controls the maximum number of history entries to retain (default: 10000).

The field should be optional in config.json. If not present, use the default value of 10000.

## TDD Cycle

### Red
1. Add test to `tests/unit/config/load_test.c`:
   ```c
   START_TEST(test_config_history_size_default)
   {
       // Config JSON without history_size field
       const char *json = "{"
           "\"openai_api_key\": \"test-key\","
           "\"openai_model\": \"gpt-4\""
       "}";
       // Write to temp file, load, verify history_size == 10000
   }
   END_TEST

   START_TEST(test_config_history_size_custom)
   {
       // Config JSON with custom history_size
       const char *json = "{"
           "\"openai_api_key\": \"test-key\","
           "\"history_size\": 5000"
       "}";
       // Verify history_size == 5000
   }
   END_TEST
   ```
2. Run `make check` - expect compilation failure (field doesn't exist)

### Green
1. Add field to `src/config.h`:
   ```c
   typedef struct {
       // ... existing fields ...
       int32_t history_size;
   } ik_cfg_t;
   ```
2. In `src/config.c`, add loading logic in `ik_cfg_load()`:
   - After loading other fields, read "history_size" from JSON
   - If field exists, use value (validate it's positive)
   - If field missing, set to 10000
3. Run `make check` - expect pass

### Refactor
1. Consider: Should there be a maximum limit for history_size?
2. Ensure error handling for negative/zero values
3. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- `ik_cfg_t` has `history_size` field
- Default value is 10000
- Custom values load correctly from JSON
- Invalid values (negative, zero) are rejected with error
- 100% test coverage maintained
