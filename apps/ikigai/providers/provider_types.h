#ifndef IK_PROVIDER_TYPES_H
#define IK_PROVIDER_TYPES_H

/**
 * Forward declarations for provider types
 *
 * Include this file when you only need forward declarations
 * without the full type definitions.
 */

/**
 * Semantic category of a system prompt block
 */
typedef enum {
    IK_SYSTEM_BLOCK_BASE_PROMPT,
    IK_SYSTEM_BLOCK_PINNED_DOC,
    IK_SYSTEM_BLOCK_SKILL,
    IK_SYSTEM_BLOCK_SKILL_CATALOG,
    IK_SYSTEM_BLOCK_AGENTS_MD,
    IK_SYSTEM_BLOCK_SESSION_SUMMARY,
    IK_SYSTEM_BLOCK_RECENT_SUMMARY,
} ik_system_block_type_t;

// Forward declarations
typedef struct ik_provider ik_provider_t;
typedef struct ik_provider_vtable ik_provider_vtable_t;
typedef struct ik_request ik_request_t;
typedef struct ik_response ik_response_t;
typedef struct ik_message ik_message_t;
typedef struct ik_content_block ik_content_block_t;
typedef struct ik_tool_def ik_tool_def_t;
typedef struct ik_stream_event ik_stream_event_t;
typedef struct ik_provider_completion ik_provider_completion_t;
typedef struct ik_usage ik_usage_t;
typedef struct ik_thinking_config ik_thinking_config_t;
typedef struct ik_provider_error ik_provider_error_t;
typedef struct ik_system_block ik_system_block_t;

#endif /* IK_PROVIDER_TYPES_H */
