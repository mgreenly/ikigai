/**
 * @file commands_send.c
 * @brief Send command handler implementation
 */

#include "apps/ikigai/commands.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_mail_helpers.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/mail.h"
#include "apps/ikigai/db/notify.h"
#include "apps/ikigai/mail/msg.h"
#include "shared/panic.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/scrollback_utils.h"
#include "apps/ikigai/shared.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>
#include <time.h>


#include "shared/poison.h"
/**
 * Core send logic - reusable by both slash command and internal tool
 *
 * @param ctx Parent context for talloc allocations
 * @param db_ctx Database context to use (main or worker thread)
 * @param session_id Current session ID
 * @param sender_uuid UUID of the sending agent
 * @param recipient_uuid UUID of the recipient agent (must be running)
 * @param body Message body (must be non-empty)
 * @param error_msg_out Optional pointer to receive error message on failure (allocated on ctx)
 * @return OK on success, ERR on failure
 */
res_t ik_send_core(void *ctx,
                   ik_db_ctx_t *db_ctx,
                   int64_t session_id,
                   const char *sender_uuid,
                   const char *recipient_uuid,
                   const char *body,
                   char **error_msg_out)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(sender_uuid != NULL);  // LCOV_EXCL_BR_LINE
    assert(recipient_uuid != NULL);  // LCOV_EXCL_BR_LINE
    assert(body != NULL);  // LCOV_EXCL_BR_LINE

    // Validate body non-empty
    if (body[0] == '\0') {  // LCOV_EXCL_BR_LINE
        if (error_msg_out != NULL) {  // LCOV_EXCL_BR_LINE
            *error_msg_out = talloc_strdup(ctx, "Message body cannot be empty");
        }
        return ERR(ctx, INVALID_ARG, "Message body cannot be empty");
    }

    // Validate recipient exists and is running
    ik_db_agent_row_t *agent_row = NULL;
    res_t res = ik_db_agent_get(db_ctx, ctx, recipient_uuid, &agent_row);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        if (error_msg_out != NULL) {  // LCOV_EXCL_BR_LINE
            *error_msg_out = talloc_asprintf(ctx, "Failed to query recipient: %s", error_message(res.err));
        }
        return res;  // LCOV_EXCL_LINE
    }

    if (strcmp(agent_row->status, "running") != 0) {  // LCOV_EXCL_BR_LINE
        if (error_msg_out != NULL) {  // LCOV_EXCL_BR_LINE
            *error_msg_out = talloc_strdup(ctx, "Recipient agent is dead");
        }
        return ERR(ctx, INVALID_ARG, "Recipient agent is dead");
    }

    // Create mail message
    ik_mail_msg_t *msg = ik_mail_msg_create(ctx, sender_uuid, recipient_uuid, body);
    if (msg == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    // Insert into database
    res = ik_db_mail_insert(db_ctx, session_id, msg);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        if (error_msg_out != NULL) {  // LCOV_EXCL_BR_LINE
            *error_msg_out = talloc_asprintf(ctx, "Failed to insert mail: %s", error_message(res.err));
        }
        return res;  // LCOV_EXCL_LINE
    }

    // Fire PG NOTIFY to wake up recipient (best effort - skip if in transaction)
    // NOTIFY requires autocommit and will abort the transaction if executed inside one
    PGTransactionStatusType tx_status = PQtransactionStatus(db_ctx->conn);
    if (tx_status == PQTRANS_IDLE) {  // Only notify when NOT in a transaction
        char *channel = talloc_asprintf(ctx, "agent_event_%s", recipient_uuid);
        if (channel == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
        res = ik_db_notify(db_ctx, channel, "mail");
        talloc_free(channel);
        // Ignore NOTIFY errors - wake-up is best effort, mail delivery is what matters
        if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
            talloc_free(res.err);  // LCOV_EXCL_LINE
        }
    }

    return OK(NULL);
}

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
    char uuid[256];
    if (!ik_mail_parse_uuid(args, uuid)) {     // LCOV_EXCL_BR_LINE
        const char *err = "Usage: /send <uuid> \"message\"";
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

    // Call core send logic
    char *error_msg = NULL;
    res_t res = ik_send_core(ctx, repl->shared->db_ctx, repl->shared->session_id,
                             repl->current->uuid, recipient->uuid, body, &error_msg);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        if (error_msg != NULL) {  // LCOV_EXCL_BR_LINE
            ik_scrollback_append_line(repl->current->scrollback, error_msg, strlen(error_msg));
            talloc_free(error_msg);
        }
        // Slash commands always return OK - errors shown in scrollback only
        return OK(NULL);
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
