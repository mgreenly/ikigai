#ifndef IK_OUTPUT_STYLE_H
#define IK_OUTPUT_STYLE_H

#include <stddef.h>
#include <stdint.h>

/**
 * Output category enum for consistent styling across all output channels.
 */
typedef enum {
    IK_OUTPUT_USER_INPUT,
    IK_OUTPUT_MODEL_TEXT,
    IK_OUTPUT_TOOL_REQUEST,
    IK_OUTPUT_TOOL_RESPONSE,
    IK_OUTPUT_WARNING,
    IK_OUTPUT_SLASH_CMD,
    IK_OUTPUT_SLASH_OUTPUT,
    IK_OUTPUT_SYSTEM_PROMPT,
    IK_OUTPUT_COUNT
} ik_output_category_t;

/**
 * Get prefix string for an output category.
 *
 * @param category  Output category
 * @return          Prefix string (UTF-8), or NULL if category has no prefix
 */
static inline const char *ik_output_prefix(ik_output_category_t category)
{
    switch (category) {
    case IK_OUTPUT_USER_INPUT:
        return "❯";
    case IK_OUTPUT_MODEL_TEXT:
        return "●";
    case IK_OUTPUT_TOOL_REQUEST:
        return "→";
    case IK_OUTPUT_TOOL_RESPONSE:
        return "←";
    case IK_OUTPUT_WARNING:
        return "⚠";
    case IK_OUTPUT_SLASH_CMD:
        return NULL;
    case IK_OUTPUT_SLASH_OUTPUT:
        return NULL;
    case IK_OUTPUT_SYSTEM_PROMPT:
        return NULL;
    case IK_OUTPUT_COUNT:
        return NULL;
    }
    return NULL;
}

/**
 * Get color code for an output category.
 *
 * @param category  Output category
 * @return          256-color palette index, or -1 for default (white)
 */
static inline int32_t ik_output_color(ik_output_category_t category)
{
    switch (category) {
    case IK_OUTPUT_USER_INPUT:
        return -1;  // white/default
    case IK_OUTPUT_MODEL_TEXT:
        return -1;  // white/default
    case IK_OUTPUT_TOOL_REQUEST:
        return 242;  // gray
    case IK_OUTPUT_TOOL_RESPONSE:
        return 242;  // gray
    case IK_OUTPUT_WARNING:
        return 179;  // subdued yellow (golden/olive)
    case IK_OUTPUT_SLASH_CMD:
        return 242;  // gray
    case IK_OUTPUT_SLASH_OUTPUT:
        return 242;  // gray
    case IK_OUTPUT_SYSTEM_PROMPT:
        return 153;  // soft blue
    case IK_OUTPUT_COUNT:
        return -1;
    }
    return -1;
}

#endif // IK_OUTPUT_STYLE_H
