## Overview

This file implements pretty-printing (formatted debugging output) for input buffer structures. It takes an input buffer object and formats its contents into a human-readable representation suitable for display, debugging, or logging purposes. The implementation uses a series of helper functions to consistently format different field types.

## Code

```
function pretty_print_input_buffer(input_buffer, output_buffer, indent_level):
    validate input_buffer and output_buffer exist

    print header with structure type and memory address at current indent level

    extract text content from the input buffer's internal byte array
    determine the length of text stored

    print the text length using the size formatter at increased indent level

    if the input buffer has a cursor:
        recursively pretty-print the cursor using the nested structure printer
        (cursor is indented one level deeper)

    print the target column value using the size formatter at increased indent level

    if there is text content:
        print the text string with proper escaping for display
        (string content is indented one level deeper)
```

The function follows a consistent pattern: validate inputs, print metadata (header and sizes), print nested structures recursively, then print the actual content. All output is accumulated in the format buffer at appropriate indentation levels for hierarchical display.
