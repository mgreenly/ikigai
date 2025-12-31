## Overview

This module provides utilities for extracting typed arguments from JSON-formatted argument strings. It offers two main functions: one for retrieving string values from a JSON object and returning them as allocated copies, and another for retrieving and validating integer values. Both functions perform defensive null-checking and handle JSON parsing errors gracefully.

## Code

```
function get_string_argument(parent_context, arguments_json, key_name):
    validate that arguments_json and key_name are not null
        if either is null, return null

    parse the JSON string
        if parsing fails, return null

    retrieve the root object from the parsed JSON
        if root doesn't exist or isn't an object, free JSON and return null

    look up the value for the requested key in the root object
        if the key doesn't exist, free JSON and return null

    verify the value is a string
        if it's not a string, free JSON and return null

    extract the string content
        if extraction fails, free JSON and return null

    allocate and copy the string into the parent context
        if allocation fails, panic with out-of-memory

    free the JSON document

    return the allocated string copy


function get_integer_argument(arguments_json, key_name, output_pointer):
    validate that arguments_json, key_name, and output_pointer are not null
        if any is null, return failure

    parse the JSON string
        if parsing fails, return failure

    retrieve the root object from the parsed JSON
        if root doesn't exist or isn't an object, free JSON and return failure

    look up the value for the requested key in the root object
        if the key doesn't exist, free JSON and return failure

    verify the value is an integer
        if it's not an integer, free JSON and return failure

    extract the integer value from JSON (64-bit signed)

    free the JSON document

    write the integer to the output location (converted to 32-bit)

    return success
```
