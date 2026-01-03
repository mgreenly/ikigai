#ifndef IK_PROVIDER_H
#define IK_PROVIDER_H

#include "error.h"
#include "providers/provider_types.h"
#include <stdbool.h>
#include <stdint.h>
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
    IK_CONTENT_TEXT = 0,              /* Text content */
    IK_CONTENT_TOOL_CALL = 1,         /* Tool call request */
    IK_CONTENT_TOOL_RESULT = 2,       /* Tool execution result */
    IK_CONTENT_THINKING = 3,          /* Thinking/reasoning content */
    IK_CONTENT_REDACTED_THINKING = 4  /* Redacted thinking (encrypted) */
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
 *
 * NOTE: Temporarily commented out to avoid conflict with openai/tool_choice.h
 * during coexistence phase. Will be uncommented after old OpenAI code is removed.
 */
/* TEMPORARILY DISABLED - TYPE CONFLICT
typedef enum {
    IK_TOOL_AUTO = 0,     / * Model decides when to use tools * /
    IK_TOOL_NONE = 1,     / * No tool use allowed * /
    IK_TOOL_REQUIRED = 2, / * Must use a tool * /
    IK_TOOL_SPECIFIC = 3  / * Must use specific tool * /
} ik_tool_choice_t;
**/

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

/* Marker for stream/vtable headers that depend on enums above */
#define IK_FINISH_REASON_DEFINED 1

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
            char *text;      /* Thinking summary text */
            char *signature; /* Cryptographic signature (required for round-trip) */
        } thinking;

        /* IK_CONTENT_REDACTED_THINKING */
        struct {
            char *data; /* Encrypted opaque data (base64) */
        } redacted_thinking;
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
    int tool_choice_mode;              /* Tool choice mode (temporarily int during coexistence) */
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

/* Include stream events and vtable after core types are defined */
#include "providers/provider_stream.h"
#include "providers/provider_vtable.h"

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

/**
 * Model capability information
 *
 * Maps model prefixes to their capabilities for validation and user feedback.
 */
typedef struct {
    const char *prefix;          /* Model name prefix (e.g., "claude-sonnet-4-5") */
    const char *provider;        /* Provider name ("anthropic", "openai", "google") */
    bool supports_thinking;      /* true if model supports thinking/reasoning */
    int32_t max_thinking_tokens; /* Maximum thinking tokens (0 if effort-based or unsupported) */
} ik_model_capability_t;

/**
 * Check if a model supports thinking/reasoning
 *
 * @param model    Model identifier
 * @param supports Output: true if model supports thinking
 * @return         OK on success, ERR on failure
 */
res_t ik_model_supports_thinking(const char *model, bool *supports);

/**
 * Get maximum thinking token budget for a model
 *
 * @param model  Model identifier
 * @param budget Output: max thinking tokens (0 if effort-based or unsupported)
 * @return       OK on success, ERR on failure
 */
res_t ik_model_get_thinking_budget(const char *model, int32_t *budget);

#endif /* IK_PROVIDER_H */
