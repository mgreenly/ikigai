// Internal ikigai function wrapper implementations
// Link seams that tests can override for failure injection
//
// In release builds (NDEBUG), these are defined as static inline in the header.
// In debug/test builds, these are compiled as weak symbols.

#include "wrapper_internal.h"

#ifndef NDEBUG
// LCOV_EXCL_START

#include "agent.h"
#include "config.h"
#include "db/connection.h"
#include "db/message.h"
#include "logger.h"
#include "msg.h"
#include "providers/common/http_multi.h"
#include "providers/request.h"
#include "repl.h"
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

MOCKABLE res_t ik_repl_render_frame_(void *repl)
{
    return ik_repl_render_frame((ik_repl_ctx_t *)repl);
}

MOCKABLE res_t ik_agent_get_provider_(void *agent, void **provider_out)
{
    return ik_agent_get_provider((ik_agent_ctx_t *)agent, (ik_provider_t **)provider_out);
}

MOCKABLE res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void **req_out)
{
    return ik_request_build_from_conversation(ctx, agent, (ik_request_t **)req_out);
}

MOCKABLE res_t ik_http_multi_create_(void *parent, void **out)
{
    res_t r = ik_http_multi_create(parent);
    if (is_ok(&r)) {
        *out = r.ok;
    }
    return r;
}

MOCKABLE void ik_http_multi_info_read_(void *http_multi, void *logger)
{
    ik_http_multi_info_read((ik_http_multi_t *)http_multi, (ik_logger_t *)logger);
}

// LCOV_EXCL_STOP
#endif
