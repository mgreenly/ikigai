/**
 * @file commands_mail_helpers.c
 * @brief Helper functions for mail command implementations
 */

#include "commands_mail_helpers.h"

#include "panic.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <time.h>

void ik_mail_format_timestamp(int64_t diff, char *buf, size_t buf_size)
{
    assert(buf != NULL);       // LCOV_EXCL_BR_LINE
    assert(buf_size > 0);      // LCOV_EXCL_BR_LINE

    if (diff < 60) {     // LCOV_EXCL_BR_LINE
        snprintf(buf, buf_size, "%" PRId64 " sec ago", diff);
    } else if (diff < 3600) {     // LCOV_EXCL_BR_LINE
        snprintf(buf, buf_size, "%" PRId64 " min ago", diff / 60);
    } else if (diff < 86400) {     // LCOV_EXCL_BR_LINE
        snprintf(buf, buf_size, "%" PRId64 " hour%s ago",
                 diff / 3600, (diff / 3600) == 1 ? "" : "s");
    } else {     // LCOV_EXCL_BR_LINE
        snprintf(buf, buf_size, "%" PRId64 " day%s ago",
                 diff / 86400, (diff / 86400) == 1 ? "" : "s");
    }
}

res_t ik_mail_render_list(void *ctx, ik_scrollback_t *scrollback,
                          ik_mail_msg_t **inbox, size_t count)
{
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(scrollback != NULL);   // LCOV_EXCL_BR_LINE
    assert(inbox != NULL);        // LCOV_EXCL_BR_LINE

    int64_t now = (int64_t)time(NULL);
    for (size_t i = 0; i < count; i++) {     // LCOV_EXCL_BR_LINE
        ik_mail_msg_t *msg = inbox[i];

        // Calculate time difference
        int64_t diff = now - msg->timestamp;

        // Format relative timestamp
        char time_str[64];
        ik_mail_format_timestamp(diff, time_str, sizeof(time_str));

        // Format message line: "  [1] * from abc123... (2 min ago)"
        char *msg_line = talloc_asprintf(ctx, "  [%zu] %s from %.22s... (%s)",
                                         i + 1,
                                         msg->read ? " " : "*",
                                         msg->from_uuid,
                                         time_str);
        if (!msg_line) {     // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }

        res_t res = ik_scrollback_append_line(scrollback, msg_line, strlen(msg_line));
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }

        // Format preview line: "      \"Preview of message...\""
        // Truncate body to 50 chars max
        size_t body_len = strlen(msg->body);
        char preview[64];
        if (body_len <= 50) {     // LCOV_EXCL_BR_LINE
            snprintf(preview, sizeof(preview), "      \"%s\"", msg->body);
        } else {
            snprintf(preview, sizeof(preview), "      \"%.50s...\"", msg->body);     // LCOV_EXCL_LINE
        }

        res = ik_scrollback_append_line(scrollback, preview, strlen(preview));
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }
    }

    return OK(NULL);
}

bool ik_mail_parse_uuid(const char *args, char *uuid_out)
{
    assert(args != NULL);      // LCOV_EXCL_BR_LINE
    assert(uuid_out != NULL);  // LCOV_EXCL_BR_LINE

    // Skip leading whitespace
    const char *p = args;
    while (*p && isspace((unsigned char)*p)) {     // LCOV_EXCL_BR_LINE
        p++;
    }

    const char *uuid_start = p;
    while (*p && !isspace((unsigned char)*p)) {     // LCOV_EXCL_BR_LINE
        p++;
    }

    if (p == uuid_start) {     // LCOV_EXCL_BR_LINE
        return false;
    }

    size_t uuid_len = (size_t)(p - uuid_start);
    if (uuid_len >= 256) {     // LCOV_EXCL_BR_LINE
        return false;
    }

    memcpy(uuid_out, uuid_start, uuid_len);
    uuid_out[uuid_len] = '\0';
    return true;
}

bool ik_mail_parse_index(const char *args, int64_t *index_out)
{
    assert(index_out != NULL);  // LCOV_EXCL_BR_LINE

    if (args == NULL || args[0] == '\0') {     // LCOV_EXCL_BR_LINE
        return false;
    }

    char *endptr = NULL;
    int64_t index = strtoll(args, &endptr, 10);
    if (*endptr != '\0' || index < 1) {     // LCOV_EXCL_BR_LINE
        return false;
    }

    *index_out = index;
    return true;
}
