# Goal: Standardize Tool Binary Naming Convention

## Objective

Rename tool binaries to follow consistent `*-tool` naming convention (all lowercase, words separated by hyphens, ending with `-tool` suffix), and update the tool discovery layer to strip the suffix when registering tools, ensuring users and LLMs never see the `-tool` suffix in tool listings or API responses.

## Outcomes

All six tool binaries follow the standard naming pattern:
- `bash-tool` (not `bash_tool`)
- `file-read-tool` (not `file-read`)
- `file-write-tool` (not `file-write`)
- `file-edit-tool` (not `file-edit`)
- `glob-tool` (not `glob`)
- `grep-tool` (not `grep`)

Tool schemas continue to output canonical names using underscore format without suffix:
- `bash`, `file_read`, `file_write`, `file_edit`, `glob`, `grep`

The tool discovery layer (`src/tool_discovery.c`) automatically maps filesystem names to schema names by stripping `-tool` suffix and converting hyphens to underscores for registry lookup.

## Acceptance

**1. Schema Output Verification**
- `bash-tool --schema | jq -r .name` outputs `bash` (not `bash_tool`)
- `file-read-tool --schema | jq -r .name` outputs `file_read`
- `file-write-tool --schema | jq -r .name` outputs `file_write`
- `file-edit-tool --schema | jq -r .name` outputs `file_edit`
- `glob-tool --schema | jq -r .name` outputs `glob`
- `grep-tool --schema | jq -r .name` outputs `grep`

**2. Binary Naming**
- All six binaries exist in `libexec/ikigai/` with `-tool` suffix and hyphenated names
- No binaries with underscore format (e.g., `bash_tool`) or missing suffix (e.g., `grep`) remain

**3. Tool Discovery Mapping**
- `extract_tool_name()` in `src/tool_discovery.c` strips `-tool` suffix from binary names
- `extract_tool_name()` converts remaining hyphens to underscores (e.g., `file-edit` → `file_edit`)
- Registry lookups succeed using the transformed names

**4. Build System**
- `make tools` builds all six binaries with correct names
- Makefile targets updated to produce hyphenated `-tool` binaries (verify target names align with output)
- All object files and dependency files use correct paths

**5. Test Integration**
- All references to old binary names (especially `bash_tool`) updated in test scripts
- `make test-integration` passes with all tests using new binary paths
- Integration tests in `tests/integration/` use correct binary names

**6. End-to-End Verification**
- `/tool` command (or equivalent tool listing) shows canonical names without `-tool` suffix
- Tool invocation via registry succeeds for all six tools using underscore names
- Error messages reference canonical tool names (e.g., "bash") not binary names (e.g., "bash-tool")

## Reference

- **Current Implementation:** `src/tool_discovery.c` lines 104-116 - `extract_tool_name()` currently returns basename as-is
- **Tool Schemas:** `src/tools/*/main.c` - Each tool's main.c contains schema output logic
- **Binary Locations:** `libexec/ikigai/` - All tool binaries installed here
- **Build System:** `Makefile` - Tool build targets and installation rules
- **Integration Tests:** `tests/integration/*_test.sh` - Test scripts that reference binary paths

## Implementation Notes

**Discovery Layer Changes:**
- Update `extract_tool_name()` to strip `-tool` suffix from basename
- Add hyphen-to-underscore conversion (e.g., `file-edit` → `file_edit`)
- Preserve existing behavior for schema name extraction from JSON

**Schema Updates:**
- Verify `bash` tool schema outputs `bash` not `bash_tool` (may need update)
- Confirm other five tools already output correct underscore names without suffix

**Binary Renaming:**
- Rename `bash_tool` → `bash-tool`
- Rename `file-read` → `file-read-tool`
- Rename `file-write` → `file-write-tool`
- Rename `file-edit` → `file-edit-tool`
- Rename `glob` → `glob-tool`
- Rename `grep` → `grep-tool`

**Build System Updates:**
- Update Makefile targets to produce hyphenated `-tool` binaries
- Ensure object file paths align with new binary names
- Verify `make clean` removes all artifacts with new names

**Test Updates:**
- Search for all references to old binary names (especially `bash_tool`)
- Update hardcoded paths in integration tests
- Verify test scripts use correct binary names

**Verification Sequence:**
1. Update schemas to output canonical names without suffix
2. Update `extract_tool_name()` to implement mapping logic
3. Rename binaries and update Makefile
4. Update all test references
5. Run `make tools && make test-integration` to verify
