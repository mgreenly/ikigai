# Sub-Agent Completes

## Description

Sub-agent completes when it reaches a state where it would normally wait for human input. Its final response becomes the tool result for the parent.

## Transcript

Sub-agent 0/0 working:
```text
───────── ↑0/ ←0/0 [0/0] →0/0 ↓- ─────────

[System: You are a research assistant.]

Find the best sorting algorithm for nearly-sorted data.

For nearly-sorted data, the best algorithms are:

1. **Insertion Sort** - O(n) for nearly sorted, simple
2. **Timsort** - O(n) best case, used by Python/Java
3. **Adaptive merge sort** - Good for partially sorted

[Tool use: web_search]
  query: "timsort performance nearly sorted data"

[Tool result]: "Timsort achieves O(n) on nearly sorted..."

Based on my research, **Timsort** is the recommended choice because:
- O(n) performance on nearly-sorted data
- Stable sort (preserves equal element order)
- Battle-tested in Python and Java standard libraries

For implementation, use your language's built-in sort (likely Timsort).

[Sub-agent would wait for human input here → COMPLETE]
```

Parent receives:
```text
[Tool result from 0/0]:
"Based on my research, **Timsort** is the recommended choice because:
- O(n) performance on nearly-sorted data
- Stable sort (preserves equal element order)
- Battle-tested in Python and Java standard libraries

For implementation, use your language's built-in sort (likely Timsort)."
```

## Walkthrough

1. Sub-agent 0/0 starts with research task

2. Sub-agent sends prompt to LLM, receives response

3. Response includes tool call (web_search)

4. Sub-agent executes tool, feeds result back to LLM

5. LLM produces final response (no tool call)

6. Sub-agent is now in state: response ready, would wait for human

7. Completion detector recognizes this state

8. Final assistant message extracted as result

9. Result packaged as tool response for parent

10. Sub-agent cleaned up (removed from tree, resources freed)

11. Parent exits EXECUTING_TOOL state

12. Parent's conversation continues with tool result
