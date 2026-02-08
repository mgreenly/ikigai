// Wait command - next message and fan-in modes
#include "apps/ikigai/commands.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_wait_core.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/panic.h"
#include "shared/wrapper_talloc.h"
#include "apps/ikigai/wrapper_pthread.h"

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

// Deferred data wrapper for on_complete callback
typedef struct {
    TALLOC_CTX *worker_ctx;
    ik_wait_result_t *result;
} ik_wait_deferred_t;

// Worker thread arguments
typedef struct {
    TALLOC_CTX *ctx;
    ik_db_ctx_t *db_ctx;
    int64_t session_id;
    char *my_uuid;
    int32_t timeout_sec;
    char **target_uuids;
    size_t target_count;
    ik_wait_result_t *result;
    pthread_mutex_t *mutex;
    bool *complete;
    bool *interrupted;
} ik_wait_worker_args_t;

// Worker thread entry point
static void *wait_worker(void *arg)
{
    ik_wait_worker_args_t *args = (ik_wait_worker_args_t *)arg;

    if (args->target_count == 0) {
        args->result->is_fanin = false;
        ik_wait_core_next_message(args->ctx, args->db_ctx, args->session_id, args->my_uuid,
                                   args->timeout_sec, args->interrupted, args->result);
    } else {
        args->result->is_fanin = true;
        ik_wait_core_fanin(args->ctx, args->db_ctx, args->session_id, args->my_uuid,
                           args->timeout_sec, args->target_uuids, args->target_count,
                           args->interrupted, args->result);
    }

    pthread_mutex_lock_(args->mutex);
    *args->complete = true;
    pthread_mutex_unlock_(args->mutex);

    return NULL;
}

// On-complete callback for wait command
static void wait_on_complete(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    (void)repl;

    // Retrieve deferred data
    ik_wait_deferred_t *deferred = (ik_wait_deferred_t *)agent->tool_deferred_data;
    if (deferred == NULL) return;  // LCOV_EXCL_BR_LINE

    ik_wait_result_t *result = deferred->result;
    TALLOC_CTX *worker_ctx = deferred->worker_ctx;

    // Render results to scrollback
    if (result->is_fanin) {
        const char *header = "Fan-in results:";
        ik_scrollback_append_line(agent->scrollback, header, strlen(header));
        for (size_t i = 0; i < result->entry_count; i++) {
            char *line = talloc_asprintf(agent, "  %s: %s%s",
                                         result->entries[i].agent_uuid,
                                         result->entries[i].status,
                                         result->entries[i].message ? " - " : "");  // LCOV_EXCL_BR_LINE
            if (result->entries[i].message) {  // LCOV_EXCL_BR_LINE
                line = talloc_asprintf_append(line, "%s", result->entries[i].message);
            }
            ik_scrollback_append_line(agent->scrollback, line, strlen(line));
            talloc_free(line);
        }
    } else {
        if (result->from_uuid != NULL) {  // LCOV_EXCL_BR_LINE
            char *line = talloc_asprintf(agent, "From: %s", result->from_uuid);
            ik_scrollback_append_line(agent->scrollback, line, strlen(line));
            ik_scrollback_append_line(agent->scrollback, result->message, strlen(result->message));
            talloc_free(line);
        } else {
            const char *msg = result->message ? result->message : "No messages";  // LCOV_EXCL_BR_LINE
            ik_scrollback_append_line(agent->scrollback, msg, strlen(msg));
        }
    }

    talloc_free(worker_ctx);
}

