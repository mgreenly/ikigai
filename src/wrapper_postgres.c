// PostgreSQL wrapper implementations
// Link seams that tests can override for failure injection
//
// In release builds (NDEBUG), these are defined as static inline in the header.
// In debug/test builds, these are compiled as weak symbols.

#include "wrapper_postgres.h"

#ifndef NDEBUG
// LCOV_EXCL_START

#include <libpq-fe.h>

// ============================================================================
// PostgreSQL wrappers - debug/test builds only
// ============================================================================

MOCKABLE PGresult *pq_exec_(PGconn *conn, const char *command)
{
    return PQexec(conn, command);
}

MOCKABLE char *PQgetvalue_(const PGresult *res, int row_number, int column_number)
{
    return PQgetvalue(res, row_number, column_number);
}

MOCKABLE PGresult *pq_exec_params_(PGconn *conn,
                                   const char *command,
                                   int nParams,
                                   const Oid *paramTypes,
                                   const char *const *paramValues,
                                   const int *paramLengths,
                                   const int *paramFormats,
                                   int resultFormat)
{
    return PQexecParams(conn, command, nParams, paramTypes, paramValues, paramLengths, paramFormats, resultFormat);
}

MOCKABLE ExecStatusType PQresultStatus_(const PGresult *res)
{
    return PQresultStatus(res);
}

// LCOV_EXCL_STOP
#endif
