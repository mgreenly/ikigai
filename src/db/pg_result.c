// src/db/pg_result.c - PGresult wrapper implementation

#include "db/pg_result.h"

#include "panic.h"

#include <libpq-fe.h>
#include <talloc.h>

// Destructor called automatically when talloc frees the wrapper
static int32_t pg_result_destructor(ik_pg_result_wrapper_t *wrapper)
{
    if (wrapper->pg_result != NULL) {
        PQclear(wrapper->pg_result);
        wrapper->pg_result = NULL;
    }
    return 0;
}

ik_pg_result_wrapper_t *ik_db_wrap_pg_result(TALLOC_CTX *ctx, PGresult *pg_res)
{
    ik_pg_result_wrapper_t *wrapper = talloc_zero(ctx, ik_pg_result_wrapper_t);
    if (wrapper == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    wrapper->pg_result = pg_res;
    talloc_set_destructor(wrapper, pg_result_destructor);

    return wrapper;
}
