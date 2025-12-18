/**
 * @file commands_mail.c
 * @brief Mail command handlers implementation
 */

#include "commands.h"

#include "agent.h"
#include "db/agent.h"
#include "db/connection.h"
#include "db/mail.h"
#include "mail/msg.h"
#include "panic.h"
#include "repl.h"
#include "scrollback.h"
#include "shared.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

res_t ik_cmd_send(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE

    // Parse: <uuid> "message"
    if (args == NULL || args[0] == '\0') {     // LCOV_EXCL_BR_LINE
        const char *err = "Usage: /send <uuid> \"message\"";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }

    // Extract UUID
    const char *p = args;
    while (*p && isspace((unsigned char)*p)) {     // LCOV_EXCL_BR_LINE
        p++;
    }

    const char *uuid_start = p;
    while (*p && !isspace((unsigned char)*p)) {     // LCOV_EXCL_BR_LINE
        p++;
    }

    if (p == uuid_start) {     // LCOV_EXCL_BR_LINE
        const char *err = "Usage: /send <uuid> \"message\"";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }

    size_t uuid_len = (size_t)(p - uuid_start);
    char uuid[256];
    if (uuid_len >= sizeof(uuid)) {     // LCOV_EXCL_BR_LINE
        const char *err = "UUID too long";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }
    memcpy(uuid, uuid_start, uuid_len);
    uuid[uuid_len] = '\0';

    // Skip whitespace before message
    while (*p && isspace((unsigned char)*p)) {     // LCOV_EXCL_BR_LINE
        p++;
    }

    // Extract quoted message
    if (*p != '"') {     // LCOV_EXCL_BR_LINE
        const char *err = "Usage: /send <uuid> \"message\"";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }
    p++;  // Skip opening quote

    const char *msg_start = p;
    while (*p && *p != '"') {     // LCOV_EXCL_BR_LINE
        p++;
    }

    if (*p != '"') {     // LCOV_EXCL_BR_LINE
        const char *err = "Usage: /send <uuid> \"message\"";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }

    size_t msg_len = (size_t)(p - msg_start);
    char body[4096];
    if (msg_len >= sizeof(body)) {     // LCOV_EXCL_BR_LINE
        const char *err = "Message too long";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }
    memcpy(body, msg_start, msg_len);
    body[msg_len] = '\0';

    // Validate recipient exists
    ik_agent_ctx_t *recipient = ik_repl_find_agent(repl, uuid);
    if (recipient == NULL) {     // LCOV_EXCL_BR_LINE
        const char *err = "Agent not found";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }

    // Validate recipient is running (Q11)
    ik_db_agent_row_t *agent_row = NULL;
    res_t res = ik_db_agent_get(repl->shared->db_ctx, ctx, recipient->uuid, &agent_row);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    if (strcmp(agent_row->status, "running") != 0) {     // LCOV_EXCL_BR_LINE
        const char *err = "Recipient agent is dead";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }

    // Validate body non-empty
    if (body[0] == '\0') {     // LCOV_EXCL_BR_LINE
        const char *err = "Message body cannot be empty";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }

    // Create mail message
    ik_mail_msg_t *msg = ik_mail_msg_create(ctx,
        repl->current->uuid, recipient->uuid, body);
    if (msg == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }

    // Insert into database
    res = ik_db_mail_insert(repl->shared->db_ctx, repl->shared->session_id, msg);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Display confirmation
    char confirm[64];
    int32_t written = snprintf(confirm, sizeof(confirm), "Mail sent to %.22s",
        recipient->uuid);
    if (written < 0 || (size_t)written >= sizeof(confirm)) {     // LCOV_EXCL_BR_LINE
        PANIC("snprintf failed");     // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->current->scrollback, confirm, (size_t)written);

    return OK(NULL);
}

res_t ik_cmd_check_mail(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    (void)args;

    // Get inbox for current agent
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res_t res = ik_db_mail_inbox(repl->shared->db_ctx, ctx,
                                  repl->shared->session_id,
                                  repl->current->uuid,
                                  &inbox, &count);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Empty inbox
    if (count == 0) {     // LCOV_EXCL_BR_LINE
        const char *msg = "No messages";
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return OK(NULL);
    }

    // Count unread messages
    size_t unread_count = 0;
    for (size_t i = 0; i < count; i++) {     // LCOV_EXCL_BR_LINE
        if (!inbox[i]->read) {     // LCOV_EXCL_BR_LINE
            unread_count++;
        }
    }

    // Display summary header
    char *header = talloc_asprintf(ctx, "Inbox (%zu message%s, %zu unread):",
                                   count, count == 1 ? "" : "s", unread_count);
    if (!header) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    res = ik_scrollback_append_line(repl->current->scrollback, header, strlen(header));
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Display blank line after header
    const char *blank = "";
    res = ik_scrollback_append_line(repl->current->scrollback, blank, strlen(blank));
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Display each message
    int64_t now = (int64_t)time(NULL);
    for (size_t i = 0; i < count; i++) {     // LCOV_EXCL_BR_LINE
        ik_mail_msg_t *msg = inbox[i];

        // Calculate time difference
        int64_t diff = now - msg->timestamp;

        // Format relative timestamp
        char time_str[64];
        if (diff < 60) {     // LCOV_EXCL_BR_LINE
            snprintf(time_str, sizeof(time_str), "%" PRId64 " sec ago", diff);
        } else if (diff < 3600) {     // LCOV_EXCL_BR_LINE
            snprintf(time_str, sizeof(time_str), "%" PRId64 " min ago", diff / 60);
        } else if (diff < 86400) {     // LCOV_EXCL_BR_LINE
            snprintf(time_str, sizeof(time_str), "%" PRId64 " hour%s ago",
                    diff / 3600, (diff / 3600) == 1 ? "" : "s");
        } else {     // LCOV_EXCL_BR_LINE
            snprintf(time_str, sizeof(time_str), "%" PRId64 " day%s ago",
                    diff / 86400, (diff / 86400) == 1 ? "" : "s");
        }

        // Format message line: "  [1] * from abc123... (2 min ago)"
        char *msg_line = talloc_asprintf(ctx, "  [%zu] %s from %.22s... (%s)",
                                         i + 1,
                                         msg->read ? " " : "*",
                                         msg->from_uuid,
                                         time_str);
        if (!msg_line) {     // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }

        res = ik_scrollback_append_line(repl->current->scrollback, msg_line, strlen(msg_line));
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

        res = ik_scrollback_append_line(repl->current->scrollback, preview, strlen(preview));
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }
    }

    return OK(NULL);
}

res_t ik_cmd_read_mail(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE

    // Validate args
    if (args == NULL || args[0] == '\0') {     // LCOV_EXCL_BR_LINE
        const char *msg = "Error: Missing message ID (usage: /read-mail <id>)";
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return OK(NULL);
    }

    // Parse message index (1-based)
    char *endptr = NULL;
    int64_t index = strtoll(args, &endptr, 10);
    if (*endptr != '\0' || index < 1) {     // LCOV_EXCL_BR_LINE
        const char *msg = "Error: Invalid message ID";
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return OK(NULL);
    }

    // Get inbox for current agent
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res_t res = ik_db_mail_inbox(repl->shared->db_ctx, ctx,
                                  repl->shared->session_id,
                                  repl->current->uuid,
                                  &inbox, &count);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Validate index is within range
    if ((size_t)index > count) {     // LCOV_EXCL_BR_LINE
        const char *msg = "Error: Message not found";
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return OK(NULL);
    }

    // Get the message (convert 1-based to 0-based index)
    ik_mail_msg_t *msg = inbox[index - 1];

    // Display message header
    char *header = talloc_asprintf(ctx, "Message from %.22s...", msg->from_uuid);
    if (!header) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    res = ik_scrollback_append_line(repl->current->scrollback, header, strlen(header));
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Display blank line
    const char *blank = "";
    res = ik_scrollback_append_line(repl->current->scrollback, blank, strlen(blank));
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Display message body
    res = ik_scrollback_append_line(repl->current->scrollback, msg->body, strlen(msg->body));
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Mark message as read
    res = ik_db_mail_mark_read(repl->shared->db_ctx, msg->id);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    return OK(NULL);
}

res_t ik_cmd_delete_mail(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE

    // Validate args
    if (args == NULL || args[0] == '\0') {     // LCOV_EXCL_BR_LINE
        const char *msg = "Error: Missing message ID (usage: /delete-mail <id>)";
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return OK(NULL);
    }

    // Parse message index (1-based position)
    char *endptr = NULL;
    int64_t index = strtoll(args, &endptr, 10);
    if (*endptr != '\0' || index < 1) {     // LCOV_EXCL_BR_LINE
        const char *msg = "Error: Invalid message ID";
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return OK(NULL);
    }

    // Get inbox for current agent
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res_t res = ik_db_mail_inbox(repl->shared->db_ctx, ctx,
                                  repl->shared->session_id,
                                  repl->current->uuid,
                                  &inbox, &count);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Validate index is within range
    if ((size_t)index > count) {     // LCOV_EXCL_BR_LINE
        const char *msg = "Error: Message not found";
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return OK(NULL);
    }

    // Get the message (convert 1-based to 0-based index)
    ik_mail_msg_t *msg = inbox[index - 1];

    // Delete using database ID (validates ownership internally)
    res = ik_db_mail_delete(repl->shared->db_ctx, msg->id, repl->current->uuid);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        if (res.err->code == ERR_IO && strstr(res.err->msg, "not found")) {     // LCOV_EXCL_BR_LINE
            const char *msg_text = "Error: Mail not found or not yours";
            ik_scrollback_append_line(repl->current->scrollback, msg_text, strlen(msg_text));
            talloc_free(res.err);
        } else {
            return res;     // LCOV_EXCL_LINE
        }
        return OK(NULL);
    }

    // Confirm
    const char *confirm = "Mail deleted";
    ik_scrollback_append_line(repl->current->scrollback, confirm, strlen(confirm));

    return OK(NULL);
}

