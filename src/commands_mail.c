/**
 * @file commands_mail.c
 * @brief Mail command handlers implementation
 */

#include "commands.h"

#include "agent.h"
#include "commands_mail_helpers.h"
#include "db/agent.h"
#include "db/connection.h"
#include "db/mail.h"
#include "mail/msg.h"
#include "panic.h"
#include "repl.h"
#include "scrollback.h"
#include "scrollback_utils.h"
#include "shared.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


#include "poison.h"
res_t ik_cmd_send(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE

    // Parse: <uuid> "message"
    if (args == NULL || args[0] == '\0') {     // LCOV_EXCL_BR_LINE
        const char *err = "Usage: /mail-send <uuid> \"message\"";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }

    // Extract UUID
    char uuid[256];
    if (!ik_mail_parse_uuid(args, uuid)) {     // LCOV_EXCL_BR_LINE
        const char *err = "Usage: /mail-send <uuid> \"message\"";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }

    // Skip to message part (after UUID and whitespace)
    const char *p = args;
    while (*p && isspace((unsigned char)*p)) {     // LCOV_EXCL_BR_LINE
        p++;
    }
    while (*p && !isspace((unsigned char)*p)) {     // LCOV_EXCL_BR_LINE
        p++;
    }
    while (*p && isspace((unsigned char)*p)) {     // LCOV_EXCL_BR_LINE
        p++;
    }

    // Extract quoted message
    if (*p != '"') {     // LCOV_EXCL_BR_LINE
        const char *err = "Usage: /mail-send <uuid> \"message\"";
        ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
        return OK(NULL);
    }
    p++;  // Skip opening quote

    const char *msg_start = p;
    while (*p && *p != '"') {     // LCOV_EXCL_BR_LINE
        p++;
    }

    if (*p != '"') {     // LCOV_EXCL_BR_LINE
        const char *err = "Usage: /mail-send <uuid> \"message\"";
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
    return ik_mail_render_list(ctx, repl->current->scrollback, inbox, count);
}

res_t ik_cmd_read_mail(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE

    // Parse message index (1-based)
    int64_t index;
    if (!ik_mail_parse_index(args, &index)) {     // LCOV_EXCL_BR_LINE
        const char *msg_text = args == NULL || args[0] == '\0'
            ? "Missing message ID (usage: /mail-read <id>)"
            : "Invalid message ID";
        char *msg = ik_scrollback_format_warning(ctx, msg_text);
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
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
        char *msg = ik_scrollback_format_warning(ctx, "Message not found");
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
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

    // Parse message index (1-based position)
    int64_t index;
    if (!ik_mail_parse_index(args, &index)) {     // LCOV_EXCL_BR_LINE
        const char *msg_text = args == NULL || args[0] == '\0'
            ? "Missing message ID (usage: /mail-delete <id>)"
            : "Invalid message ID";
        char *msg = ik_scrollback_format_warning(ctx, msg_text);
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
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
        char *msg = ik_scrollback_format_warning(ctx, "Message not found");
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
        return OK(NULL);
    }

    // Get the message (convert 1-based to 0-based index)
    ik_mail_msg_t *msg = inbox[index - 1];

    // Delete using database ID (validates ownership internally)
    res = ik_db_mail_delete(repl->shared->db_ctx, msg->id, repl->current->uuid);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        if (res.err->code == ERR_IO && strstr(res.err->msg, "not found")) {     // LCOV_EXCL_BR_LINE
            char *msg_text = ik_scrollback_format_warning(ctx, "Mail not found or not yours");
            ik_scrollback_append_line(repl->current->scrollback, msg_text, strlen(msg_text));
            talloc_free(msg_text);
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
        char *msg = ik_scrollback_format_warning(ctx, "Usage: /mail-filter --from <uuid>");
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
        return OK(NULL);
    }

    // Extract UUID (skip "--from ")
    const char *uuid_arg = args + 7;
    while (*uuid_arg && isspace((unsigned char)*uuid_arg)) {     // LCOV_EXCL_BR_LINE
        uuid_arg++;
    }

    if (*uuid_arg == '\0') {     // LCOV_EXCL_BR_LINE
        char *msg = ik_scrollback_format_warning(ctx, "Usage: /mail-filter --from <uuid>");
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
        return OK(NULL);
    }

    // Find the sender agent by UUID (partial match allowed)
    ik_agent_ctx_t *sender = ik_repl_find_agent(repl, uuid_arg);
    if (sender == NULL) {     // LCOV_EXCL_BR_LINE
        char *msg;
        if (ik_repl_uuid_ambiguous(repl, uuid_arg)) {     // LCOV_EXCL_BR_LINE
            msg = ik_scrollback_format_warning(ctx, "Ambiguous UUID prefix");
        } else {
            msg = ik_scrollback_format_warning(ctx, "Agent not found");
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
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
    return ik_mail_render_list(ctx, repl->current->scrollback, inbox, count);
}
