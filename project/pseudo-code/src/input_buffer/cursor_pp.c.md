## Overview

This file implements pretty-printing functionality for cursor objects. It provides a single function that formats a cursor's state (byte offset and grapheme offset) for display purposes, with configurable indentation for nested output.

## Code

```
function pretty_print_cursor(cursor, output_buffer, indentation_level):
    validate that cursor is not null
    validate that output_buffer is not null

    format and write header with the cursor's type name and memory address
        indentation: indentation_level

    format and write the cursor's byte offset field
        label: "byte_offset"
        indentation: indentation_level + 2

    format and write the cursor's grapheme offset field
        label: "grapheme_offset"
        indentation: indentation_level + 2
```