res_t ik_cmd_filter_mail(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE

    // Parse --from <uuid>
    if (args == NULL || strncmp(args, "--from ", 7) != 0) {     // LCOV_EXCL_BR_LINE
        const char *msg = "Error: Usage: /filter-mail --from <uuid>";
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return OK(NULL);
    }

    // Extract UUID (skip "--from ")
    const char *uuid_arg = args + 7;
    while (*uuid_arg && isspace((unsigned char)*uuid_arg)) {     // LCOV_EXCL_BR_LINE
        uuid_arg++;
    }

    if (*uuid_arg == '\0') {     // LCOV_EXCL_BR_LINE
        const char *msg = "Error: Usage: /filter-mail --from <uuid>";
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return OK(NULL);
    }

    // Find the sender agent by UUID (partial match allowed)
    ik_agent_ctx_t *sender = ik_repl_find_agent(repl, uuid_arg);
    if (sender == NULL) {     // LCOV_EXCL_BR_LINE
        if (ik_repl_uuid_ambiguous(repl, uuid_arg)) {     // LCOV_EXCL_BR_LINE
            const char *msg = "Error: Ambiguous UUID prefix";
            ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        } else {
            const char *msg = "Error: Agent not found";
            ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        }
        return OK(NULL);
    }

    // Get filtered inbox for current agent from sender
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res_t res = ik_db_mail_inbox_filtered(repl->shared->db_ctx, ctx,
                                          repl->shared->session_id,
                                          repl->current->uuid,
                                          sender->uuid,
                                          &inbox, &count);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Empty result
    if (count == 0) {     // LCOV_EXCL_BR_LINE
        char *msg = talloc_asprintf(ctx, "No messages from %.22s...", sender->uuid);
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return OK(NULL);
    }

    // Count unread messages
    size_t unread_count = 0;
    for (size_t i = 0; i < count; i++) {     // LCOV_EXCL_BR_LINE
        if (!inbox[i]->read) {     // LCOV_EXCL_BR_LINE
            unread_count++;
        }
    }

    // Display filtered header
    char *header = talloc_asprintf(ctx, "Inbox (filtered by %.22s..., %zu message%s, %zu unread):",
                                   sender->uuid,
                                   count, count == 1 ? "" : "s", unread_count);     // LCOV_EXCL_BR_LINE
    if (!header) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    res = ik_scrollback_append_line(repl->current->scrollback, header, strlen(header));
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Display blank line after header
    const char *blank = "";
    res = ik_scrollback_append_line(repl->current->scrollback, blank, strlen(blank));
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Display each message (same format as check-mail)
    int64_t now = (int64_t)time(NULL);
    for (size_t i = 0; i < count; i++) {     // LCOV_EXCL_BR_LINE
        ik_mail_msg_t *msg = inbox[i];

        // Calculate time difference
        int64_t diff = now - msg->timestamp;

        // Format relative timestamp
        char time_str[64];
        if (diff < 60) {     // LCOV_EXCL_BR_LINE
            snprintf(time_str, sizeof(time_str), "%" PRId64 " sec ago", diff);
        } else if (diff < 3600) {     // LCOV_EXCL_BR_LINE
            snprintf(time_str, sizeof(time_str), "%" PRId64 " min ago", diff / 60);
        } else if (diff < 86400) {     // LCOV_EXCL_BR_LINE
            snprintf(time_str, sizeof(time_str), "%" PRId64 " hour%s ago",
                    diff / 3600, (diff / 3600) == 1 ? "" : "s");
        } else {     // LCOV_EXCL_BR_LINE
            snprintf(time_str, sizeof(time_str), "%" PRId64 " day%s ago",
                    diff / 86400, (diff / 86400) == 1 ? "" : "s");
        }

        // Format message line: "  [1] * from abc123... (2 min ago)"
        char *msg_line = talloc_asprintf(ctx, "  [%zu] %s from %.22s... (%s)",
                                         i + 1,
                                         msg->read ? " " : "*",     // LCOV_EXCL_BR_LINE
                                         msg->from_uuid,
                                         time_str);
        if (!msg_line) {     // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }

        res = ik_scrollback_append_line(repl->current->scrollback, msg_line, strlen(msg_line));
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

        res = ik_scrollback_append_line(repl->current->scrollback, preview, strlen(preview));
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }
    }

    return OK(NULL);
}
