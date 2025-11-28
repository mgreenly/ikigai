// src/db/pg_result.h - PGresult wrapper for automatic cleanup

#ifndef IK_DB_PG_RESULT_H
#define IK_DB_PG_RESULT_H

#include <libpq-fe.h>
#include <talloc.h>

// Wrapper for PGresult that integrates with talloc memory management
// Automatically calls PQclear() when the talloc context is freed
typedef struct {
    PGresult *pg_result;
} ik_pg_result_wrapper_t;

// Wraps a PGresult in a talloc-managed wrapper
// The wrapper's destructor will automatically call PQclear() on the PGresult
// when the parent talloc context is freed.
//
// mem_ctx: Parent talloc context
// pg_res: PGresult to wrap (may be NULL)
// Returns: Wrapper object (never NULL - panics on allocation failure)
ik_pg_result_wrapper_t *ik_db_wrap_pg_result(TALLOC_CTX *mem_ctx, PGresult *pg_res);

#endif // IK_DB_PG_RESULT_H
