#pragma once

#include "connection.h"
#include "../mail/msg.h"
#include "../error.h"

#include <stdint.h>
#include <talloc.h>

// Insert mail message (sets msg->id on success)
res_t ik_db_mail_insert(ik_db_ctx_t *db, int64_t session_id, ik_mail_msg_t *msg);

// Get inbox for agent (unread first, then by timestamp desc)
res_t ik_db_mail_inbox(ik_db_ctx_t *db,
                       TALLOC_CTX *ctx,
                       int64_t session_id,
                       const char *to_uuid,
                       ik_mail_msg_t ***out,
                       size_t *count);

// Mark message as read
res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t mail_id);

// Delete message (validates recipient owns the message)
res_t ik_db_mail_delete(ik_db_ctx_t *db, int64_t mail_id, const char *recipient_uuid);

// Get filtered inbox by sender (unread first, then by timestamp desc)
res_t ik_db_mail_inbox_filtered(ik_db_ctx_t *db,
                                TALLOC_CTX *ctx,
                                int64_t session_id,
                                const char *to_uuid,
                                const char *from_uuid,
                                ik_mail_msg_t ***out,
                                size_t *count);
