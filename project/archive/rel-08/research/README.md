# External Tool Architecture Research

Research conducted to validate and inform the rel-08 external tool architecture design.

## Research Areas

### External Process Execution

**[external-process-execution.md](external-process-execution.md)** - How external tools integrate with ikigai:
- Integration with existing thread model
- Bidirectional pipe pattern (fork/exec)
- Timeout handling
- Alternative approaches considered

### Testing Strategies

**[testing-strategies.md](testing-strategies.md)** - How to test external tool systems:
- Unit testing subprocess spawning
- Integration testing async operations
- Mocking and stubbing approaches
- C testing frameworks (CMocka, Unity)

### Token Efficiency

**[token-efficiency.md](token-efficiency.md)** - External tools at zero overhead:
- Why bash-wrapped tools have overhead
- ikigai's protocol makes external tools identical to built-in
- Extensibility without penalty

### Skills + Tools Model

**[skill-tool-model.md](skill-tool-model.md)** - Extension architecture beyond rel-08:
- Skills = context (when/why), Tools = action (what)
- We control both ends - no vendor context flooding
- MCP only useful for unplanned/ad-hoc situations
- Direct APIs beat vendor abstractions

### Error Message Examples

**[error-message-examples.md](error-message-examples.md)** - Example LLM conversations showing error handling:
- Missing credentials flow
- Invalid credentials, rate limits
- How LLM relays helpful error messages to users

### LLM Interaction Examples

**[llm-interaction-examples.md](llm-interaction-examples.md)** - Example conversations showing tool usage:
- Tool calls and structured responses
- Error handling and adaptation
- Output truncation handling

## Key Findings Summary

### 1. Thread Model Integration

External tools plug into ikigai's existing thread-based tool execution. The thread calls fork/exec with pipes instead of internal C dispatch. No changes to main loop, select(), or completion polling.

**Verdict:** Minimal change. Existing architecture already solved async tool execution.

### 2. Bidirectional Pipe Pattern

External tools need stdin (JSON params) and stdout (JSON result). Standard fork/exec with pipe() handles this. Blocking in thread is fine - that's what threads are for.

**Verdict:** Simple Unix pattern. fork() overhead negligible at tool-call frequency.

### 3. Extensibility Without Penalty

External tools via ikigai's protocol are indistinguishable from built-in tools. Other agents have a two-tier system (efficient built-ins, overhead for bash-wrapped externals). ikigai has one tier.

**Verdict:** Any capability can be added as a first-class tool. No reason to build things into ikigai when an external tool works equally well.

### 4. Testing Challenges

C lacks mature subprocess mocking frameworks. Best approach: wrapper pattern around system calls (`fork`, `exec`, `pipe`) with function pointers for test injection.

**Verdict:** Implement wrapper layer in `src/wrapper.c` (already exists!) for testability.

### 5. Build System Patterns

GNU standards place helper executables in `$(libexecdir)/$(package)` (typically `/usr/local/libexec/ikigai`). Tools are NOT in user's PATH.

**Verdict:** Follow FHS. Use `PREFIX/libexec/ikigai/` for system tools.

## Research Methodology

Primary research method: Analyze existing ikigai codebase to understand current patterns, then design minimal changes for external tool support.

## Validation Status

| Design Decision | Research Status | Validation |
|-----------------|-----------------|------------|
| Thread + blocking pipes | ✓ Researched | Matches existing tool execution, minimal change |
| fork/exec (not posix_spawn) | ✓ Researched | Simpler, overhead negligible at our scale |
| JSON I/O protocol | ✓ Researched | Simple Unix pattern (stdin/stdout) |
| Schema via --schema flag | ✓ Researched | Common pattern (like --help, --version) |
| Response wrapper | ✓ Researched | Distinguishes tool errors from execution errors |
| Wrapper for testability | ✓ Researched | Best practice for C testing |

## Next Steps

Research findings inform:
1. **User stories** - How tool discovery and execution work
2. **Task breakdown** - Specific modules to implement with confidence
3. **Test strategy** - Concrete approaches for unit/integration tests
4. **Build system** - Standard-compliant installation paths
