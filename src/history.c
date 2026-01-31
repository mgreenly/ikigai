#include "history.h"
#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>


#include "poison.h"
ik_history_t *ik_history_create(void *ctx, size_t capacity)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(capacity > 0);      // LCOV_EXCL_BR_LINE

    ik_history_t *hist = talloc_zero(ctx, ik_history_t);
    if (hist == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    hist->capacity = capacity;
    hist->count = 0;
    hist->index = 0;  // Not browsing initially (index == count == 0)
    hist->pending = NULL;

    // Allocate entries array
    hist->entries = talloc_array(hist, char *, (unsigned int)capacity);
    if (hist->entries == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return hist;
}

res_t ik_history_add(ik_history_t *hist, const char *entry)
{
    assert(hist != NULL);      // LCOV_EXCL_BR_LINE
    assert(entry != NULL);     // LCOV_EXCL_BR_LINE

    if (entry[0] == '\0') {
        return OK(NULL);
    }

    // Skip if identical to most recent entry
    if (hist->count > 0 && strcmp(hist->entries[hist->count - 1], entry) == 0) {
        hist->index = hist->count;
        if (hist->pending != NULL) {
            talloc_free(hist->pending);
            hist->pending = NULL;
        }
        return OK(NULL);
    }

    // Check if entry exists - move to end if found
    int existing_index = -1;
    for (size_t i = 0; i < hist->count; i++) {
        if (strcmp(hist->entries[i], entry) == 0) {
            existing_index = (int)i;
            break;
        }
    }

    if (existing_index >= 0) {
        talloc_free(hist->entries[existing_index]);
        for (size_t i = (size_t)existing_index; i < hist->count - 1; i++) {
            hist->entries[i] = hist->entries[i + 1];
        }
        hist->count--;
    } else if (hist->count == hist->capacity) {
        talloc_free(hist->entries[0]);
        for (size_t i = 0; i < hist->count - 1; i++) {
            hist->entries[i] = hist->entries[i + 1];
        }
        hist->count--;
    }

    hist->entries[hist->count] = talloc_strdup(hist, entry);
    if (hist->entries[hist->count] == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    hist->count++;

    hist->index = hist->count;
    if (hist->pending != NULL) {
        talloc_free(hist->pending);
        hist->pending = NULL;
    }

    return OK(NULL);
}

res_t ik_history_start_browsing(ik_history_t *hist, const char *pending_input)
{
    assert(hist != NULL);           // LCOV_EXCL_BR_LINE
    assert(pending_input != NULL);  // LCOV_EXCL_BR_LINE

    // If history is empty, just save pending and stay in non-browsing state
    if (hist->count == 0) {
        if (hist->pending != NULL) {
            talloc_free(hist->pending);
        }
        hist->pending = talloc_strdup(hist, pending_input);
        if (hist->pending == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        hist->index = 0;  // Still not browsing (count is 0)
        return OK(NULL);
    }

    // Free old pending if it exists
    if (hist->pending != NULL) {
        talloc_free(hist->pending);
    }

    // Save pending input
    hist->pending = talloc_strdup(hist, pending_input);
    if (hist->pending == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Move to last entry
    hist->index = hist->count - 1;

    return OK(NULL);
}

const char *ik_history_prev(ik_history_t *hist)
{
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    // If not browsing or at beginning, return NULL
    if (!ik_history_is_browsing(hist) || hist->index == 0) {
        return NULL;
    }

    // Move to previous entry
    hist->index--;

    return hist->entries[hist->index];
}

const char *ik_history_next(ik_history_t *hist)
{
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    // If we're past the last entry, return NULL
    if (hist->index > hist->count) {
        return NULL;
    }

    // If we're exactly at pending position, increment and return NULL
    // (user has already seen pending, trying to go past it)
    if (hist->index == hist->count) {
        hist->index++;
        return NULL;
    }

    // Move forward in history
    hist->index++;

    // If we're still in history entries, return the entry
    if (hist->index < hist->count) {
        return hist->entries[hist->index];
    }

    // We just moved to pending position - return pending
    // Don't increment further so user stays "at" pending
    return hist->pending;
}

void ik_history_stop_browsing(ik_history_t *hist)
{
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    // Move to non-browsing position
    hist->index = hist->count;

    // Free pending input
    if (hist->pending != NULL) {
        talloc_free(hist->pending);
        hist->pending = NULL;
    }
}

const char *ik_history_get_current(const ik_history_t *hist)
{
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    // If not browsing (index == count), return pending or NULL
    if (hist->index >= hist->count) {
        return hist->pending;
    }

    // Otherwise return entry at current index
    return hist->entries[hist->index];
}

bool ik_history_is_browsing(const ik_history_t *hist)
{
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    // Browsing if index is less than count
    return hist->index < hist->count;
}
