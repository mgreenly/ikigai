---
description: Condense a file by ~50% while preserving all facts.
---

Compact the file at `$ARGUMENTS` to reduce token usage while preserving information.

**Requirements:**
1. Read the entire file content
2. Rewrite to reduce size by approximately 50%
3. Preserve ALL facts, specifications, code examples, and technical details
4. Techniques to use:
   - Remove redundant explanations
   - Consolidate repeated concepts
   - Use terse phrasing (no filler words)
   - Convert prose to bullet points or tables where appropriate
   - Remove unnecessary examples if one example suffices
   - Eliminate hedging language ("might", "could potentially", "it's worth noting")
5. Do NOT remove: function signatures, type definitions, code blocks, dependency lists, file paths
6. Overwrite the original file with the compacted version
7. Commit the change with message: `chore: compact <filename>`

**Validation:**
- If `$ARGUMENTS` is empty, respond with: "Usage: /compact <filepath>"
- If file doesn't exist, respond with: "Error: File not found: <filepath>"

**Output:** Report the before/after line count and approximate reduction percentage.
