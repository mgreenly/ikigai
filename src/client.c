#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <talloc.h>
#include "terminal.h"
#include "logger.h"

int main(void)
{
    // Create root talloc context
    void *root_ctx = talloc_new(NULL);
    if (!root_ctx) {
        fprintf(stderr, "Failed to create talloc context\n");
        return EXIT_FAILURE;
    }

    // Initialize terminal
    ik_term_ctx_t *term_ctx = NULL;
    res_t result = ik_term_init(root_ctx, &term_ctx);
    if (is_err(&result)) {
        error_fprintf(stderr, result.err);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }

    // Get terminal dimensions
    int rows, cols;
    result = ik_term_get_size(term_ctx, &rows, &cols);
    if (is_err(&result)) {
        error_fprintf(stderr, result.err);
        ik_term_cleanup(term_ctx);
        talloc_free(root_ctx);
        return EXIT_FAILURE;
    }

    // Display terminal information
    char msg[256];
    int len = snprintf(msg, sizeof(msg),
                      "Terminal dimensions: %d rows x %d columns\r\n"
                      "Press Ctrl+C to exit\r\n",
                      rows, cols);
    if (write(term_ctx->tty_fd, msg, (size_t)len) < 0) {
        // Ignore write errors in demo
    }

    // Main loop: read bytes until Ctrl+C
    char byte;
    while (1) {
        ssize_t n = read(term_ctx->tty_fd, &byte, 1);
        if (n <= 0) {
            break;
        }

        // Check for Ctrl+C (0x03)
        if (byte == 0x03) {
            break;
        }

        // Echo the byte for demonstration (optional)
        if (write(term_ctx->tty_fd, &byte, 1) < 0) {
            // Ignore write errors in demo
        }
    }

    // Cleanup and exit
    ik_term_cleanup(term_ctx);
    talloc_free(root_ctx);
    return EXIT_SUCCESS;
}
