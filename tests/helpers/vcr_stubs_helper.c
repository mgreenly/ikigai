/**
 * @file vcr_stubs.c
 * @brief Stub implementations of VCR functions for tests that don't use VCR
 *
 * These weak symbol stubs allow wrapper.c VCR integration to work
 * even when VCR is not initialized. Tests that actually use VCR
 * will link against vcr.o which provides the real implementations.
 */

#include "vcr_helper.h"

// Weak symbol stubs - will be replaced by real implementations when vcr.o is linked

__attribute__((weak))
bool vcr_is_active(void)
{
    return false;
}

__attribute__((weak))
bool vcr_is_recording(void)
{
    return false;
}

__attribute__((weak))
int vcr_get_response_status(void)
{
    return 0;
}

__attribute__((weak))
bool vcr_next_chunk(const char **data_out, size_t *len_out)
{
    (void)data_out;
    (void)len_out;
    return false;
}

__attribute__((weak))
bool vcr_has_more(void)
{
    return false;
}

__attribute__((weak))
void vcr_record_chunk(const char *data, size_t len)
{
    (void)data;
    (void)len;
}
