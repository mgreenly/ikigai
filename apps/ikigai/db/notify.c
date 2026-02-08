#include "apps/ikigai/db/notify.h"

#include "shared/error.h"
#include "apps/ikigai/wrapper_postgres.h"

#include "shared/panic.h"
#include "shared/poison.h"

#include <assert.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <stdint.h>
#include <talloc.h>
res_t ik_db_listen(ik_db_ctx_t *db_ctx, const char *channel)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(channel != NULL);  // LCOV_EXCL_BR_LINE

    char *query = talloc_asprintf(NULL, "LISTEN %s", channel);
    if (query == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    PGresult *res = pq_exec_(db_ctx->conn, query);
    talloc_free(query);

    if (res == NULL) PANIC("PQexec returned NULL");  // LCOV_EXCL_BR_LINE

    ExecStatusType status = PQresultStatus_(res);
    if (status != PGRES_COMMAND_OK) {
        char *err_msg = talloc_asprintf(NULL, "LISTEN failed: %s", PQresultErrorMessage(res));
        if (err_msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        PQclear(res);
        res_t result = ERR(db_ctx, DB_CONNECT, "%s", err_msg);
        talloc_free(err_msg);
        return result;
    }

    PQclear(res);
    return OK(NULL);
}

res_t ik_db_notify(ik_db_ctx_t *db_ctx, const char *channel, const char *payload)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(channel != NULL);  // LCOV_EXCL_BR_LINE
    assert(payload != NULL);  // LCOV_EXCL_BR_LINE

    char *query = talloc_asprintf(NULL, "NOTIFY %s, '%s'", channel, payload);
    if (query == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    PGresult *res = pq_exec_(db_ctx->conn, query);
    talloc_free(query);

    if (res == NULL) PANIC("PQexec returned NULL");  // LCOV_EXCL_BR_LINE

    ExecStatusType status = PQresultStatus_(res);
    if (status != PGRES_COMMAND_OK) {
        char *err_msg = talloc_asprintf(NULL, "NOTIFY failed: %s", PQresultErrorMessage(res));
        if (err_msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        PQclear(res);
        res_t result = ERR(db_ctx, DB_CONNECT, "%s", err_msg);
        talloc_free(err_msg);
        return result;
    }

    PQclear(res);
    return OK(NULL);
}

int32_t ik_db_socket_fd(ik_db_ctx_t *db_ctx)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    return PQsocket_(db_ctx->conn);
}

res_t ik_db_consume_notifications(ik_db_ctx_t *db_ctx, ik_db_notify_callback_t callback, void *user_data)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(callback != NULL);  // LCOV_EXCL_BR_LINE

    int consume_result = PQconsumeInput_(db_ctx->conn);
    if (consume_result == 0) {
        return ERR(db_ctx, DB_CONNECT, "PQconsumeInput failed: %s", PQerrorMessage(db_ctx->conn));
    }

    size_t count = 0;
    PGnotify *notify = NULL;
    while ((notify = PQnotifies_(db_ctx->conn)) != NULL) {
        callback(user_data, notify->relname, notify->extra);
        PQfreemem(notify);
        count++;
    }

    return OK((void *)(uintptr_t)count);
}
