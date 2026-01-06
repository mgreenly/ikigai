# Task: Paths Module Core Implementation

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** None

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Pre-Read

**Skills:**
(Baseline skills jj, errors, style, tdd are pre-loaded. Only list additional skills.)
- `/load naming` - For naming conventions and approved abbreviations
- `/load di` - For dependency injection patterns

**Plan:**
- `rel-08/plan/paths-module.md` - Complete paths module specification
- `project/install-directories.md` - Installation directory layout

**Source:**
- `src/config.c` - For tilde expansion pattern to migrate
- `src/client.c` - For current path usage pattern

## Libraries

Use only:
- talloc - For memory management
- Standard POSIX - For readlink, getenv, getcwd

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)

## Objective

Implement the core paths module that provides centralized path resolution for ikigai. The module detects installation type (development/user/system), resolves XDG directories, expands tildes, and provides install-appropriate paths for config, tools, and data directories.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_paths_init(TALLOC_CTX *ctx, ik_paths_t **out)` | Creates paths instance, detects install type, returns OK/ERR |
| `const char *ik_paths_get_config_dir(ik_paths_t *paths)` | Returns config directory path (never NULL) |
| `const char *ik_paths_get_data_dir(ik_paths_t *paths)` | Returns data directory path (never NULL) |
| `const char *ik_paths_get_libexec_dir(ik_paths_t *paths)` | Returns libexec directory path (never NULL) |
| `const char *ik_paths_get_tools_system_dir(ik_paths_t *paths)` | Returns system tools directory path (NULL if not applicable) |
| `const char *ik_paths_get_tools_user_dir(ik_paths_t *paths)` | Returns user tools directory path (never NULL) |
| `const char *ik_paths_get_tools_project_dir(ik_paths_t *paths)` | Returns project tools directory path (never NULL) |
| `res_t ik_paths_expand_tilde(TALLOC_CTX *ctx, const char *path, char **out)` | Expands ~ to $HOME, returns OK/ERR |

Structs to define:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_paths_t` | install_type, config_dir, data_dir, libexec_dir, tools_system_dir, tools_user_dir, tools_project_dir | Holds resolved paths for current install |

Enums to define:

| Enum | Values | Purpose |
|------|--------|---------|
| `ik_install_type_t` | INSTALL_TYPE_DEV, INSTALL_TYPE_USER, INSTALL_TYPE_SYSTEM | Installation type detection |

## Behaviors

### Install Type Detection

**Development install** (INSTALL_TYPE_DEV):
- Detected when: `IKIGAI_DEV` environment variable is set (any value)
- Binary location: Anywhere
- Config dir: `$IKIGAI_CONFIG_DIR` if set, else project root
- Data dir: `$IKIGAI_DATA_DIR` if set, else project root
- Libexec dir: `$IKIGAI_LIBEXEC_DIR` if set, else `libexec/ikigai` relative to project root
- Tools system dir: NULL (no system tools in dev mode)
- Tools user dir: `~/.ikigai/tools/`
- Tools project dir: `./.ikigai/tools/`

**User install** (INSTALL_TYPE_USER):
- Detected when: Binary is in `~/.local/bin/` or `$XDG_BIN_HOME/`
- Config dir: `$XDG_CONFIG_HOME/ikigai/` (default: `~/.config/ikigai/`)
- Data dir: `$XDG_DATA_HOME/ikigai/` (default: `~/.local/share/ikigai/`)
- Libexec dir: `~/.local/libexec/ikigai/`
- Tools system dir: NULL (no system tools in user install)
- Tools user dir: `~/.ikigai/tools/`
- Tools project dir: `./.ikigai/tools/`

**System install** (INSTALL_TYPE_SYSTEM):
- Detected when: Binary is in `/usr/bin/`, `/usr/local/bin/`, or any other location
- Prefix extraction: Binary in `/usr/local/bin/ikigai` → prefix is `/usr/local`
- Config dir: `$XDG_CONFIG_HOME/ikigai/` (default: `~/.config/ikigai/`)
- Data dir: `$XDG_DATA_HOME/ikigai/` (default: `~/.local/share/ikigai/`)
- Libexec dir: `<prefix>/libexec/ikigai/`
- Tools system dir: `<prefix>/libexec/ikigai/tools/`
- Tools user dir: `~/.ikigai/tools/`
- Tools project dir: `./.ikigai/tools/`

### Tilde Expansion

**Behavior:**
- Input `~/foo` → Output `$HOME/foo`
- Input `/absolute/path` → Output `/absolute/path` (unchanged)
- Input `relative/path` → Output `relative/path` (unchanged)
- Input `~` alone → Output `$HOME`
- Input `foo~/bar` → Output `foo~/bar` (tilde not at start, unchanged)

