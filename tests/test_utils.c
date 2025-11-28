#include "test_utils.h"
#include <talloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

// ========== Allocator Wrapper Overrides ==========
// Strong symbols that override the weak symbols in src/wrapper.c

void *talloc_zero_(TALLOC_CTX *ctx, size_t size)
{
    return talloc_zero_size(ctx, size);
}

char *talloc_strdup_(TALLOC_CTX *ctx, const char *str)
{
    return talloc_strdup(ctx, str);
}

void *talloc_array_(TALLOC_CTX *ctx, size_t el_size, size_t count)
{
    return talloc_zero_size(ctx, el_size * count);
}

void *talloc_realloc_(TALLOC_CTX *ctx, void *ptr, size_t size)
{
    return talloc_realloc_size(ctx, ptr, size);
}

char *talloc_asprintf_(TALLOC_CTX *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *result = talloc_vasprintf(ctx, fmt, ap);
    va_end(ap);
    return result;
}

// ========== Test Config Helper ==========

ik_cfg_t *ik_test_create_config(TALLOC_CTX *ctx)
{
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    if (cfg == NULL) return NULL;

    // Set minimal required fields for testing
    cfg->openai_api_key = talloc_strdup(cfg, "test-api-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 4096;
    cfg->openai_system_message = NULL;
    cfg->listen_address = talloc_strdup(cfg, "127.0.0.1");
    cfg->listen_port = 8080;

    return cfg;
}

// ========== File I/O Helpers ==========

char *load_file_to_string(TALLOC_CTX *ctx, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    // Get file size
    struct stat st;
    if (fstat(fileno(f), &st) != 0) {
        fclose(f);
        return NULL;
    }

    size_t size = (size_t)st.st_size;

    // Allocate buffer (talloc panics on OOM)
    char *buffer = talloc_zero_size(ctx, size + 1);

    // Read file
    size_t bytes_read = fread(buffer, 1, size, f);
    fclose(f);

    if (bytes_read != size) {
        talloc_free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
}
