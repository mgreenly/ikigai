# Goal: Complete Missing External Tools

## Objective

Implement the five remaining external tools (file_read, file_write, file_edit, glob, grep) following the established bash_tool pattern to complete the rel-08 external tool system.

## Outcomes

Five standalone tool binaries that:
- Follow bash_tool's architectural pattern (directory structure, shared dependencies, JSON I/O protocol)
- Match specifications in `rel-08/plan/tool-specifications.md` for schemas, behavior, and error handling
- Build successfully via Makefile targets
- Execute reliably without memory leaks
- Return spec-compliant output for both success and error cases

## Acceptance

**Prerequisites verified:**
- All TOOL_COMMON_SRCS exist: `src/error.c`, `src/panic.c`, `src/json_allocator.c`, `src/vendor/yyjson/yyjson.c`

**For each of the five tools (file_read, file_write, file_edit, glob, grep):**

1. **Build Integration**
   - Source file exists: `src/tools/<tool_name>/main.c`
   - Binary builds: `libexec/ikigai/<tool-name>` (hyphenated)
   - Makefile target works: `make tool-<tool-name>`

2. **Schema Compliance**
   - `<tool> --schema` outputs valid JSON matching `rel-08/plan/tool-specifications.md`
   - Schema includes correct name (underscore format), description, and parameters

3. **Functional Correctness**
   - Core success case works (e.g., file_read reads content, grep finds matches, glob returns files)
   - Critical error cases handled correctly:
     - Missing required parameters → error response
     - File not found (file_read, file_edit) → FILE_NOT_FOUND error
     - Permission denied → PERMISSION_DENIED error
     - Data integrity errors (file_edit non-unique string, file_write failures) → appropriate errors
   - Output format matches specifications exactly (error messages, success messages, JSON structure)

4. **Memory Safety**
   - Tools run clean under memory analysis (no leaks, no invalid access)
   - Proper cleanup on both success and error paths

**Build system:**
- `make tools` builds all six tools (including existing bash)
- All tool binaries appear in `libexec/ikigai/` directory

## Reference

- **Specifications:** `rel-08/plan/tool-specifications.md` - Complete schemas, behavior, error cases for all tools
- **Architecture:** `rel-08/plan/architecture.md` - Integration patterns, naming conventions, build system
- **Example implementation:** `src/tools/bash/main.c` - Reference pattern for structure and approach
- **Build patterns:** `rel-08/plan/tool-specifications.md` lines 709-800 - Makefile targets and shared sources

## Notes

- Tools are standalone executables with no dependency on paths infrastructure (Phase 0)
- Each tool follows bash_tool's pattern: talloc memory management, yyjson for JSON I/O, simple main() structure
- Tools receive file paths as JSON parameters, not from configuration
- Error handling is critical - tools must fail safely and provide clear error messages
- Comprehensive integration testing happens in later phases; this phase focuses on working, correct standalone tools
