// Internal ikigai function wrappers for testing
#ifndef IK_WRAPPER_INTERNAL_H
#define IK_WRAPPER_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <talloc.h>
#include "wrapper_base.h"

#ifdef NDEBUG
#include "db/connection.h"
#include "db/message.h"
#include "config.h"
#include "scrollback.h"
#include "msg.h"
#include "openai/client.h"

MOCKABLE res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, void **out_ctx)
{
    return ik_db_init(mem_ctx, conn_str, (ik_db_ctx_t **)out_ctx);
}

MOCKABLE res_t ik_db_message_insert_(ik_db_ctx_t *db,
                                     int64_t session_id,
                                     const char *agent_uuid,
                                     const char *kind,
                                     const char *content,
                                     const char *data_json)
{
    return ik_db_message_insert(db, session_id, agent_uuid, kind, content, data_json);
}

MOCKABLE res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length)
{
    return ik_scrollback_append_line((ik_scrollback_t *)scrollback, text, length);
}

MOCKABLE res_t ik_openai_conversation_add_msg_(ik_openai_conversation_t *conv, ik_msg_t *msg)
{
    return ik_openai_conversation_add_msg(conv, msg);
}

#else
// Note: These use void* because the actual types are defined in headers that may
// not be included when wrapper.h is processed
#include "error.h"

MOCKABLE res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, void **out_ctx);
MOCKABLE res_t ik_db_message_insert_(void *db,
                                     int64_t session_id,
                                     const char *agent_uuid,
                                     const char *kind,
                                     const char *content,
                                     const char *data_json);
MOCKABLE res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length);
MOCKABLE res_t ik_openai_conversation_add_msg_(void *conv, void *msg);
#endif

#endif // IK_WRAPPER_INTERNAL_H
