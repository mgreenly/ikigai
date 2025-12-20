#include "agent_row.h"

#include "../error.h"
#include "../panic.h"
#include "../wrapper.h"

#include <assert.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <talloc.h>

res_t ik_db_agent_parse_row(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                             PGresult *res, int row_index,
                             ik_db_agent_row_t **out)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(res != NULL);     // LCOV_EXCL_BR_LINE
    assert(out != NULL);     // LCOV_EXCL_BR_LINE

    // Allocate row on provided context
    ik_db_agent_row_t *row = talloc_zero(ctx, ik_db_agent_row_t);
    if (row == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Extract uuid (column 0)
    row->uuid = talloc_strdup(row, PQgetvalue_(res, row_index, 0));
    if (row->uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Extract name (column 1) - nullable
    if (!PQgetisnull(res, row_index, 1)) {
        row->name = talloc_strdup(row, PQgetvalue_(res, row_index, 1));
        if (row->name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        row->name = NULL;
    }

    // Extract parent_uuid (column 2) - nullable
    if (!PQgetisnull(res, row_index, 2)) {
        row->parent_uuid = talloc_strdup(row, PQgetvalue_(res, row_index, 2));
        if (row->parent_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        row->parent_uuid = NULL;
    }

    // Extract fork_message_id (column 3)
    row->fork_message_id = talloc_strdup(row, PQgetvalue_(res, row_index, 3));
    if (row->fork_message_id == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Extract status (column 4)
    row->status = talloc_strdup(row, PQgetvalue_(res, row_index, 4));
    if (row->status == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Parse created_at (column 5)
    const char *created_at_str = PQgetvalue_(res, row_index, 5);
    if (sscanf(created_at_str, "%lld", (long long *)&row->created_at) != 1) {
        return ERR(db_ctx, PARSE, "Failed to parse created_at");
    }

    // Parse ended_at (column 6)
    const char *ended_at_str = PQgetvalue_(res, row_index, 6);
    if (sscanf(ended_at_str, "%lld", (long long *)&row->ended_at) != 1) {
        return ERR(db_ctx, PARSE, "Failed to parse ended_at");
    }

    *out = row;
    return OK(NULL);
}
