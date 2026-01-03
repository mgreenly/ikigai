## Overview

Pretty-printing helper functions for formatting structured data into readable text output. These utilities provide a family of functions that consistently format different data types (pointers, integers, strings, booleans) with proper indentation and escaping, enabling consistent diagnostic and debug output across the codebase.

## Code

```
function pp_header(buffer, indent_level, type_name, memory_address):
    validate buffer and type_name are not null

    indent to the specified level
    output: "{type_name} @ {hex_address}"

function pp_pointer(buffer, indent_level, field_name, memory_address):
    validate buffer and field_name are not null

    indent to the specified level
    if memory_address is null:
        output: "{field_name}: NULL"
    else:
        output: "{field_name}: {hex_address}"

function pp_size_t(buffer, indent_level, field_name, unsigned_integer):
    validate buffer and field_name are not null

    indent to the specified level
    output: "{field_name}: {unsigned_integer}"

function pp_int32(buffer, indent_level, field_name, signed_integer):
    validate buffer and field_name are not null

    indent to the specified level
    output: "{field_name}: {signed_integer}"

function pp_uint32(buffer, indent_level, field_name, unsigned_integer):
    validate buffer and field_name are not null

    indent to the specified level
    output: "{field_name}: {unsigned_integer}"

function pp_string(buffer, indent_level, field_name, string_data, length):
    validate buffer and field_name are not null

    indent to the specified level
    output field name and colon

    if string_data is null:
        output "NULL"
        return

    output opening quote

    for each character in string up to length:
        if character is newline:
            output escaped sequence "\n"
        else if character is carriage return:
            output escaped sequence "\r"
        else if character is tab:
            output escaped sequence "\t"
        else if character is backslash:
            output escaped sequence "\\"
        else if character is double quote:
            output escaped sequence "\""
        else if character is control character (less than 32) or DEL (127):
            output hexadecimal escape sequence "\xHH"
        else:
            output character as-is

    output closing quote and newline

function pp_bool(buffer, indent_level, field_name, boolean_value):
    validate buffer and field_name are not null

    indent to the specified level
    output: "{field_name}: true" or "{field_name}: false"
```
