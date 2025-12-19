Update auto-generated skill files by analyzing the codebase.

**Usage:**
- `/update-skills` - Update all auto-generated skills

**Skills updated:**
- `source-code` - Map of all src/*.c files organized by functional area
- `makefile` - All make targets organized by category
- `database` - Table schema, related structs, and database functions
- `mocking` - All MOCKABLE wrapper functions from wrapper.h
- `errors` - All error codes from err_code_t enum

---

Update the following skills by spawning five subagents **in parallel**:

## Subagent 1: source-code skill

**Prompt:**
```
Analyze all .c files in src/ and update `.claude/library/source-code/SKILL.md`.

Instructions:
1. Use Glob to find all src/**/*.c files
2. Read the first ~50 lines of each file to understand its purpose
3. Group files by functional area (e.g., UI/Rendering, Input Handling, Database, OpenAI Client, Tools, etc.)
4. Write the skill file with:
   - # Source Code
   - ## Description (1 sentence)
   - ## Category Name (for each category)
   - Bullet list of files with 1-2 sentence descriptions

Format:
## Category Name

- `src/path/file.c` - 1-2 sentence description of what this file provides.

Include ALL .c files. Organize logically. Keep descriptions concise.
```

## Subagent 2: makefile skill

**Prompt:**
```
Analyze the Makefile and update `.claude/library/makefile/SKILL.md`.

Instructions:
1. Read the Makefile completely
2. Extract all .PHONY targets and their purposes
3. Group targets by category (Build, Testing, Quality, Distribution, etc.)
4. Write the skill file with:
   - # Makefile
   - ## Description (1 sentence)
   - ## Category Name (for each category)
   - Bullet list of targets with 1 sentence descriptions
   - ## Build Modes (table of BUILD= options)
   - ## Common Workflows (example commands)

Format:
## Category Name

- `make target` - 1 sentence description.

Include ALL targets. Keep descriptions actionable and concise.
```

## Subagent 3: database skill

**Prompt:**
```
Analyze the database layer and update `.claude/library/database/SKILL.md`.

Instructions:
1. Read all migrations/*.sql files to document the current schema
2. Read src/db/*.h files for struct definitions and function signatures
3. Read src/db/*.c files for implementation overview
4. Write the skill file with:
   - # Database
   - ## Description (1 sentence)
   - ## Schema (document each table with columns and constraints)
   - ## Key Types (structs from headers)
   - ## Connection API (init functions)
   - ## Session API (session CRUD)
   - ## Message API (message CRUD)
   - ## Replay API (conversation replay)
   - ## Migration System (how migrations work)
   - ## Testing Patterns (DB test utilities)
   - ## Key Files (table of relevant files)

Document the actual current schema from migrations, not assumptions.
Include function signatures for the public API.
```

## Subagent 4: mocking skill

**Prompt:**
```
Analyze wrapper.h and update the "Available Wrappers" section in `.claude/library/mocking/SKILL.md`.

Instructions:
1. Read `src/wrapper.h` completely
2. Find all MOCKABLE function declarations
3. Group by category (POSIX, talloc, yyjson, curl, libpq, etc.)
4. Update ONLY the "Available Wrappers" section in the skill file
5. Preserve all other sections (Philosophy, The MOCKABLE Pattern, Using Mocks, Adding New Wrappers, etc.)

Format for Available Wrappers section:
## Available Wrappers

### POSIX
| Wrapper | Wraps |
|---------|-------|
| `posix_open_()` | `open()` |

### Library Name
| Wrapper | Wraps |
|---------|-------|
| `wrapper_name_()` | `original_function()` |

Include ALL MOCKABLE functions from wrapper.h. Keep the rest of the skill file intact.
```

## Subagent 5: errors skill

**Prompt:**
```
Analyze error.h and update `.claude/library/errors/SKILL.md` to include all error codes.

Instructions:
1. Read `src/error.h` to find the `err_code_t` enum
2. Extract all error code names and their values
3. Add a new "## Error Codes" section after "## Core Types" listing all codes
4. Preserve all other sections of the skill file

Format for Error Codes section:
## Error Codes

| Code | Value | Usage |
|------|-------|-------|
| `ERR_NONE` | 0 | Success/no error |
| `ERR_IO` | 1 | I/O operations |
| ... | ... | ... |

Infer usage from the code name. Include ALL error codes from the enum.
Keep the rest of the skill file intact - only add/update the Error Codes section.
```

## Execution

Spawn all five subagents **in parallel** using the Task tool with `subagent_type=general-purpose`.

After all complete, report which skills were updated.
