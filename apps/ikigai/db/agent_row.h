#ifndef IK_DB_AGENT_ROW_H
#define IK_DB_AGENT_ROW_H

#include "apps/ikigai/db/agent.h"

#include <libpq-fe.h>
#include <talloc.h>

/**
 * Parse an agent row from a PGresult
 *
 * Extracts all fields from a single row in a PGresult and allocates
 * an ik_db_agent_row_t structure on the provided context.
 *
 * @param db_ctx Database context for error allocation (must not be NULL)
 * @param ctx Talloc context for row allocation (must not be NULL)
 * @param res PGresult containing query data (must not be NULL)
 * @param row_index Row number to extract (must be valid)
 * @param out Output parameter for parsed row (must not be NULL)
 * @return OK with parsed row on success, ERR on failure
 */
res_t ik_db_agent_parse_row(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, PGresult *res, int row_index,
                            ik_db_agent_row_t **out);

#endif // IK_DB_AGENT_ROW_H
