# Server Entry Point (`server.c`)

[← Back to Phase 1 Details](phase-1-details.md)

Main entry point for Phase 1. Coordinates module initialization, runs the server, and handles top-level cleanup.

## Responsibilities

- Parse command-line arguments (if any)
- Load configuration via `ik_cfg_load()`
- Initialize logger module
- Start HTTP/WebSocket server via `ik_httpd_run()`
- Handle top-level errors (log and exit with appropriate code)
- Clean up all resources on exit

## Implementation

```c
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // Create top-level talloc context
    TALLOC_CTX *ctx = talloc_new(NULL);
    if (!ctx) {
        fprintf(stderr, "FATAL: Failed to create talloc context\n");
        return EXIT_FAILURE;
    }

    // Load configuration
    ik_result_t res = ik_cfg_load(ctx, "~/.ikigai/config.json");
    if (ik_is_err(&res)) {
        fprintf(stderr, "ERROR: %s\n", ik_error_message(res.err));
        talloc_free(ctx);
        return EXIT_FAILURE;
    }
    ik_cfg_t *config = res.ok;

    // Run server (blocks until shutdown)
    res = ik_httpd_run(ctx, config);
    if (ik_is_err(&res)) {
        ik_log_error("%s", ik_error_message(res.err));
        talloc_free(ctx);
        return EXIT_FAILURE;
    }

    // Clean shutdown
    ik_log_info("Server stopped");
    talloc_free(ctx);
    return EXIT_SUCCESS;
}
```

## Error Handling

**Configuration errors:**
- Missing config file → auto-created with defaults, continue
- Invalid JSON → log error, exit
- Invalid config values → log error, exit
- HOME not set → log error, exit

**Server errors:**
- Port already in use → log error, exit
- Permission denied → log error, exit
- libulfius initialization failure → log error, exit

**All errors are fatal** - server exits with `EXIT_FAILURE` code

## Memory Management

- Single top-level talloc context for entire server lifetime
- Config allocated on this context
- Context freed before exit (both success and error paths)
- ulfius instance is stack-allocated in `ik_httpd_run()`, cleaned up internally

## Logging

- Use `fprintf(stderr, ...)` before logger module is available
- Use `ik_log_*()` after logger module initialized
- All errors logged before exit

## Future Enhancements

**Phase 1 scope:**
- No command-line arguments
- Fixed config path (`~/.ikigai/config.json`)
- No daemon mode
- No PID file

**Future phases may add:**
- `-c <config>` flag for custom config path
- `-d` daemon mode
- `-v` version info
- `--help` usage info
- Signal handling for reload (if needed)
