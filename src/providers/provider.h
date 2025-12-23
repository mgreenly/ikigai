#ifndef IK_PROVIDER_H
#define IK_PROVIDER_H

#include "error.h"
#include "providers/provider_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/select.h>
#include <talloc.h>

/**
 * Provider Core Types
 *
 * This header defines the unified provider abstraction interface.
 * All providers (Anthropic, OpenAI, Google) implement this interface
 * through async/non-blocking vtables that integrate with the
 * select()-based event loop.
 *
 * Key design:
 * - Async everything: All HTTP operations use curl_multi (non-blocking)
 * - Event loop integration: fdset/perform/timeout/info_read pattern
 * - Callbacks for responses: No blocking waits
 */

// Forward declaration for logger
typedef struct ik_logger ik_logger_t;

/* ================================================================
 * Enum Definitions
 * ================================================================ */

/**
 * Provider-agnostic thinking budget levels
 *
 * Maps to provider-specific parameters:
 * - Anthropic: budget_tokens (1024/22016/43008)
 * - OpenAI: reasoning_effort ("low"/"medium"/"high")
 * - Google: thinking_budget (128/11008/21888)
 */
typedef enum {
    IK_THINKING_NONE = 0,  /* No thinking/reasoning */
    IK_THINKING_LOW = 1,   /* Low thinking budget */
    IK_THINKING_MED = 2,   /* Medium thinking budget */
    IK_THINKING_HIGH = 3   /* High thinking budget */
} ik_thinking_level_t;

/**
 * Normalized completion reasons across providers
 */
typedef enum {
    IK_FINISH_STOP = 0,           /* Normal completion */
    IK_FINISH_LENGTH = 1,         /* Max tokens reached */
    IK_FINISH_TOOL_USE = 2,       /* Stopped to use a tool */
    IK_FINISH_CONTENT_FILTER = 3, /* Content policy violation */
    IK_FINISH_ERROR = 4,          /* Error during generation */
    IK_FINISH_UNKNOWN = 5         /* Unknown/unmapped reason */
} ik_finish_reason_t;

/**
 * Content block types
 */
typedef enum {
    IK_CONTENT_TEXT = 0,        /* Text content */
    IK_CONTENT_TOOL_CALL = 1,   /* Tool call request */
    IK_CONTENT_TOOL_RESULT = 2, /* Tool execution result */
    IK_CONTENT_THINKING = 3     /* Thinking/reasoning content */
} ik_content_type_t;

/**
 * Message roles
 */
typedef enum {
    IK_ROLE_USER = 0,      /* User message */
    IK_ROLE_ASSISTANT = 1, /* Assistant message */
    IK_ROLE_TOOL = 2       /* Tool result message */
} ik_role_t;

/**
 * Tool invocation control modes
 */
typedef enum {
    IK_TOOL_AUTO = 0,     /* Model decides when to use tools */
    IK_TOOL_NONE = 1,     /* No tool use allowed */
    IK_TOOL_REQUIRED = 2, /* Must use a tool */
    IK_TOOL_SPECIFIC = 3  /* Must use specific tool */
} ik_tool_choice_t;

/**
 * Provider error categories for retry logic
 */
typedef enum {
    IK_ERR_CAT_AUTH = 0,           /* Invalid credentials (401, 403) */
    IK_ERR_CAT_RATE_LIMIT = 1,     /* Rate limit exceeded (429) */
    IK_ERR_CAT_INVALID_ARG = 2,    /* Bad request (400) */
    IK_ERR_CAT_NOT_FOUND = 3,      /* Model not found (404) */
    IK_ERR_CAT_SERVER = 4,         /* Server error (500, 502, 503) */
    IK_ERR_CAT_TIMEOUT = 5,        /* Request timeout */
    IK_ERR_CAT_CONTENT_FILTER = 6, /* Content policy violation */
    IK_ERR_CAT_NETWORK = 7,        /* Network/connection error */
    IK_ERR_CAT_UNKNOWN = 8         /* Other/unmapped errors */
} ik_error_category_t;

/**
 * Stream event types
 */
typedef enum {
    IK_STREAM_START = 0,           /* Stream started */
    IK_STREAM_TEXT_DELTA = 1,      /* Text content chunk */
    IK_STREAM_THINKING_DELTA = 2,  /* Thinking/reasoning chunk */
    IK_STREAM_TOOL_CALL_START = 3, /* Tool call started */
    IK_STREAM_TOOL_CALL_DELTA = 4, /* Tool call argument chunk */
    IK_STREAM_TOOL_CALL_DONE = 5,  /* Tool call complete */
    IK_STREAM_DONE = 6,            /* Stream complete */
    IK_STREAM_ERROR = 7            /* Error occurred */
} ik_stream_event_type_t;

/* ================================================================
 * Structure Definitions
 * ================================================================ */

/**
 * Token usage counters
 */
struct ik_usage {
    int32_t input_tokens;    /* Prompt/input tokens */
    int32_t output_tokens;   /* Completion/output tokens */
    int32_t thinking_tokens; /* Thinking/reasoning tokens */
    int32_t cached_tokens;   /* Cache hit tokens */
    int32_t total_tokens;    /* Total tokens used */
};

