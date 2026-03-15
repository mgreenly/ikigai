/**
 * @file bg_startup.c
 * @brief Startup recovery scan for background processes
 */

#include "apps/ikigai/bg_startup.h"

#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <talloc.h>

#include "apps/ikigai/db/pg_result.h"
#include "apps/ikigai/tmp_ctx.h"
#include "apps/ikigai/wrapper_postgres.h"
#include "shared/wrapper.h"

#include "shared/poison.h"

/* ================================================================
 * DB helpers
 * ================================================================ */

static void db_mark_killed(ik_db_ctx_t *db, int32_t id)
{
    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "UPDATE background_processes SET status = 'killed' WHERE id = $1";

    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%" PRId32, id); /* NOLINT */

    const char *params[1] = { id_str };
    ik_pg_result_wrapper_t *wr =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 1,
                                                   NULL, params, NULL, NULL, 0));
    (void)wr;
    talloc_free(tmp);
}

static void db_mark_exited(ik_db_ctx_t *db, int32_t id)
{
    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "UPDATE background_processes "
        "SET status = 'exited', exit_code = -1 "
        "WHERE id = $1";

    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%" PRId32, id); /* NOLINT */

    const char *params[1] = { id_str };
    ik_pg_result_wrapper_t *wr =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 1,
                                                   NULL, params, NULL, NULL, 0));
    (void)wr;
    talloc_free(tmp);
}

/* ================================================================
 * bg_startup_recover
 * ================================================================ */

void bg_startup_recover(ik_db_ctx_t *db)
{
    if (db == NULL) return;

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "SELECT id, pid FROM background_processes "
        "WHERE status IN ('starting', 'running')";

    ik_pg_result_wrapper_t *wr =
        ik_db_wrap_pg_result(tmp, pq_exec_(db->conn, query));
    PGresult *res = wr->pg_result;

    if (PQresultStatus_(res) != PGRES_TUPLES_OK) {
        talloc_free(tmp);
        return;
    }

    int nrows = PQntuples_(res);
    for (int i = 0; i < nrows; i++) {
        const char *id_str  = PQgetvalue_(res, i, 0);
        const char *pid_str = PQgetvalue_(res, i, 1);

        int32_t id  = (int32_t)strtol(id_str,  NULL, 10);
        pid_t   pid = (pid_t)  strtol(pid_str, NULL, 10);

        if (pid <= 0) {
            db_mark_exited(db, id);
            continue;
        }

        if (kill_(pid, 0) == 0) {
            /* Process is still alive — kill the group and mark KILLED */
            kill_(-pid, SIGKILL);
            db_mark_killed(db, id);
        } else {
            /* Process is already dead — mark EXITED */
            db_mark_exited(db, id);
        }
    }

    talloc_free(tmp);
}
