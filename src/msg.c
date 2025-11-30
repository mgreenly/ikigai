/**
 * @file msg.c
 * @brief Canonical message format implementation
 */

#include "msg.h"
#include "panic.h"
#include <assert.h>
#include <string.h>
#include <talloc.h>

res_t ik_msg_from_db(void *parent, const ik_message_t *db_msg)
{
    assert(db_msg != NULL);  // LCOV_EXCL_BR_LINE
    assert(db_msg->kind != NULL);  // LCOV_EXCL_BR_LINE

    // Skip non-conversation kinds
    if (strcmp(db_msg->kind, "clear") == 0 ||
        strcmp(db_msg->kind, "mark") == 0 ||
        strcmp(db_msg->kind, "rewind") == 0) {
        return OK(NULL);
    }

    // Create canonical message
    ik_msg_t *msg = talloc_zero(parent, ik_msg_t);
    if (msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Copy kind
    msg->kind = talloc_strdup(msg, db_msg->kind);
    if (msg->kind == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Copy content (may be NULL for some message types)
    if (db_msg->content != NULL) {
        msg->content = talloc_strdup(msg, db_msg->content);
        if (msg->content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        msg->content = NULL;
    }

    // Copy data_json for tool messages
    if (strcmp(db_msg->kind, "tool_call") == 0 ||
        strcmp(db_msg->kind, "tool_result") == 0) {
        if (db_msg->data_json != NULL) {
            msg->data_json = talloc_strdup(msg, db_msg->data_json);
            if (msg->data_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        } else {
            msg->data_json = NULL;
        }
    } else {
        // Non-tool messages don't have data_json
        msg->data_json = NULL;
    }

    return OK(msg);
}