/**
 * Thinking configuration
 */
struct ik_thinking_config {
    ik_thinking_level_t level; /* Thinking budget level */
    bool include_summary;      /* Include thinking summary in response */
};

/**
 * Content block with variant data
 */
struct ik_content_block {
    ik_content_type_t type; /* Content type discriminator */
    union {
        /* IK_CONTENT_TEXT */
        struct {
            char *text; /* Text content */
        } text;

        /* IK_CONTENT_TOOL_CALL */
        struct {
            char *id;        /* Tool call ID */
            char *name;      /* Function name */
            char *arguments; /* JSON arguments */
        } tool_call;

        /* IK_CONTENT_TOOL_RESULT */
        struct {
            char *tool_call_id; /* ID of the tool call this result is for */
            char *content;      /* Result content */
            bool is_error;      /* true if tool execution failed */
        } tool_result;

        /* IK_CONTENT_THINKING */
        struct {
            char *text; /* Thinking summary text */
        } thinking;
    } data;
};

/**
 * Single message in conversation
 */
struct ik_message {
    ik_role_t role;                     /* Message role */
    ik_content_block_t *content_blocks; /* Array of content blocks */
    size_t content_count;               /* Number of content blocks */
    char *provider_metadata;            /* Provider-specific metadata (JSON) */
};

/**
 * Tool definition
 */
struct ik_tool_def {
    char *name;              /* Tool name */
    char *description;       /* Tool description */
    char *parameters;        /* JSON schema for parameters */
    bool strict;             /* Strict schema validation */
};

/**
 * Request to provider
 */
struct ik_request {
    char *system_prompt;               /* System prompt */
    ik_message_t *messages;            /* Array of messages */
    size_t message_count;              /* Number of messages */
    char *model;                       /* Model identifier */
    ik_thinking_config_t thinking;     /* Thinking configuration */
    ik_tool_def_t *tools;              /* Array of tool definitions */
    size_t tool_count;                 /* Number of tools */
    int32_t max_output_tokens;         /* Maximum response tokens */
    ik_tool_choice_t tool_choice_mode; /* Tool choice mode */
    char *tool_choice_name;            /* Specific tool name (if mode is IK_TOOL_SPECIFIC) */
};

/**
 * Response from provider
 */
struct ik_response {
    ik_content_block_t *content_blocks; /* Array of content blocks */
    size_t content_count;               /* Number of content blocks */
    ik_finish_reason_t finish_reason;   /* Completion reason */
    ik_usage_t usage;                   /* Token usage */
    char *model;                        /* Actual model used */
    char *provider_data;                /* Provider-specific data (JSON) */
};

/**
 * Provider error information
 */
struct ik_provider_error {
    ik_error_category_t category; /* Error category */
    int32_t http_status;          /* HTTP status code (0 if not HTTP) */
    char *message;                /* Human-readable message */
    char *provider_code;          /* Provider's error type/code */
    int32_t retry_after_ms;       /* Retry delay (-1 if not applicable) */
};

/**
 * Stream event with variant payload
 */
struct ik_stream_event {
    ik_stream_event_type_t type; /* Event type */
    int32_t index;               /* Content block index */
    union {
        /* IK_STREAM_START */
        struct {
            const char *model; /* Model name */
        } start;

        /* IK_STREAM_TEXT_DELTA, IK_STREAM_THINKING_DELTA */
        struct {
            const char *text; /* Text fragment */
        } delta;

        /* IK_STREAM_TOOL_CALL_START */
        struct {
            const char *id;   /* Tool call ID */
            const char *name; /* Function name */
        } tool_start;

        /* IK_STREAM_TOOL_CALL_DELTA */
        struct {
            const char *arguments; /* JSON fragment */
        } tool_delta;

        /* IK_STREAM_DONE */
        struct {
            ik_finish_reason_t finish_reason; /* Completion reason */
            ik_usage_t usage;                 /* Token usage */
            const char *provider_data;        /* Provider metadata */
        } done;

        /* IK_STREAM_ERROR */
        struct {
            ik_error_category_t category; /* Error category */
            const char *message;          /* Error message */
        } error;
    } data;
};

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
    res_t (*start_request)(void *ctx, const ik_request_t *req,
                           ik_provider_completion_cb_t completion_cb, void *completion_ctx);

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
    res_t (*start_stream)(void *ctx, const ik_request_t *req,
                          ik_stream_cb_t stream_cb, void *stream_ctx,
                          ik_provider_completion_cb_t completion_cb, void *completion_ctx);

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

/* ================================================================
 * Functions
 * ================================================================ */

/**
 * Infer provider name from model prefix
 *
 * @param model_name Model identifier (e.g., "gpt-5-mini", "claude-sonnet-4-5")
 * @return           Provider name ("openai", "anthropic", "google"), or NULL if unknown
 *
 * Model prefix to provider mapping:
 * - "gpt-*", "o1-*", "o3-*" -> "openai"
 * - "claude-*" -> "anthropic"
 * - "gemini-*" -> "google"
 * - Unknown -> NULL
 */
const char *ik_infer_provider(const char *model_name);

#endif /* IK_PROVIDER_H */