// /wait command handler
res_t ik_cmd_wait(void *ctx, ik_repl_ctx_t *repl, const char *args_str)
{
    (void)ctx;
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(args_str != NULL);  // LCOV_EXCL_BR_LINE

    if (repl->shared->db_ctx == NULL || repl->shared->worker_db_ctx == NULL) {  // LCOV_EXCL_BR_LINE
        const char *msg = "Error: Database not available";  // LCOV_EXCL_LINE
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));  // LCOV_EXCL_LINE
        return OK(NULL);  // LCOV_EXCL_LINE
    }

    char *args_copy = talloc_strdup(repl, args_str);
    if (args_copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *saveptr = NULL;
    char *timeout_str = strtok_r(args_copy, " ", &saveptr);
    if (timeout_str == NULL) {
        const char *msg = "Usage: /wait TIMEOUT [UUID1 UUID2 ...]";
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(args_copy);
        return OK(NULL);
    }

    char *endptr = NULL;
    long timeout_val = strtol(timeout_str, &endptr, 10);
    if (endptr == timeout_str || *endptr != '\0' || timeout_val < 0 || timeout_val > INT32_MAX) {  // LCOV_EXCL_BR_LINE
        const char *msg = "Error: Invalid timeout value";
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(args_copy);
        return OK(NULL);
    }
    int32_t timeout_sec = (int32_t)timeout_val;

    char **target_uuids = NULL;
    size_t target_count = 0;
    char *uuid_token = NULL;
    while ((uuid_token = strtok_r(NULL, " ", &saveptr)) != NULL) {
        target_uuids = talloc_realloc(repl, target_uuids, char *, (unsigned int)(target_count + 1));
        if (target_uuids == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        target_uuids[target_count] = talloc_strdup(target_uuids, uuid_token);
        if (target_uuids[target_count] == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        target_count++;
    }

    talloc_free(args_copy);

    // Set up worker context for thread
    TALLOC_CTX *worker_ctx = talloc_new(NULL);
    if (worker_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    ik_wait_result_t *result = talloc_zero(worker_ctx, ik_wait_result_t);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    ik_wait_deferred_t *deferred = talloc_zero(repl->current, ik_wait_deferred_t);
    if (deferred == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    deferred->worker_ctx = worker_ctx;
    deferred->result = result;

    ik_wait_worker_args_t *worker_args = talloc_zero(worker_ctx, ik_wait_worker_args_t);
    if (worker_args == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    worker_args->ctx = worker_ctx;
    worker_args->db_ctx = repl->shared->worker_db_ctx;
    worker_args->session_id = repl->shared->session_id;
    worker_args->my_uuid = talloc_strdup(worker_ctx, repl->current->uuid);
    worker_args->timeout_sec = timeout_sec;
    worker_args->target_uuids = target_uuids;
    worker_args->target_count = target_count;
    worker_args->result = result;
    worker_args->mutex = &repl->current->tool_thread_mutex;
    worker_args->complete = &repl->current->tool_thread_complete;
    worker_args->interrupted = &repl->current->interrupt_requested;

    // Set up deferred execution
    repl->current->tool_thread_ctx = worker_ctx;
    repl->current->tool_deferred_data = deferred;
    repl->current->pending_on_complete = wait_on_complete;

    // Start worker thread
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    repl->current->tool_thread_complete = false;
    repl->current->tool_thread_running = true;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    int pthread_result = pthread_create_(&repl->current->tool_thread, NULL, wait_worker, worker_args);
    if (pthread_result != 0) {  // LCOV_EXCL_BR_LINE
        pthread_mutex_lock_(&repl->current->tool_thread_mutex);  // LCOV_EXCL_LINE
        repl->current->tool_thread_running = false;  // LCOV_EXCL_LINE
        pthread_mutex_unlock_(&repl->current->tool_thread_mutex);  // LCOV_EXCL_LINE
        const char *msg = "Error: Failed to spawn worker thread";  // LCOV_EXCL_LINE
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));  // LCOV_EXCL_LINE
        talloc_free(deferred);  // LCOV_EXCL_LINE
        talloc_free(worker_ctx);  // LCOV_EXCL_LINE
        repl->current->tool_thread_ctx = NULL;  // LCOV_EXCL_LINE
        repl->current->tool_deferred_data = NULL;  // LCOV_EXCL_LINE
        repl->current->pending_on_complete = NULL;  // LCOV_EXCL_LINE
        return OK(NULL);  // LCOV_EXCL_LINE
    }

    // Transition to EXECUTING_TOOL state (manually, since we're coming from IDLE not WAITING_FOR_LLM)
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    atomic_store(&repl->current->state, IK_AGENT_STATE_EXECUTING_TOOL);
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    return OK(NULL);
}
