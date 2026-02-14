#include "apps/ikigai/db/agent.h"

#include "apps/ikigai/db/pg_result.h"
#include "apps/ikigai/tmp_ctx.h"
#include "apps/ikigai/wrapper_postgres.h"
#include "shared/panic.h"

#include <assert.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <stddef.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

res_t ik_db_agent_get_names_batch(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                                   char **uuids, size_t uuid_count,
                                   ik_db_agent_name_entry_t **out, size_t *out_count)
{
    assert(db_ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(uuids != NULL);     // LCOV_EXCL_BR_LINE
    assert(out != NULL);       // LCOV_EXCL_BR_LINE
    assert(out_count != NULL); // LCOV_EXCL_BR_LINE

    if (uuid_count == 0) {
        *out = NULL;
        *out_count = 0;
        return OK(NULL);
    }

    TALLOC_CTX *tmp = tmp_ctx_create();

    char *query = talloc_strdup(tmp, "SELECT uuid, name FROM agents WHERE uuid IN (");
    if (query == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < uuid_count; i++) {
        char *next = talloc_asprintf_append_buffer(query, "$%" PRIu64, (uint64_t)(i + 1));
        if (next == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        query = next;

        if (i < uuid_count - 1) {
            next = talloc_asprintf_append_buffer(query, ",");
            if (next == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            query = next;
        }
    }

    char *final_query = talloc_asprintf_append_buffer(query, ")");
    if (final_query == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    query = final_query;

    const char **param_values = talloc_array(tmp, const char *, (unsigned int)uuid_count);
    if (param_values == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    for (size_t i = 0; i < uuid_count; i++) {
        param_values[i] = uuids[i];
    }

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db_ctx->conn, query, (int32_t)uuid_count, NULL,
                                                  param_values, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        talloc_free(tmp);
        return ERR(db_ctx, IO, "Failed to get agent names: %s", pq_err);
    }

    int32_t num_rows = PQntuples(res);
    *out_count = (size_t)num_rows;

    if (num_rows == 0) {
        *out = NULL;
        talloc_free(tmp);
        return OK(NULL);
    }

    ik_db_agent_name_entry_t *entries = talloc_array(ctx, ik_db_agent_name_entry_t, (unsigned int)num_rows);
    if (entries == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (int32_t i = 0; i < num_rows; i++) {
        const char *uuid_str = PQgetvalue_(res, i, 0);
        entries[i].uuid = talloc_strdup(ctx, uuid_str);
        if (entries[i].uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        if (PQgetisnull(res, i, 1)) {
            entries[i].name = NULL;
        } else {
            const char *name_str = PQgetvalue_(res, i, 1);
            entries[i].name = talloc_strdup(ctx, name_str);
            if (entries[i].name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
    }

    *out = entries;
    talloc_free(tmp);
    return OK(NULL);
}
