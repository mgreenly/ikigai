// Core wait logic - shared between slash command and internal tool
#include "apps/ikigai/commands_wait_core.h"

#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/mail.h"
#include "apps/ikigai/db/notify.h"
#include "apps/ikigai/mail/msg.h"
#include "shared/panic.h"
#include "shared/wrapper_talloc.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>
#include <time.h>

#include "shared/poison.h"

// Notification callback context
typedef struct {
    const char *channel;
    const char *payload;
    bool received;
} ik_notify_ctx_t;

// Notification callback
// LCOV_EXCL_START - Called by PG NOTIFY; tested in integration tests
static void notify_callback(void *user_data, const char *channel, const char *payload)
{
    ik_notify_ctx_t *ctx = (ik_notify_ctx_t *)user_data;
    ctx->channel = channel;
    ctx->payload = payload;
    ctx->received = true;
}
// LCOV_EXCL_STOP

// Check for mail (optionally filtered by sender)
static ik_mail_msg_t *check_mail(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id,
                                  const char *my_uuid, const char *from_uuid)
{
    ik_mail_msg_t **msgs = NULL;
    size_t count = 0;
    res_t res;
    if (from_uuid != NULL) {
        res = ik_db_mail_inbox_filtered(db_ctx, ctx, session_id, my_uuid, from_uuid, &msgs, &count);
    } else {
        res = ik_db_mail_inbox(db_ctx, ctx, session_id, my_uuid, &msgs, &count);
    }
    if (is_err(&res) || count == 0) {  // LCOV_EXCL_BR_LINE
        return NULL;
    }
    return msgs[0];
}

// Check and update single target status in fan-in mode
static bool update_target_status(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id,
                                  const char *my_uuid, const char *target_uuid,
                                  ik_wait_fanin_entry_t *entry)
{
    if (strcmp(entry->status, "running") != 0) {  // LCOV_EXCL_BR_LINE
        return true;  // Already resolved  // LCOV_EXCL_LINE
    }

    ik_mail_msg_t *msg = check_mail(ctx, db_ctx, session_id, my_uuid, target_uuid);
    if (msg != NULL) {
        entry->status = talloc_strdup(ctx, "received");
        entry->message = talloc_strdup(ctx, msg->body);
        ik_db_mail_delete(db_ctx, msg->id, my_uuid);
        return true;
    }

    ik_db_agent_row_t *agent = NULL;
    res_t agent_res = ik_db_agent_get(db_ctx, ctx, target_uuid, &agent);
    if (is_ok(&agent_res)) {  // LCOV_EXCL_BR_LINE
        if (strcmp(agent->status, "dead") == 0) {
            entry->status = talloc_strdup(ctx, "dead");
            return true;
        } else if (agent->idle) {  // LCOV_EXCL_BR_LINE
            entry->status = talloc_strdup(ctx, "idle");
            return true;
        }
    }
    return false;  // LCOV_EXCL_LINE
}

