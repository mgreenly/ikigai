# Task: NO_COLOR and TERM Detection

## Target
Infrastructure: ANSI color support (Phase 4 - Configuration)

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/di.md

### Pre-read Docs
- docs/ansi-color.md

### Pre-read Source (patterns)
- src/ansi.h (color constants)
- src/ansi.c (color functions)
- src/config.h (config struct pattern)

### Pre-read Tests (patterns)
- tests/unit/ansi/skip_test.c
- tests/unit/config/config_test.c

## Pre-conditions
- `make check` passes
- ANSI color constants and functions exist
- Width calculation and input stripping tasks completed

## Task
Add color enablement detection based on environment variables. Colors should be disabled when:
1. `NO_COLOR` environment variable is set (any value) - https://no-color.org/
2. `TERM` environment variable is "dumb"

Add a function to check if colors are enabled and a global state that can be initialized at startup.

API additions to ansi.h:
```c
// Initialize color state from environment
// Call once at startup
void ik_ansi_init(void);

// Check if colors are enabled
// Returns true if colors should be used
bool ik_ansi_colors_enabled(void);
```

## TDD Cycle

### Red
1. Update `src/ansi.h`:
   - Add `void ik_ansi_init(void);`
   - Add `bool ik_ansi_colors_enabled(void);`
2. Create `tests/unit/ansi/config_test.c`:
   - Test `ik_ansi_colors_enabled()` returns true by default (no env vars)
   - Test `ik_ansi_colors_enabled()` returns false when NO_COLOR is set
   - Test `ik_ansi_colors_enabled()` returns false when NO_COLOR="" (empty)
   - Test `ik_ansi_colors_enabled()` returns false when TERM=dumb
   - Test `ik_ansi_colors_enabled()` returns true when TERM=xterm-256color
   - Test NO_COLOR takes precedence over TERM
3. Add stubs returning true
4. Run `make check` - expect test failures

### Green
1. Add static state variable in ansi.c:
   ```c
   static bool g_colors_enabled = true;
   static bool g_initialized = false;
   ```
2. Implement `ik_ansi_init()`:
   - Check `getenv("NO_COLOR")` - if not NULL, disable colors
   - Check `getenv("TERM")` - if equals "dumb", disable colors
   - Set `g_initialized = true`
3. Implement `ik_ansi_colors_enabled()`:
   - Return `g_colors_enabled`
4. Note: Tests will need to use setenv/unsetenv and call ik_ansi_init() to test different configurations
5. Run `make check` - expect pass

### Refactor
1. Consider adding helper to reset state for testing (internal use only)
2. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `ik_ansi_init()` detects NO_COLOR and TERM=dumb
- `ik_ansi_colors_enabled()` returns correct state
- 100% test coverage for new code
