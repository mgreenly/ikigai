## Overview

The main entry point for the ikigai client application. Orchestrates application initialization in a carefully controlled sequence: bootstrap the logger, load configuration, initialize shared context and terminal, set up the REPL (read-eval-print loop), and run the interactive session with comprehensive error handling and cleanup at each stage. All errors are logged and the application gracefully exits with proper resource cleanup.

## Code

```
function main():
    initialize random seed with current time and process ID

    capture current working directory

    // Logger bootstrap (separate talloc root for independent lifetime)
    allocate logger context
    create logger with current directory
    register logger for panic handler

    // Log session start
    create JSON log entry with event "session_start"
    record the working directory
    write to logger

    // Application initialization (main talloc root)
    allocate application root context

    // Load configuration
    load configuration from ~/.config/ikigai/config.json
    if load fails:
        log config load error with details (error code, file, line)
        log session end with exit failure code
        clean up logger context
        return failure

    // Initialize shared context (database, terminal, file paths, etc.)
    initialize shared context with config, cwd, and logger
    if initialization fails:
        log shared context error with details
        log session end with exit failure code
        clean up root context then logger context
        return failure

    // Initialize REPL
    initialize REPL with shared context
    if initialization fails:
        clean up terminal first (exit alternate buffer)
        log REPL init error with details
        log session end with exit failure code
        clean up root context then logger context
        return failure

    // Set up panic handler to restore terminal on abort
    register global terminal context for panic handler
    set talloc abort function to custom panic handler

    // Run the interactive session
    run REPL event loop

    clean up REPL resources

    // Log any REPL errors
    if REPL run failed:
        log REPL run error with details

    // Final cleanup (order matters: app resources before logger)
    free all application resources via root context

    determine exit code based on REPL result (success or failure)

    // Log session end
    create JSON log entry with event "session_end"
    record exit code
    write to logger

    disable panic logging
    free logger context

    return exit code
```
