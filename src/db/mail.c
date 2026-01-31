#include "mail.h"
#include "pg_result.h"
#include "../error.h"
#include "../tmp_ctx.h"
#include "../wrapper.h"

#include <assert.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>


#include "poison.h"
res_t ik_db_mail_insert(ik_db_ctx_t *db, int64_t session_id,
                        ik_mail_msg_t *msg)
{
    assert(db != NULL);                  // LCOV_EXCL_BR_LINE
    assert(db->conn != NULL);            // LCOV_EXCL_BR_LINE
    assert(session_id > 0);              // LCOV_EXCL_BR_LINE
    assert(msg != NULL);                 // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "INSERT INTO mail (session_id, from_uuid, to_uuid, body, timestamp) "
        "VALUES ($1, $2, $3, $4, $5) RETURNING id";

    char *session_id_str = talloc_asprintf(tmp, "%lld", (long long)session_id);
    if (session_id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *timestamp_str = talloc_asprintf(tmp, "%lld", (long long)msg->timestamp);
    if (timestamp_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *params[5];
    params[0] = session_id_str;
    params[1] = msg->from_uuid;
    params[2] = msg->to_uuid;
    params[3] = msg->body;
    params[4] = timestamp_str;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 5, NULL, params, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        res_t error_res = ERR(db, IO, "Mail insert failed: %s", pq_err);
        talloc_free(tmp);
        return error_res;
    }

    // Get the returned ID
    const char *id_str = PQgetvalue(res, 0, 0);
    msg->id = strtoll(id_str, NULL, 10);

    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_mail_inbox(ik_db_ctx_t *db, TALLOC_CTX *ctx,
                       int64_t session_id, const char *to_uuid,
                       ik_mail_msg_t ***out, size_t *count)
{
    assert(db != NULL);            // LCOV_EXCL_BR_LINE
    assert(db->conn != NULL);      // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(session_id > 0);        // LCOV_EXCL_BR_LINE
    assert(to_uuid != NULL);       // LCOV_EXCL_BR_LINE
    assert(out != NULL);           // LCOV_EXCL_BR_LINE
    assert(count != NULL);         // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "SELECT id, from_uuid, to_uuid, body, timestamp, read "
        "FROM mail "
        "WHERE session_id = $1 AND to_uuid = $2 "
        "ORDER BY read ASC, timestamp DESC";

    char *session_id_str = talloc_asprintf(tmp, "%lld", (long long)session_id);
    if (session_id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *params[2];
    params[0] = session_id_str;
    params[1] = to_uuid;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 2, NULL, params, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        res_t error_res = ERR(db, IO, "Mail inbox query failed: %s", pq_err);
        talloc_free(tmp);
        return error_res;
    }

    int nrows = PQntuples(res);
    *count = (size_t)nrows;

    if (nrows == 0) {
        *out = NULL;
        talloc_free(tmp);
        return OK(NULL);
    }

    // Allocate array of pointers
    ik_mail_msg_t **messages = talloc_array(ctx, ik_mail_msg_t *, (unsigned int)nrows);
    if (messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Build message structures
    for (int i = 0; i < nrows; i++) {
        ik_mail_msg_t *msg = talloc_zero(messages, ik_mail_msg_t);
        if (msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        const char *id_str = PQgetvalue(res, i, 0);
        msg->id = strtoll(id_str, NULL, 10);

        msg->from_uuid = talloc_strdup(msg, PQgetvalue(res, i, 1));
        if (msg->from_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        msg->to_uuid = talloc_strdup(msg, PQgetvalue(res, i, 2));
        if (msg->to_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        msg->body = talloc_strdup(msg, PQgetvalue(res, i, 3));
        if (msg->body == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        const char *timestamp_str = PQgetvalue(res, i, 4);
        msg->timestamp = strtoll(timestamp_str, NULL, 10);

        const char *read_str = PQgetvalue(res, i, 5);
        msg->read = (strcmp(read_str, "1") == 0);

        messages[i] = msg;
    }

    *out = messages;
    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t mail_id)
{
    assert(db != NULL);        // LCOV_EXCL_BR_LINE
    assert(db->conn != NULL);  // LCOV_EXCL_BR_LINE
    assert(mail_id > 0);       // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query = "UPDATE mail SET read = 1 WHERE id = $1";

    char *mail_id_str = talloc_asprintf(tmp, "%lld", (long long)mail_id);
    if (mail_id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *params[1];
    params[0] = mail_id_str;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 1, NULL, params, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        res_t error_res = ERR(db, IO, "Mail mark read failed: %s", pq_err);
        talloc_free(tmp);
        return error_res;
    }

    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_mail_delete(ik_db_ctx_t *db, int64_t mail_id,
                        const char *recipient_uuid)
{
    assert(db != NULL);              // LCOV_EXCL_BR_LINE
    assert(db->conn != NULL);        // LCOV_EXCL_BR_LINE
    assert(mail_id > 0);             // LCOV_EXCL_BR_LINE
    assert(recipient_uuid != NULL);  // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query = "DELETE FROM mail WHERE id = $1 AND to_uuid = $2";

    char *mail_id_str = talloc_asprintf(tmp, "%lld", (long long)mail_id);
    if (mail_id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *params[2];
    params[0] = mail_id_str;
    params[1] = recipient_uuid;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 2, NULL, params, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        res_t error_res = ERR(db, IO, "Mail delete failed: %s", pq_err);
        talloc_free(tmp);
        return error_res;
    }

    // Check if any rows were affected (mail existed and belonged to recipient)
    char *rows_affected = PQcmdTuples(res);
    if (strcmp(rows_affected, "0") == 0) {
        res_t error_res = ERR(db, IO, "Mail not found or not yours");
        talloc_free(tmp);
        return error_res;
    }

    talloc_free(tmp);
    return OK(NULL);
}

res_t ik_db_mail_inbox_filtered(ik_db_ctx_t *db, TALLOC_CTX *ctx,
                                int64_t session_id, const char *to_uuid,
                                const char *from_uuid,
                                ik_mail_msg_t ***out, size_t *count)
{
    assert(db != NULL);         // LCOV_EXCL_BR_LINE
    assert(db->conn != NULL);   // LCOV_EXCL_BR_LINE
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(session_id > 0);     // LCOV_EXCL_BR_LINE
    assert(to_uuid != NULL);    // LCOV_EXCL_BR_LINE
    assert(from_uuid != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);        // LCOV_EXCL_BR_LINE
    assert(count != NULL);      // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "SELECT id, from_uuid, to_uuid, body, timestamp, read "
        "FROM mail "
        "WHERE session_id = $1 AND to_uuid = $2 AND from_uuid = $3 "
        "ORDER BY read ASC, timestamp DESC";

    char *session_id_str = talloc_asprintf(tmp, "%lld", (long long)session_id);
    if (session_id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *params[3];
    params[0] = session_id_str;
    params[1] = to_uuid;
    params[2] = from_uuid;

    ik_pg_result_wrapper_t *res_wrapper =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 3, NULL, params, NULL, NULL, 0));
    PGresult *res = res_wrapper->pg_result;

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        res_t error_res = ERR(db, IO, "Mail filtered inbox query failed: %s", pq_err);
        talloc_free(tmp);
        return error_res;
    }

    int nrows = PQntuples(res);
    *count = (size_t)nrows;

    if (nrows == 0) {
        *out = NULL;
        talloc_free(tmp);
        return OK(NULL);
    }

    // Allocate array of pointers
    ik_mail_msg_t **messages = talloc_array(ctx, ik_mail_msg_t *, (unsigned int)nrows);
    if (messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Build message structures
    for (int i = 0; i < nrows; i++) {
        ik_mail_msg_t *msg = talloc_zero(messages, ik_mail_msg_t);
        if (msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        const char *id_str = PQgetvalue(res, i, 0);
        msg->id = strtoll(id_str, NULL, 10);

        msg->from_uuid = talloc_strdup(msg, PQgetvalue(res, i, 1));
        if (msg->from_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        msg->to_uuid = talloc_strdup(msg, PQgetvalue(res, i, 2));
        if (msg->to_uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        msg->body = talloc_strdup(msg, PQgetvalue(res, i, 3));
        if (msg->body == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        const char *timestamp_str = PQgetvalue(res, i, 4);
        msg->timestamp = strtoll(timestamp_str, NULL, 10);

        const char *read_str = PQgetvalue(res, i, 5);
        msg->read = (strcmp(read_str, "1") == 0);

        messages[i] = msg;
    }

    *out = messages;
    talloc_free(tmp);
    return OK(NULL);
}
