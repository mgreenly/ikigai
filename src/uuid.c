#include "uuid.h"

#include "panic.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

// Base64url alphabet (RFC 4648 section 5)
static const char BASE64URL[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

char *ik_generate_uuid(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    // Generate 16 random bytes (128-bit UUID v4)
    unsigned char bytes[16];
    for (int i = 0; i < 16; i++) {
        bytes[i] = (unsigned char)(rand() & 0xFF);
    }

    // Set version (4) and variant (RFC 4122)
    bytes[6] = (bytes[6] & 0x0F) | 0x40;  // Version 4
    bytes[8] = (bytes[8] & 0x3F) | 0x80;  // Variant 1

    // Encode 16 bytes to 22 base64url characters (no padding)
    // 16 bytes = 128 bits, base64 encodes 6 bits per char
    // ceil(128/6) = 22 characters
    char *uuid = talloc_array(ctx, char, 23);  // 22 chars + null
    if (uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    int j = 0;
    for (int i = 0; i < 16; i += 3) {
        uint32_t n = ((uint32_t)bytes[i] << 16);
        if (i + 1 < 16) n |= ((uint32_t)bytes[i + 1] << 8);
        if (i + 2 < 16) n |= bytes[i + 2];

        uuid[j++] = BASE64URL[(n >> 18) & 0x3F];
        uuid[j++] = BASE64URL[(n >> 12) & 0x3F];
        if (i + 1 < 16) uuid[j++] = BASE64URL[(n >> 6) & 0x3F];
        if (i + 2 < 16) uuid[j++] = BASE64URL[n & 0x3F];
    }
    uuid[j] = '\0';

    return uuid;
}
