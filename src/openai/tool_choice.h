#ifndef IK_OPENAI_TOOL_CHOICE_H
#define IK_OPENAI_TOOL_CHOICE_H

#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

/**
 * Tool choice mode enum
 *
 * Determines how the model should use tools in a request.
 */
typedef enum {
    IK_TOOL_CHOICE_AUTO,      /* Model decides whether to use tools (default) */
    IK_TOOL_CHOICE_NONE,      /* Model must not use tools */
    IK_TOOL_CHOICE_REQUIRED,  /* Model must use at least one tool */
    IK_TOOL_CHOICE_SPECIFIC   /* Model must use a specific named tool */
} ik_tool_choice_mode_t;

/**
 * Tool choice configuration
 *
 * Represents a tool_choice value for an OpenAI API request.
 * Small enough to pass by value.
 */
typedef struct {
    ik_tool_choice_mode_t mode;  /* Tool choice mode */
    char *tool_name;              /* Tool name (for SPECIFIC mode only) */
} ik_tool_choice_t;

/**
 * Create tool_choice for "auto" mode
 *
 * Model decides whether to use tools.
 *
 * @return tool_choice value
 */
ik_tool_choice_t ik_tool_choice_auto(void);

/**
 * Create tool_choice for "none" mode
 *
 * Model must not use tools.
 *
 * @return tool_choice value
 */
ik_tool_choice_t ik_tool_choice_none(void);

/**
 * Create tool_choice for "required" mode
 *
 * Model must use at least one tool.
 *
 * @return tool_choice value
 */
ik_tool_choice_t ik_tool_choice_required(void);

/**
 * Create tool_choice for specific tool
 *
 * Model must use the named tool.
 *
 * @param parent     Talloc context for tool_name string
 * @param tool_name  Name of the tool to force (e.g., "glob")
 * @return           tool_choice value
 */
ik_tool_choice_t ik_tool_choice_specific(void *parent, const char *tool_name);

/**
 * Serialize tool_choice to JSON
 *
 * Adds a tool_choice field to the given JSON object.
 * - AUTO/NONE/REQUIRED modes serialize as string values
 * - SPECIFIC mode serializes as object: {"type": "function", "function": {"name": "<tool_name>"}}
 *
 * @param doc     yyjson mutable document
 * @param obj     yyjson mutable object to add tool_choice field to
 * @param key     JSON key name (typically "tool_choice")
 * @param choice  Tool choice configuration to serialize
 */
void ik_tool_choice_serialize(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, ik_tool_choice_t choice);

#endif /* IK_OPENAI_TOOL_CHOICE_H */
