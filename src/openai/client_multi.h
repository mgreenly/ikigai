#ifndef IK_OPENAI_CLIENT_MULTI_H
#define IK_OPENAI_CLIENT_MULTI_H

#include "error.h"
#include "config.h"
#include "openai/client.h"
#include <sys/select.h>

/**
 * Multi-handle manager for non-blocking OpenAI HTTP requests
 *
 * Provides non-blocking HTTP client interface using curl_multi.
 * Integrates with select()-based event loops.
 *
 * Memory: talloc-based ownership
 * Errors: Result types with OK()/ERR() patterns
 */

/**
 * Multi-handle manager structure (opaque)
 *
 * Manages non-blocking HTTP requests using curl_multi interface.
 * Integrates with select()-based event loops.
 */
typedef struct ik_openai_multi ik_openai_multi_t;

/**
 * Create a multi-handle manager
 *
 * @param parent  Talloc context parent (or NULL)
 * @return        OK(multi) or ERR(...)
 */
res_t ik_openai_multi_create(void *parent);

/**
 * Add a request to the multi-handle (non-blocking)
 *
 * Initiates an HTTP request without blocking. The request will make progress
 * when ik_openai_multi_perform() is called.
 *
 * @param multi      Multi-handle manager
 * @param cfg        Configuration with API key, model, etc.
 * @param conv       Conversation to send
 * @param stream_cb  Callback for streaming chunks (or NULL)
 * @param cb_ctx     Context pointer passed to callback
 * @return           OK(NULL) or ERR(...)
 */
res_t ik_openai_multi_add_request(ik_openai_multi_t *multi,
                                   const ik_cfg_t *cfg,
                                   ik_openai_conversation_t *conv,
                                   ik_openai_stream_cb_t stream_cb,
                                   void *cb_ctx);

/**
 * Perform non-blocking I/O operations
 *
 * Call this when select() indicates curl FDs are ready, or periodically.
 *
 * @param multi          Multi-handle manager
 * @param still_running  Output: number of requests still in progress
 * @return               OK(NULL) or ERR(...)
 */
res_t ik_openai_multi_perform(ik_openai_multi_t *multi, int *still_running);

/**
 * Get file descriptors for select()
 *
 * Populates fd_sets with curl's file descriptors.
 *
 * @param multi      Multi-handle manager
 * @param read_fds   Read FD set (will be modified)
 * @param write_fds  Write FD set (will be modified)
 * @param exc_fds    Exception FD set (will be modified)
 * @param max_fd     Output: highest FD number + 1
 * @return           OK(NULL) or ERR(...)
 */
res_t ik_openai_multi_fdset(ik_openai_multi_t *multi,
                             fd_set *read_fds, fd_set *write_fds,
                             fd_set *exc_fds, int *max_fd);

/**
 * Get timeout for select()
 *
 * Returns the timeout value curl recommends for select().
 *
 * @param multi      Multi-handle manager
 * @param timeout_ms Output: timeout in milliseconds (-1 = no timeout)
 * @return           OK(NULL) or ERR(...)
 */
res_t ik_openai_multi_timeout(ik_openai_multi_t *multi, long *timeout_ms);

/**
 * Check for completed requests
 *
 * Call this after ik_openai_multi_perform() to handle completed transfers.
 * Processes all completed requests and invokes completion callbacks.
 *
 * @param multi  Multi-handle manager
 * @return       OK(NULL) or ERR(...)
 */
res_t ik_openai_multi_info_read(ik_openai_multi_t *multi);

#endif /* IK_OPENAI_CLIENT_MULTI_H */
