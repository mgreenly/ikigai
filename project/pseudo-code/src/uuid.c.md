## Overview

This file provides UUID generation functionality. It implements UUID v4 (random) generation with base64url encoding, producing 22-character unique identifiers suitable for use as compact opaque IDs in the system.

## Code

```
function generate_uuid() -> string:
    validate caller provided valid memory context

    allocate array of 16 random bytes

    populate bytes with random values

    set UUID version to 4 (random) in byte 6
    set UUID variant to RFC 4122 in byte 8

    allocate output buffer for 22 characters (plus null terminator)

    encode 16 bytes to base64url format:
        for each group of 3 bytes (16 bytes = 5 groups with remainder):
            combine 3 bytes into single 24-bit value
            extract 4 base64url characters (6 bits each)
            write characters to output, skipping padding

    null-terminate the output string

    return generated UUID string
```

The encoding follows RFC 4648 base64url specification (using `-` and `_` instead of `+` and `/`), and produces unpadded output since the byte count aligns perfectly with 22 base64 characters.
