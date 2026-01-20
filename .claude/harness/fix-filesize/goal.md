## Objective

All source files must be under the 16KB (16384 bytes) filesize limit.

## Strategy

1. Run `check-filesize` to identify oversized files
2. For each oversized file:
   - Use `check-filesize --file=<path>` to see details for that file
   - **Analyze the file to identify logical boundaries for splitting**
     - Group related functions by purpose/domain
     - Look for cohesive subsystems that can be extracted
     - Consider natural module boundaries (parsing, validation, formatting, etc.)
   - **Extract cohesive modules into separate files**
     - Create new .c files with clear, descriptive names
     - Create corresponding .h headers if needed
     - Move logically related functions together
   - **Update build system**
     - Add new .c files to Makefile in appropriate targets
     - Verify dependency chains are correct
     - Ensure linking works correctly
   - **Update any tests that reference the original file**
     - Check if test files include headers from the split files
     - Update test includes if module structure changed
     - Verify test still compiles and links
   - **Verify compilation after changes**
     - Run `make fmt` to format all code (validation happens on formatted code)
     - Run `check-compile` to ensure `{"ok": true}`
     - Fix any compilation or linking errors
   - **Maintain existing functionality and test coverage**
     - Ensure ownership (talloc) patterns remain correct
     - Preserve error handling patterns
     - Keep existing API contracts intact
3. Re-run filesize check after each fix to verify progress

## Refactoring Guidelines

- **Split along functional boundaries, not arbitrary line counts**
  - Extract helper utilities (string processing, array operations, etc.)
  - Separate parsing logic from business logic
  - Isolate formatting/rendering functions
  - Group I/O operations together
- **Keep related functions together**
  - All array operations in one module
  - All validation functions together
  - All formatters/renderers together
- **Prefer extracting helper modules over arbitrary splits**
  - Create utility modules with clear purposes
  - Don't split mid-function or mid-logical-unit
- **Update headers and includes appropriately**
  - Create new headers for extracted modules
  - Update include paths in affected files
  - Maintain clean header dependencies
- **Maintain existing API contracts**
  - Don't change public function signatures
  - Keep existing behavior identical

## Hints

- For talloc/ownership patterns: `/load memory`
- For Result type patterns: `/load errors`
- For naming conventions: `/load style`
- For understanding module structure: `/load source-code`
- For Makefile targets and patterns: `/load makefile`

## Validation

**IMPORTANT:** Trust the harness script outputs. Do not re-implement checking logic. The scripts return JSON with `{"ok": true}` or `{"ok": false, "items": [...]}` - trust these results.

**Validation sequence (must run in this exact order):**
1. **First: Format** - Run `make fmt` to format all code. Ignore the output - this is just an action to format the code. Wait for it to complete before moving to the next step.
2. **Then: Compile check** - Run `check-compile` and verify it returns `{"ok": true}`
3. **Finally: Filesize check** - Run `check-filesize` and verify it returns `{"ok": true}`

## Acceptance

DONE when all validation steps pass in order and both checks return `{"ok": true}`.
