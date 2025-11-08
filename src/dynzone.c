/**
 * @file dynzone.c
 * @brief Dynamic zone text buffer implementation
 */

#include "dynzone.h"
#include "error.h"
#include <assert.h>
#include <talloc.h>

res_t ik_dynzone_create(void *parent, ik_dynzone_t **zone_out) {
    assert(zone_out != NULL); /* LCOV_EXCL_BR_LINE */

    ik_dynzone_t *zone = talloc_zero(parent, ik_dynzone_t);
    if (zone == NULL) {
        return ERR(parent, OOM, "Failed to allocate dynzone");
    }

    res_t res = ik_byte_array_create(zone, 64);
    if (is_err(&res)) {
        talloc_free(zone);
        return res;
    }

    zone->text = res.ok;
    zone->cursor_byte_offset = 0;
    *zone_out = zone;
    return OK(zone);
}

res_t ik_dynzone_insert_codepoint(ik_dynzone_t *zone, uint32_t codepoint) {
    assert(zone != NULL); /* LCOV_EXCL_BR_LINE */

    (void)codepoint;
    return OK(NULL);
}

res_t ik_dynzone_insert_newline(ik_dynzone_t *zone) {
    assert(zone != NULL); /* LCOV_EXCL_BR_LINE */

    return OK(NULL);
}

res_t ik_dynzone_backspace(ik_dynzone_t *zone) {
    assert(zone != NULL); /* LCOV_EXCL_BR_LINE */

    return OK(NULL);
}

res_t ik_dynzone_delete(ik_dynzone_t *zone) {
    assert(zone != NULL); /* LCOV_EXCL_BR_LINE */

    return OK(NULL);
}

res_t ik_dynzone_get_text(ik_dynzone_t *zone, char **text_out, size_t *len_out) {
    assert(zone != NULL);     /* LCOV_EXCL_BR_LINE */
    assert(text_out != NULL); /* LCOV_EXCL_BR_LINE */
    assert(len_out != NULL);  /* LCOV_EXCL_BR_LINE */

    *text_out = (char *)zone->text->data;
    *len_out = ik_byte_array_size(zone->text);
    return OK(NULL);
}

void ik_dynzone_clear(ik_dynzone_t *zone) {
    assert(zone != NULL); /* LCOV_EXCL_BR_LINE */

    ik_byte_array_clear(zone->text);
    zone->cursor_byte_offset = 0;
}
