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
