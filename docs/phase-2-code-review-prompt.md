# Phase 2 Code Review Prompt

**Context**: Phase 2 (Interactive REPL) is feature-complete with all bugs fixed and quality gates passing. Final code review needed before Phase 2 completion.

## Task: Comprehensive Code Review

Review Phase 2 implementation for production readiness:

### Review Areas

1. **Security Issues**
   - Command injection vulnerabilities
   - Buffer overflows
   - Input validation gaps
   - Integer overflows
   - Format string vulnerabilities

2. **Memory Management**
   - Talloc hierarchy correctness
   - Memory leak potential
   - Use-after-free risks
   - Double-free risks
   - Proper parent-child relationships

3. **Error Handling**
   - All error paths covered
   - Resource cleanup on errors
   - Error propagation correctness
   - Assertion usage appropriate

4. **Code Quality**
   - Style consistency
   - Function complexity
   - Code duplication
   - Documentation completeness
   - Edge case handling

### Files to Review

**Core REPL (src/repl.c, src/repl.h)**:
- Event loop: `ik_repl_run()` (main event loop with read/parse/process/render)
- Rendering: `ik_repl_render_frame()`
- Actions: `ik_repl_process_action()`

**Workspace (src/workspace.c, src/workspace_multiline.c)**:
- Text editing operations
- Multi-line navigation (cursor_up/down, column preservation)
- Readline shortcuts (Ctrl+A/E/K/U/W)
- Focus: `ik_workspace_delete_word_backward()` (recently refactored for punctuation)

**Input Parsing (src/input.c)**:
- Byte stream parsing
- UTF-8 handling
- Escape sequence handling

**Terminal (src/terminal.c)**:
- Raw mode setup/cleanup
- Terminal state management

**Entry Point (src/client.c)**:
- Main function (32 lines, coordination only)

### Current Quality Status

**Coverage**: 100% (1315 lines, 105 functions, 525 branches)

**Quality Gates**: ALL PASSING ✅
- ✅ `make check` - All tests pass
- ✅ `make lint` - Complexity/file size checks pass
- ✅ `make coverage` - 100% branch coverage
- ✅ `make check-dynamic` - ASan, UBSan, TSan pass

**Recent Bug Fixes** (verify fixes are secure):
1. Empty workspace crashes - Added NULL checks (commit 9b32cff)
2. Column preservation - Added target_column field (commit 3c226d3)
3. Ctrl+W punctuation - Character class system (commits 3c226d3, 4f38c6b)

### Output Format

For each issue found, provide:
```
**[SEVERITY]** File:Line - Issue Title
- Description: What the issue is
- Impact: Security/Memory/Crash risk
- Recommendation: How to fix
```

Severity levels: CRITICAL, HIGH, MEDIUM, LOW, INFO

### Success Criteria

- No CRITICAL or HIGH severity issues
- All MEDIUM issues documented with mitigation plans
- Code meets production quality standards

### Commands to Run

```bash
cd /home/claude/projects/ikigai/main

# Review key files
Read src/repl.c
Read src/workspace.c
Read src/workspace_multiline.c
Read src/input.c
Read src/terminal.c
Read src/client.c

# Check for common issues
grep -r "strcpy\|strcat\|sprintf\|gets" src/
grep -r "malloc\|free" src/  # Should use talloc only
grep -r "TODO\|FIXME\|XXX\|HACK" src/

# Review test coverage
cat coverage/summary.txt
```

Start with the highest-risk areas first: input parsing, memory management, and the recently changed Ctrl+W implementation.