**Error handling:**
- If `$HOME` not set and tilde expansion needed: Return `ERR_IO` with message "HOME environment variable not set"
- If path is NULL: Return `ERR_INVALID_ARG`
- If allocation fails: PANIC

### Environment Variable Priority

**Override variables checked in order:**
1. `IKIGAI_DEV` - If set (any value), forces development mode
2. `IKIGAI_CONFIG_DIR` - Overrides config directory
3. `IKIGAI_DATA_DIR` - Overrides data directory
4. `IKIGAI_LIBEXEC_DIR` - Overrides libexec directory
5. XDG variables (`XDG_CONFIG_HOME`, `XDG_DATA_HOME`) - Used for user/system installs
6. `HOME` - Used for tilde expansion and XDG defaults

### Binary Location Discovery

Use `/proc/self/exe` on Linux to find binary path:
```c
char bin_path[PATH_MAX];
ssize_t len = readlink("/proc/self/exe", bin_path, sizeof(bin_path) - 1);
if (len == -1) {
    return ERR(ctx, ERR_IO, "Failed to read /proc/self/exe");
}
bin_path[len] = '\0';
```

Extract directory and prefix:
- `/usr/local/bin/ikigai` → bin_dir = `/usr/local/bin`, prefix = `/usr/local`
- `~/.local/bin/ikigai` → bin_dir = `~/.local/bin`, prefix = `~/.local`

### Memory Management

- All strings in `ik_paths_t` are talloc-allocated children of the paths instance
- Caller owns the paths instance (allocated on provided context)
- String getters return `const char *` - callers must NOT free them
- Freeing the paths instance frees all internal strings

## Test Implementation

**Follow TDD workflow (Red/Green/Verify):**

**Step 1 - Red (Failing Test):**

Create test file `tests/unit/paths/paths_test.c` with test cases:

1. `test_paths_init_development_mode` - Verify INSTALL_TYPE_DEV detection
2. `test_paths_init_user_install` - Verify INSTALL_TYPE_USER detection
3. `test_paths_init_system_install` - Verify INSTALL_TYPE_SYSTEM detection
4. `test_paths_get_config_dir` - Verify config dir resolution
5. `test_paths_get_data_dir` - Verify data dir resolution
6. `test_paths_get_libexec_dir` - Verify libexec dir resolution
7. `test_paths_get_tools_dirs` - Verify all three tool dirs
8. `test_paths_expand_tilde_home` - Verify `~/foo` → `$HOME/foo`
9. `test_paths_expand_tilde_alone` - Verify `~` → `$HOME`
10. `test_paths_expand_tilde_not_at_start` - Verify `foo~/bar` unchanged
11. `test_paths_expand_tilde_no_home` - Verify ERR_IO when HOME not set
12. `test_paths_expand_tilde_null_input` - Verify ERR_INVALID_ARG

Add function declarations to `src/paths.h`.

Add stub implementations to `src/paths.c` that compile but return minimal values:
```c
res_t ik_paths_init(TALLOC_CTX *ctx, ik_paths_t **out) {
    return OK(NULL);
}

const char *ik_paths_get_config_dir(ik_paths_t *paths) {
    return NULL;
}

// ... etc
```

Build and run: `make check`

Verify tests FAIL with assertion failures (NOT compilation errors).

**Step 2 - Green (Minimal Implementation):**

Implement each function to make tests pass:

1. Implement `ik_paths_init()`:
   - Check for `IKIGAI_DEV` environment variable
   - If not set, use readlink to get binary path
   - Detect install type based on binary location
   - Resolve all paths based on install type and environment variables
   - Allocate `ik_paths_t` struct on provided context
   - Store all resolved paths as talloc children
   - Return OK with paths instance

2. Implement getter functions:
   - Return corresponding field from paths struct
   - Assert paths != NULL
   - Return const char * (never NULL except tools_system_dir in dev/user mode)

3. Implement `ik_paths_expand_tilde()`:
   - Check for NULL input → ERR_INVALID_ARG
   - Check if path starts with `~/` or is exactly `~`
   - If yes, get HOME environment variable
   - If HOME not set → ERR_IO
   - Allocate new string with HOME prefix
   - Return OK with expanded path

STOP when all tests pass.

**Step 3 - Verify:**
- Run `make check` - all tests must pass
- Run `make lint` - complexity under threshold

## Completion

After completing work (whether success, partial, or failed), commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(paths-core.md): [success|partial|failed] - [brief description]

[Optional: Details about what was accomplished, failures, or remaining work]
EOF
)"
```

Report status to orchestration:
- Success: Task complete, all tests passing
- Partial/Failed: Describe what's incomplete or failing

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass (12+ test cases)
- [ ] `make check` passes
- [ ] All changes committed using commit message template
- [ ] Working copy is clean (no uncommitted changes)
