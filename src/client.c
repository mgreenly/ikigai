#include "config.h"
#include "error.h"
#include "logger.h"
#include "panic.h"
#include "repl.h"
#include "shared.h"
#include "terminal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <time.h>
#include <unistd.h>

/* LCOV_EXCL_START */
int main(void)
{
    // Initialize random number generator for UUID generation
    // Mix time and PID to reduce collision probability across concurrent processes
    srand((unsigned int)(time(NULL) ^ (unsigned int)getpid()));

    // Capture working directory for logger initialization (minimal bootstrap)
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        PANIC("Failed to get current working directory");
    }

    // Logger first (its own talloc root for independent lifetime)
    void *logger_ctx = talloc_new(NULL);
    if (logger_ctx == NULL) PANIC("Failed to create logger context");

    ik_logger_t *logger = ik_logger_create(logger_ctx, cwd);
    g_panic_logger = logger;  // Enable panic logging

    // Log session start
    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "session_start");
    yyjson_mut_obj_add_str(doc, root, "cwd", cwd);
    ik_logger_info_json(logger, doc);

    // Now create app root_ctx and continue with normal init
    void *root_ctx = talloc_new(NULL);
    if (root_ctx == NULL) PANIC("Failed to create root talloc context");

    // Load configuration
    ik_config_t *cfg = NULL;
    res_t cfg_result = ik_config_load(root_ctx, "~/.config/ikigai/config.json", &cfg);
    if (is_err(&cfg_result)) {
        doc = ik_log_create();
        root = yyjson_mut_doc_get_root(doc);
        yyjson_mut_obj_add_str(doc, root, "event", "config_load_error");
        yyjson_mut_obj_add_str(doc, root, "message", error_message(cfg_result.err));
        yyjson_mut_obj_add_int(doc, root, "code", cfg_result.err->code);
        yyjson_mut_obj_add_str(doc, root, "file", cfg_result.err->file);
        yyjson_mut_obj_add_int(doc, root, "line", cfg_result.err->line);
        ik_logger_error_json(logger, doc);

        // Log session end before cleanup
        doc = ik_log_create();
        root = yyjson_mut_doc_get_root(doc);
        yyjson_mut_obj_add_str(doc, root, "event", "session_end");
        yyjson_mut_obj_add_int(doc, root, "exit_code", EXIT_FAILURE);
        ik_logger_info_json(logger, doc);

        g_panic_logger = NULL;   // Disable panic logging
        talloc_free(root_ctx);
        talloc_free(logger_ctx); // Logger last
        return EXIT_FAILURE;
    }

    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    res_t result = ik_shared_ctx_init(root_ctx, cfg, cwd, ".ikigai", logger, &shared);
    if (is_err(&result)) {
        doc = ik_log_create();
        root = yyjson_mut_doc_get_root(doc);
        yyjson_mut_obj_add_str(doc, root, "event", "shared_ctx_init_error");
        yyjson_mut_obj_add_str(doc, root, "message", error_message(result.err));
        yyjson_mut_obj_add_int(doc, root, "code", result.err->code);
        yyjson_mut_obj_add_str(doc, root, "file", result.err->file);
        yyjson_mut_obj_add_int(doc, root, "line", result.err->line);
        ik_logger_error_json(logger, doc);

        // Log session end before cleanup
        doc = ik_log_create();
        root = yyjson_mut_doc_get_root(doc);
        yyjson_mut_obj_add_str(doc, root, "event", "session_end");
        yyjson_mut_obj_add_int(doc, root, "exit_code", EXIT_FAILURE);
        ik_logger_info_json(logger, doc);

        g_panic_logger = NULL;   // Disable panic logging
        talloc_free(root_ctx);
        talloc_free(logger_ctx); // Logger last
        return EXIT_FAILURE;
    }

    // Create REPL context with shared context
    ik_repl_ctx_t *repl = NULL;
    result = ik_repl_init(root_ctx, shared, &repl);
    if (is_err(&result)) {
        // Cleanup terminal first (exit alternate buffer)
        ik_term_cleanup(shared->term);
        shared->term = NULL;  // Prevent double cleanup

        doc = ik_log_create();
        root = yyjson_mut_doc_get_root(doc);
        yyjson_mut_obj_add_str(doc, root, "event", "repl_init_error");
        yyjson_mut_obj_add_str(doc, root, "message", error_message(result.err));
        yyjson_mut_obj_add_int(doc, root, "code", result.err->code);
        yyjson_mut_obj_add_str(doc, root, "file", result.err->file);
        yyjson_mut_obj_add_int(doc, root, "line", result.err->line);
        ik_logger_error_json(logger, doc);

        // Log session end before cleanup
        doc = ik_log_create();
        root = yyjson_mut_doc_get_root(doc);
        yyjson_mut_obj_add_str(doc, root, "event", "session_end");
        yyjson_mut_obj_add_int(doc, root, "exit_code", EXIT_FAILURE);
        ik_logger_info_json(logger, doc);

        g_panic_logger = NULL;   // Disable panic logging
        talloc_free(root_ctx);
        talloc_free(logger_ctx); // Logger last
        return EXIT_FAILURE;
    }

    // Our abort implementation uses `g_term_ctx_for_panic` to restore the primary buffer if it's not NULL.
    g_term_ctx_for_panic = shared->term;
    // The talloc library will call this, instead of `abort` if it's defined, which will restore the primary buffer.
    talloc_set_abort_fn(ik_talloc_abort_handler);

    result = ik_repl_run(repl);

    ik_repl_cleanup(repl);

    if (is_err(&result)) {
        doc = ik_log_create();
        root = yyjson_mut_doc_get_root(doc);
        yyjson_mut_obj_add_str(doc, root, "event", "repl_run_error");
        yyjson_mut_obj_add_str(doc, root, "message", error_message(result.err));
        yyjson_mut_obj_add_int(doc, root, "code", result.err->code);
        yyjson_mut_obj_add_str(doc, root, "file", result.err->file);
        yyjson_mut_obj_add_int(doc, root, "line", result.err->line);
        ik_logger_error_json(logger, doc);
    }

    talloc_free(root_ctx);  // Free all app resources first

    // Determine exit code
    int exit_code = is_ok(&result) ? EXIT_SUCCESS : EXIT_FAILURE;

    // Log session end
    doc = ik_log_create();
    root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "session_end");
    yyjson_mut_obj_add_int(doc, root, "exit_code", exit_code);
    ik_logger_info_json(logger, doc);

    g_panic_logger = NULL;   // Disable panic logging
    talloc_free(logger_ctx); // Logger last

    return exit_code;
}

/* LCOV_EXCL_STOP */
