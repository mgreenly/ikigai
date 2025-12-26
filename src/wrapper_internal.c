// Internal ikigai function wrapper implementations
// Link seams that tests can override for failure injection
//
// In release builds (NDEBUG), these are defined as static inline in the header.
// In debug/test builds, these are compiled as weak symbols.

#include "wrapper_internal.h"

#ifndef NDEBUG
// LCOV_EXCL_START

#include "config.h"
#include "db/connection.h"
#include "db/message.h"
#include "msg.h"
#include "scrollback.h"

// ============================================================================
// Internal ikigai function wrappers for testing - debug/test builds only
// ============================================================================

MOCKABLE res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, void **out_ctx)
{
    return ik_db_init(mem_ctx, conn_str, (ik_db_ctx_t **)out_ctx);
}

MOCKABLE res_t ik_db_message_insert_(void *db,
                                     int64_t session_id,
                                     const char *agent_uuid,
                                     const char *kind,
                                     const char *content,
                                     const char *data_json)
{
    return ik_db_message_insert((ik_db_ctx_t *)db, session_id, agent_uuid, kind, content, data_json);
}

MOCKABLE res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length)
{
    return ik_scrollback_append_line((ik_scrollback_t *)scrollback, text, length);
}

// LCOV_EXCL_STOP
#endif
