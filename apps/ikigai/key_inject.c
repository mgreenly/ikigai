#include "key_inject.h"

#include "shared/panic.h"
#include "shared/wrapper.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

#define INITIAL_CAPACITY 256

ik_key_inject_buf_t *ik_key_inject_init(TALLOC_CTX *ctx)
{
    ik_key_inject_buf_t *buf = talloc_zero_(ctx, sizeof(ik_key_inject_buf_t));
    if (!buf) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    buf->data = talloc_zero_array(buf, char, INITIAL_CAPACITY);
    if (!buf->data) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    buf->capacity = INITIAL_CAPACITY;
    buf->len = 0;
    buf->read_pos = 0;

    return buf;
}

void ik_key_inject_destroy(ik_key_inject_buf_t *buf)
{
    if (buf) {
        talloc_free(buf);
    }
}

res_t ik_key_inject_append(ik_key_inject_buf_t *buf, const char *raw_bytes, size_t len)
{
    if (!buf || !raw_bytes) {
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);
        res_t err_res = ERR(tmp_ctx, INVALID_ARG, "NULL pointer");
        return err_res;
    }

    // Grow buffer if needed
    while (buf->len + len > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        char *new_data = talloc_realloc(buf, buf->data, char, (unsigned int)new_capacity);
        if (!new_data) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    // Append bytes
    memcpy(buf->data + buf->len, raw_bytes, len);
    buf->len += len;

    return OK(NULL);
}

bool ik_key_inject_drain(ik_key_inject_buf_t *buf, char *out_byte)
{
    if (!buf || !out_byte) {
        return false;
    }

    // Check if empty
    if (buf->read_pos >= buf->len) {
        return false;
    }

    // Drain one byte
    *out_byte = buf->data[buf->read_pos];
    buf->read_pos++;

    // Reset when fully drained
    if (buf->read_pos == buf->len) {
        buf->read_pos = 0;
        buf->len = 0;
    }

    return true;
}

size_t ik_key_inject_pending(ik_key_inject_buf_t *buf)
{
    if (!buf) {
        return 0;
    }
    return buf->len - buf->read_pos;
}

res_t ik_key_inject_unescape(TALLOC_CTX *ctx, const char *input, size_t input_len,
                               char **out, size_t *out_len)
{
    if (!input || !out || !out_len) {
        return ERR(ctx, INVALID_ARG, "NULL pointer");
    }

    // Allocate output buffer (worst case: same size as input)
    char *result = talloc_array(ctx, char, (unsigned int)input_len);
    if (!result) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t out_pos = 0;
    size_t i = 0;

    while (i < input_len) {
        if (input[i] != '\\' || i + 1 >= input_len) {
            // Regular character
            result[out_pos++] = input[i];
            i++;
            continue;
        }

        // Escape sequence
        char next = input[i + 1];
        switch (next) {
            case 'r':
                result[out_pos++] = '\r';
                i += 2;
                break;
            case 'n':
                result[out_pos++] = '\n';
                i += 2;
                break;
            case 't':
                result[out_pos++] = '\t';
                i += 2;
                break;
            case '\\':
                result[out_pos++] = '\\';
                i += 2;
                break;
            case 'x':
                if (i + 3 < input_len) {
                    // Hex escape: \xNN
                    char hex[3] = {input[i + 2], input[i + 3], '\0'};
                    char *endptr = NULL;
                    int64_t val = strtol(hex, &endptr, 16);
                    if (endptr == hex + 2) {
                        result[out_pos++] = (char)val;
                        i += 4;
                    } else {
                        // Invalid hex - pass through backslash
                        result[out_pos++] = input[i];
                        i++;
                    }
                } else {
                    // Not enough chars for \xNN - pass through backslash
                    result[out_pos++] = input[i];
                    i++;
                }
                break;
            default:
                // Unknown escape - pass through backslash
                result[out_pos++] = input[i];
                i++;
                break;
        }
    }

    *out = result;
    *out_len = out_pos;

    return OK(NULL);
}
