#include "tool.h"

#include "panic.h"

#include <assert.h>
#include <talloc.h>

ik_tool_call_t *ik_tool_call_create(TALLOC_CTX *ctx,
                                    const char *id,
                                    const char *name,
                                    const char *arguments)
{
    assert(id != NULL); // LCOV_EXCL_BR_LINE
    assert(name != NULL); // LCOV_EXCL_BR_LINE
    assert(arguments != NULL); // LCOV_EXCL_BR_LINE

    ik_tool_call_t *call = talloc_zero(ctx, ik_tool_call_t);
    if (call == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Copy strings as children of the struct
    call->id = talloc_strdup(call, id);
    if (call->id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    call->name = talloc_strdup(call, name);
    if (call->name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    call->arguments = talloc_strdup(call, arguments);
    if (call->arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    return call;
}
