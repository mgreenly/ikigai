# Release: Minor UI Fixes

Two minor UI bug fixes.

## Features

### 1. Sub-agent lower separator missing

When running inside a sub-agent, the lower separator line is not drawn.

### 2. Duplicate error messages

Error messages are displayed twice. For example, typing an unknown command like `/bingo` produces:

```
Error: Unknown command 'bingo'
Error: Unknown command 'bingo'
```

Expected: single error message output.
