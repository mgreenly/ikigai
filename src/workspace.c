/**
 * @file workspace.c
 * @brief Workspace text buffer implementation
 */

#include "workspace.h"
#include "error.h"
#include <assert.h>
#include <talloc.h>

res_t ik_workspace_create(void *parent, ik_workspace_t **workspace_out)
{
    assert(workspace_out != NULL); /* LCOV_EXCL_BR_LINE */

    ik_workspace_t *workspace = talloc_zero(parent, ik_workspace_t);
    if (workspace == NULL) {
        return ERR(parent, OOM, "Failed to allocate workspace");
    }

    res_t res = ik_byte_array_create(workspace, 64);
    if (is_err(&res)) {
        talloc_free(workspace);
        return res;
    }

    workspace->text = res.ok;
    workspace->cursor_byte_offset = 0;
    *workspace_out = workspace;
    return OK(workspace);
}

res_t ik_workspace_insert_codepoint(ik_workspace_t *workspace, uint32_t codepoint)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    (void)codepoint;
    return OK(NULL);
}

res_t ik_workspace_insert_newline(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    return OK(NULL);
}

res_t ik_workspace_backspace(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    return OK(NULL);
}

res_t ik_workspace_delete(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    return OK(NULL);
}

res_t ik_workspace_get_text(ik_workspace_t *workspace, char **text_out, size_t *len_out)
{
    assert(workspace != NULL);     /* LCOV_EXCL_BR_LINE */
    assert(text_out != NULL); /* LCOV_EXCL_BR_LINE */
    assert(len_out != NULL);  /* LCOV_EXCL_BR_LINE */

    *text_out = (char *)workspace->text->data;
    *len_out = ik_byte_array_size(workspace->text);
    return OK(NULL);
}

void ik_workspace_clear(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    ik_byte_array_clear(workspace->text);
    workspace->cursor_byte_offset = 0;
}
