#ifndef IK_DB_NOTIFY_H
#define IK_DB_NOTIFY_H

#include "apps/ikigai/db/connection.h"
#include "shared/error.h"

#include <inttypes.h>

// Notification callback - called for each notification received
// ctx: user-provided context pointer
// channel: notification channel name
// payload: notification payload string (may be empty)
typedef void (*ik_db_notify_callback_t)(void *ctx, const char *channel, const char *payload);

// Start listening on a PostgreSQL NOTIFY channel
// Executes LISTEN <channel> synchronously
res_t ik_db_listen(ik_db_ctx_t *db_ctx, const char *channel);

// Stop listening on a PostgreSQL NOTIFY channel
// Executes UNLISTEN <channel> synchronously
res_t ik_db_unlisten(ik_db_ctx_t *db_ctx, const char *channel);

// Send a notification on a PostgreSQL NOTIFY channel
// Executes NOTIFY <channel>, '<payload>' synchronously
res_t ik_db_notify(ik_db_ctx_t *db_ctx, const char *channel, const char *payload);

// Get the socket file descriptor for use with select()
// Returns the underlying PostgreSQL connection socket FD
int32_t ik_db_socket_fd(ik_db_ctx_t *db_ctx);

// Consume pending notifications and invoke callback for each
// Calls PQconsumeInput() then loops over PQnotifies()
// Returns number of notifications processed
res_t ik_db_consume_notifications(ik_db_ctx_t *db_ctx, ik_db_notify_callback_t callback, void *user_data);

#endif // IK_DB_NOTIFY_H
