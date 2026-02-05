#include "apps/ikigai/repl.h"

#include "apps/ikigai/shared.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>


#include "shared/poison.h"
#ifdef IKIGAI_DEV
void ik_repl_dev_dump_framebuffer(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */

    // Skip if no framebuffer saved
    if (repl->dev_framebuffer == NULL || repl->dev_framebuffer_len == 0) {
        return;
    }

    // Check if debug directory exists (runtime opt-in)
    struct stat st;
    if (stat(".ikigai/debug", &st) != 0 || !S_ISDIR(st.st_mode)) {
        return;
    }

    // Open file for writing
    int fd = open(".ikigai/debug/repl_viewport.framebuffer",
                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return;  // Silently fail - debug feature shouldn't break normal operation
    }

    // Write header line
    char header[128];
    int header_len = snprintf(header, sizeof(header),
                              "# rows=%d cols=%d cursor=%d,%d len=%zu\n",
                              repl->shared->term->screen_rows,
                              repl->shared->term->screen_cols,
                              repl->dev_cursor_row,
                              repl->dev_cursor_col,
                              repl->dev_framebuffer_len);

    ssize_t written = write(fd, header, (size_t)header_len);
    (void)written;  // Ignore write errors for debug feature

    // Write raw framebuffer bytes
    written = write(fd, repl->dev_framebuffer, repl->dev_framebuffer_len);
    (void)written;

    close(fd);
}
#endif
