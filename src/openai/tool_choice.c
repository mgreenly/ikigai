#include "openai/tool_choice.h"
#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <talloc.h>

/**
 * Tool choice implementation
 *
 * Provides helper functions for creating tool_choice values.
 */

ik_tool_choice_t ik_tool_choice_auto(void) {
    ik_tool_choice_t choice;
    choice.mode = IK_TOOL_CHOICE_AUTO;
    choice.tool_name = NULL;
    return choice;
}

ik_tool_choice_t ik_tool_choice_none(void) {
    ik_tool_choice_t choice;
    choice.mode = IK_TOOL_CHOICE_NONE;
    choice.tool_name = NULL;
    return choice;
}

ik_tool_choice_t ik_tool_choice_required(void) {
    ik_tool_choice_t choice;
    choice.mode = IK_TOOL_CHOICE_REQUIRED;
    choice.tool_name = NULL;
    return choice;
}

ik_tool_choice_t ik_tool_choice_specific(void *parent, const char *tool_name) {
    assert(parent != NULL); // LCOV_EXCL_BR_LINE
    assert(tool_name != NULL); // LCOV_EXCL_BR_LINE

    ik_tool_choice_t choice;
    choice.mode = IK_TOOL_CHOICE_SPECIFIC;
    choice.tool_name = talloc_strdup(parent, tool_name);
    if (!choice.tool_name) { // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    return choice;
}

void ik_tool_choice_serialize(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, ik_tool_choice_t choice) {
    assert(doc != NULL); // LCOV_EXCL_BR_LINE
    assert(obj != NULL); // LCOV_EXCL_BR_LINE
    assert(key != NULL); // LCOV_EXCL_BR_LINE

    switch (choice.mode) { // LCOV_EXCL_BR_LINE
        case IK_TOOL_CHOICE_AUTO: // LCOV_EXCL_BR_LINE
            if (!yyjson_mut_obj_add_str(doc, obj, key, "auto")) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to add tool_choice auto to JSON"); // LCOV_EXCL_LINE
            }
            break;

        case IK_TOOL_CHOICE_NONE: // LCOV_EXCL_BR_LINE
            if (!yyjson_mut_obj_add_str(doc, obj, key, "none")) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to add tool_choice none to JSON"); // LCOV_EXCL_LINE
            }
            break;

        case IK_TOOL_CHOICE_REQUIRED: // LCOV_EXCL_BR_LINE
            if (!yyjson_mut_obj_add_str(doc, obj, key, "required")) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to add tool_choice required to JSON"); // LCOV_EXCL_LINE
            }
            break;

        case IK_TOOL_CHOICE_SPECIFIC: {
            assert(choice.tool_name != NULL); // LCOV_EXCL_BR_LINE

            // Create tool_choice object: {"type": "function", "function": {"name": "<tool_name>"}}
            yyjson_mut_val *choice_obj = yyjson_mut_obj(doc);
            if (choice_obj == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

            // Add type field
            if (!yyjson_mut_obj_add_str(doc, choice_obj, "type", "function")) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to add type field to tool_choice object"); // LCOV_EXCL_LINE
            }

            // Create function object
            yyjson_mut_val *function_obj = yyjson_mut_obj(doc);
            if (function_obj == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

            // Add name field to function object
            if (!yyjson_mut_obj_add_str(doc, function_obj, "name", choice.tool_name)) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to add name field to function object"); // LCOV_EXCL_LINE
            }

            // Add function object to tool_choice object
            if (!yyjson_mut_obj_add_val(doc, choice_obj, "function", function_obj)) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to add function object to tool_choice"); // LCOV_EXCL_LINE
            }

            // Add tool_choice object to parent object
            if (!yyjson_mut_obj_add_val(doc, obj, key, choice_obj)) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to add tool_choice object to JSON"); // LCOV_EXCL_LINE
            }
            break;
        }

        default: // LCOV_EXCL_LINE
            PANIC("Invalid tool_choice mode"); // LCOV_EXCL_LINE
    }
}
