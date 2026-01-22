/**
 * @file provider_vtable.h
 * @brief Provider vtable and callback interfaces
 */

#ifndef IK_PROVIDER_VTABLE_H
#define IK_PROVIDER_VTABLE_H

#include "error.h"
#include "provider_stream.h"
#include <sys/select.h>
#include <stdbool.h>
#include <stdint.h>

/* Forward declarations to avoid circular dependencies */
typedef struct ik_logger ik_logger_t;
typedef struct ik_request ik_request_t;
typedef struct ik_response ik_response_t;
typedef struct ik_stream_event ik_stream_event_t;
typedef struct ik_provider_vtable ik_provider_vtable_t;

/* ================================================================
 * Callback Type Definitions
 * ================================================================ */

/**
 * Stream callback - receives streaming events as data arrives
 *
 * Called during perform() as data arrives from the network.
 *
 * @param event Stream event (text delta, tool call, etc.)
 * @param ctx   User-provided context
 * @return      OK(NULL) to continue, ERR(...) to abort stream
 */
typedef res_t (*ik_stream_cb_t)(const ik_stream_event_t *event, void *ctx);

/**
 * HTTP completion callback payload
 */
struct ik_provider_completion {
    bool success;                    /* true if request succeeded */
    int32_t http_status;             /* HTTP status code */
    ik_response_t *response;         /* Parsed response (NULL on error) */
    ik_error_category_t error_category; /* Error category if failed */
    char *error_message;             /* Human-readable error message if failed */
    int32_t retry_after_ms;          /* Suggested retry delay (-1 if not applicable) */
};

/**
 * Completion callback - invoked when request finishes
 *
 * Called from info_read() when transfer completes.
 *
 * @param completion Completion info (success/error, usage, etc.)
 * @param ctx        User-provided context
 * @return           OK(NULL) on success, ERR(...) on failure
 */
typedef res_t (*ik_provider_completion_cb_t)(const ik_provider_completion_t *completion, void *ctx);

/* ================================================================
 * Provider Vtable Interface (Async/Non-blocking)
 * ================================================================ */

/**
 * Provider vtable for async/non-blocking HTTP operations
 *
 * All providers MUST implement these methods to integrate with the
 * select()-based event loop. Blocking implementations are NOT acceptable.
 */
struct ik_provider_vtable {
    /* ============================================================
     * Event Loop Integration (REQUIRED)
     * These methods integrate the provider with select()
     * ============================================================ */

    /**
     * fdset - Populate fd_sets for select()
     *
     * @param ctx       Provider context (opaque)
     * @param read_fds  Read fd_set to populate (will be modified)
     * @param write_fds Write fd_set to populate (will be modified)
     * @param exc_fds   Exception fd_set to populate (will be modified)
     * @param max_fd    Output: highest FD number (will be updated)
     * @return          OK(NULL) on success, ERR(...) on failure
     *
     * Called before select() to get file descriptors the provider
     * needs to monitor. Provider adds its curl_multi FDs to the sets.
     */
    res_t (*fdset)(void *ctx, fd_set *read_fds, fd_set *write_fds, fd_set *exc_fds, int *max_fd);

    /**
     * perform - Process pending I/O operations
     *
     * @param ctx             Provider context (opaque)
     * @param running_handles Output: number of active transfers
     * @return                OK(NULL) on success, ERR(...) on failure
     *
     * Called after select() returns to process ready file descriptors.
     * This drives curl_multi_perform() internally. Non-blocking.
     */
    res_t (*perform)(void *ctx, int *running_handles);

    /**
     * timeout - Get recommended timeout for select()
     *
     * @param ctx        Provider context (opaque)
     * @param timeout_ms Output: recommended timeout in milliseconds
     * @return           OK(NULL) on success, ERR(...) on failure
     *
     * Returns curl's recommended timeout. Caller should use minimum
     * of this and any other timeout requirements.
     */
    res_t (*timeout)(void *ctx, long *timeout_ms);

    /**
     * info_read - Process completed transfers
     *
     * @param ctx    Provider context (opaque)
     * @param logger Logger for debug output
     *
     * Called after perform() to check for completed transfers.
     * Invokes completion callbacks for finished requests.
     */
    void (*info_read)(void *ctx, ik_logger_t *logger);

    /* ============================================================
     * Request Initiation (Non-blocking)
     * These methods start requests but return immediately
     * ============================================================ */

    /**
     * start_request - Initiate non-streaming request
     *
     * @param ctx           Provider context
     * @param req           Request to send
     * @param completion_cb Callback invoked when request completes
     * @param completion_ctx User context for callback
     * @return              OK(NULL) on success, ERR(...) on failure
     *
     * Returns immediately. Request progresses through perform().
     * completion_cb is invoked from info_read() when transfer completes.
     */
    res_t (*start_request)(void *ctx,
                           const ik_request_t *req,
                           ik_provider_completion_cb_t completion_cb,
                           void *completion_ctx);

    /**
     * start_stream - Initiate streaming request
     *
     * @param ctx           Provider context
     * @param req           Request to send
     * @param stream_cb     Callback for streaming events
     * @param stream_ctx    User context for stream callback
     * @param completion_cb Callback invoked when stream completes
     * @param completion_ctx User context for completion callback
     * @return              OK(NULL) on success, ERR(...) on failure
     *
     * Returns immediately. Stream events delivered via stream_cb
     * as data arrives during perform(). completion_cb invoked when done.
     */
    res_t (*start_stream)(void *ctx,
                          const ik_request_t *req,
                          ik_stream_cb_t stream_cb,
                          void *stream_ctx,
                          ik_provider_completion_cb_t completion_cb,
                          void *completion_ctx);

    /* ============================================================
     * Cleanup & Cancellation
     * ============================================================ */

    /**
     * cleanup - Release provider resources
     *
     * @param ctx Provider context
     *
     * Optional if talloc hierarchy handles all cleanup.
     * Called before provider is freed. May be NULL.
     */
    void (*cleanup)(void *ctx);

    /**
     * cancel - Cancel all in-flight requests
     *
     * @param ctx Provider context
     *
     * Called when user presses Ctrl+C or agent is being terminated.
     * After cancel(), perform() should complete quickly with no more callbacks.
     * MUST be async-signal-safe (no malloc, no mutex).
     */
    void (*cancel)(void *ctx);
};

/**
 * Provider instance - holds vtable and implementation context
 */
struct ik_provider {
    const char *name;                /* Provider name ("anthropic", "openai", "google") */
    const ik_provider_vtable_t *vt;  /* Vtable with async methods */
    void *ctx;                       /* Provider-specific context (opaque) */
};

#endif /* IK_PROVIDER_VTABLE_H */