// Core wait logic - Mode 1: next message
void ik_wait_core_next_message(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id,
                                const char *my_uuid, int32_t timeout_sec, bool *interrupted,
                                ik_wait_result_t *result)
{
    char *my_channel = talloc_asprintf(ctx, "agent_event_%s", my_uuid);
    if (my_channel == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    res_t res = ik_db_listen(db_ctx, my_channel);
    if (is_err(&res)) {
        result->from_uuid = NULL;
        result->message = talloc_strdup(ctx, "Failed to LISTEN");
        return;
    }

    ik_mail_msg_t *msg = check_mail(ctx, db_ctx, session_id, my_uuid, NULL);
    if (msg != NULL) {
        result->from_uuid = talloc_strdup(ctx, msg->from_uuid);
        result->message = talloc_strdup(ctx, msg->body);
        ik_db_mail_delete(db_ctx, msg->id, my_uuid);
        ik_db_unlisten(db_ctx, my_channel);
        return;
    }

    int32_t sock_fd = ik_db_socket_fd(db_ctx);
    if (sock_fd < 0) {  // LCOV_EXCL_BR_LINE - Bug B fix; tested in integration
        result->from_uuid = NULL;  // LCOV_EXCL_LINE
        result->message = talloc_strdup(ctx, "Database connection error");  // LCOV_EXCL_LINE
        ik_db_unlisten(db_ctx, my_channel);  // LCOV_EXCL_LINE
        return;  // LCOV_EXCL_LINE
    }

    // LCOV_EXCL_START - Select loop tested in integration tests
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (true) {
        if (*interrupted) {
            result->from_uuid = NULL;
            result->message = talloc_strdup(ctx, "Interrupted");
            break;
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
        int64_t remaining_ms = (int64_t)timeout_sec * 1000 - elapsed_ms;
        if (remaining_ms <= 0) {
            result->from_uuid = NULL;
            result->message = talloc_strdup(ctx, "Timeout");
            break;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock_fd, &readfds);
        int64_t select_ms = remaining_ms < 50 ? remaining_ms : 50;
        struct timeval tv;
        tv.tv_sec = select_ms / 1000;
        tv.tv_usec = (select_ms % 1000) * 1000;

        int select_result = select(sock_fd + 1, &readfds, NULL, NULL, &tv);
        if (select_result < 0) {
            if (errno == EINTR) continue;
            result->from_uuid = NULL;
            result->message = talloc_strdup(ctx, "Select failed");
            break;
        }
        if (select_result == 0) continue;  // Poll interval expired, loop to check interrupt/timeout

        ik_notify_ctx_t notify_ctx = {0};
        ik_db_consume_notifications(db_ctx, notify_callback, &notify_ctx);

        msg = check_mail(ctx, db_ctx, session_id, my_uuid, NULL);
        if (msg != NULL) {
            result->from_uuid = talloc_strdup(ctx, msg->from_uuid);
            result->message = talloc_strdup(ctx, msg->body);
            ik_db_mail_delete(db_ctx, msg->id, my_uuid);
            break;
        }
    }
    // LCOV_EXCL_STOP

    ik_db_unlisten(db_ctx, my_channel);
}

// Core wait logic - Mode 2: fan-in
void ik_wait_core_fanin(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id,
                        const char *my_uuid, int32_t timeout_sec, char **target_uuids,
                        size_t target_count, bool *interrupted, ik_wait_result_t *result)
{
    char *my_channel = talloc_asprintf(ctx, "agent_event_%s", my_uuid);
    if (my_channel == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    res_t res = ik_db_listen(db_ctx, my_channel);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        result->entries = NULL;  // LCOV_EXCL_LINE
        result->entry_count = 0;  // LCOV_EXCL_LINE
        return;  // LCOV_EXCL_LINE
    }

    for (size_t i = 0; i < target_count; i++) {
        char *target_channel = talloc_asprintf(ctx, "agent_event_%s", target_uuids[i]);
        if (target_channel == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        ik_db_listen(db_ctx, target_channel);
    }

    result->entries = talloc_array_(ctx, sizeof(ik_wait_fanin_entry_t), target_count);
    if (result->entries == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    result->entry_count = target_count;

    for (size_t i = 0; i < target_count; i++) {
        result->entries[i].agent_uuid = talloc_strdup(ctx, target_uuids[i]);
        result->entries[i].agent_name = talloc_strdup(ctx, "undefined");
        result->entries[i].status = talloc_strdup(ctx, "running");
        result->entries[i].message = NULL;
    }

    ik_db_agent_name_entry_t *name_entries = NULL;
    size_t name_count = 0;
    res_t name_res = ik_db_agent_get_names_batch(db_ctx, ctx, target_uuids, target_count,
                                                   &name_entries, &name_count);
    if (is_err(&name_res) || name_entries == NULL) {
        goto skip_name_lookup;
    }

    for (size_t i = 0; i < target_count; i++) {
        for (size_t j = 0; j < name_count; j++) {
            bool match = strcmp(result->entries[i].agent_uuid, name_entries[j].uuid) == 0;
            if (!match) continue;
            if (name_entries[j].name == NULL) break;
            result->entries[i].agent_name = talloc_strdup(ctx, name_entries[j].name);
            break;
        }
    }

skip_name_lookup:

    int32_t sock_fd = ik_db_socket_fd(db_ctx);
    if (sock_fd < 0) {  // LCOV_EXCL_BR_LINE - Bug B fix; tested in integration
        ik_db_unlisten(db_ctx, my_channel);  // LCOV_EXCL_LINE
        for (size_t i = 0; i < target_count; i++) {  // LCOV_EXCL_LINE
            char *target_channel = talloc_asprintf(ctx, "agent_event_%s", target_uuids[i]);  // LCOV_EXCL_LINE
            if (target_channel == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            ik_db_unlisten(db_ctx, target_channel);  // LCOV_EXCL_LINE
        }
        return;  // LCOV_EXCL_LINE
    }

    // LCOV_EXCL_START - Fanin select loop tested in integration tests
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (true) {
        if (*interrupted) {
            break;
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
        int64_t remaining_ms = (int64_t)timeout_sec * 1000 - elapsed_ms;
        if (remaining_ms <= 0) {
            break;
        }

        bool all_resolved = true;
        for (size_t i = 0; i < target_count; i++) {
            bool resolved = update_target_status(ctx, db_ctx, session_id, my_uuid,
                                                  target_uuids[i], &result->entries[i]);
            if (!resolved) all_resolved = false;
        }

        if (all_resolved) {
            break;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock_fd, &readfds);
        int64_t select_ms = remaining_ms < 50 ? remaining_ms : 50;
        struct timeval tv;
        tv.tv_sec = select_ms / 1000;
        tv.tv_usec = (select_ms % 1000) * 1000;

        int select_result = select(sock_fd + 1, &readfds, NULL, NULL, &tv);
        if (select_result < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (select_result == 0) continue;  // Poll interval expired, loop to check interrupt/timeout

        ik_notify_ctx_t notify_ctx = {0};
        ik_db_consume_notifications(db_ctx, notify_callback, &notify_ctx);
    }
    // LCOV_EXCL_STOP

    ik_db_unlisten(db_ctx, my_channel);
    for (size_t i = 0; i < target_count; i++) {
        char *target_channel = talloc_asprintf(ctx, "agent_event_%s", target_uuids[i]);
        if (target_channel == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        ik_db_unlisten(db_ctx, target_channel);
    }
}
