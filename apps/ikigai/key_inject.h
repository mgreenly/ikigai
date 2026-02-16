#ifndef IK_KEY_INJECT_H
#define IK_KEY_INJECT_H

#include <stdbool.h>
#include <stddef.h>
#include <talloc.h>

#include "shared/error.h"

// Key injection buffer for queuing keyboard input
typedef struct {
    char *data;
    size_t len;
    size_t capacity;
    size_t read_pos;
} ik_key_inject_buf_t;

// Initialize a new key injection buffer
ik_key_inject_buf_t *ik_key_inject_init(TALLOC_CTX *ctx);

// Destroy a key injection buffer
void ik_key_inject_destroy(ik_key_inject_buf_t *buf);

// Append raw bytes to the buffer, growing if needed
res_t ik_key_inject_append(ik_key_inject_buf_t *buf, const char *raw_bytes, size_t len);

// Drain one byte from the buffer
// Returns true if a byte was available, false if empty
// When read_pos == len, resets both to 0
bool ik_key_inject_drain(ik_key_inject_buf_t *buf, char *out_byte);

// Get number of bytes remaining in buffer
size_t ik_key_inject_pending(ik_key_inject_buf_t *buf);

// Convert C-style escape sequences to raw bytes
// Handles: \r \n \t \\ \xNN (hex)
// Returns talloc'd output buffer
res_t ik_key_inject_unescape(TALLOC_CTX *ctx, const char *input, size_t input_len,
                               char **out, size_t *out_len);

#endif
