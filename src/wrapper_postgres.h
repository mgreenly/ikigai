// PostgreSQL libpq wrappers for testing
#ifndef IK_WRAPPER_POSTGRES_H
#define IK_WRAPPER_POSTGRES_H

#include <libpq-fe.h>
#include "wrapper_base.h"

#ifdef NDEBUG

MOCKABLE char *PQgetvalue_(const PGresult *res, int row_number, int column_number)
{
    return PQgetvalue(res, row_number, column_number);
}

MOCKABLE PGresult *pq_exec_(PGconn *conn, const char *command)
{
    return PQexec(conn, command);
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

#else

MOCKABLE char *PQgetvalue_(const PGresult *res, int row_number, int column_number);
MOCKABLE PGresult *pq_exec_(PGconn *conn, const char *command);
MOCKABLE PGresult *pq_exec_params_(PGconn *conn,
                                   const char *command,
                                   int nParams,
                                   const Oid *paramTypes,
                                   const char *const *paramValues,
                                   const int *paramLengths,
                                   const int *paramFormats,
                                   int resultFormat);

#endif

#endif // IK_WRAPPER_POSTGRES_H
