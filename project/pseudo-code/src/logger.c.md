## Overview

Thread-safe JSON logging system that writes structured log entries to files with timestamps. Provides both global logging (single file) and instance-based logging (per-logger file), with automatic log rotation on initialization. Uses mutex protection for concurrent access and JSONL format for all output.

## Code

```
structure Logger:
    file: FILE pointer
    mutex: thread lock

global_file: FILE pointer
global_mutex: thread lock

================================================================================
TIMESTAMP FORMATTING
================================================================================

function format_timestamp_impl(buffer, format_string):
    get current time as microseconds and local time structure
    calculate milliseconds from microseconds
    calculate timezone offset (hours and minutes)

    write formatted timestamp into buffer using provided format string

    return

function format_timestamp(buffer):
    use ISO 8601 format: 2025-12-30T14:05:23.123+00:00
    call format_timestamp_impl with this format

function format_archive_timestamp(buffer):
    use archive format: 2025-12-30T14-05-23.123+00:00
    (colons replaced with dashes for filesystem compatibility)
    call format_timestamp_impl with this format

================================================================================
LOG ROTATION AND DIRECTORY SETUP
================================================================================

function rotate_log_if_exists(log_path):
    if log file does not exist:
        return (nothing to rotate)

    get current timestamp with archive format
    construct archive path: replace "current.log" with "timestamp.log"
    move old log file to archive path with timestamped name

    return

function setup_log_directories(working_directory, output_log_path):
    check IKIGAI_LOG_DIR environment variable:
        if set and not empty:
            validate directory exists (create if needed)
            set log_path to IKIGAI_LOG_DIR/current.log
            return

    construct path: working_directory/.ikigai
    validate directory exists (create if needed)

    construct path: working_directory/.ikigai/logs
    validate directory exists (create if needed)

    set log_path to working_directory/.ikigai/logs/current.log
    return

================================================================================
GLOBAL LOGGING FUNCTIONS
================================================================================

function init_global_logger(working_directory):
    lock global_mutex

    determine log path based on working directory
    rotate existing log file if present
    open log file for writing

    unlock global_mutex

function shutdown_global_logger():
    lock global_mutex

    if global file is open:
        close global file
        clear reference

    unlock global_mutex

function reinit_global_logger(working_directory):
    lock global_mutex

    if global file is open:
        close it

    determine log path based on working directory
    rotate existing log file if present
    open log file for writing

    unlock global_mutex

function create_log_entry():
    allocate new empty JSON document
    return it

function create_jsonl_entry(level, json_document):
    create wrapper JSON object containing:
        "level": level string (debug/info/warn/error/fatal)
        "timestamp": current formatted timestamp
        "logline": copy of original JSON document

    serialize wrapper to JSON string
    free wrapper document
    return JSON string

function write_log(level, json_document):
    if global file not initialized:
        free document
        return

    serialize document to JSONL format with level and timestamp

    lock global_mutex
    write JSON string with newline to global file
    flush file to disk
    unlock global_mutex

    free JSON string and document

function log_debug_json(json_document):
    write_log("debug", json_document)

function log_info_json(json_document):
    write_log("info", json_document)

function log_warn_json(json_document):
    write_log("warn", json_document)

function log_error_json(json_document):
    write_log("error", json_document)

function log_fatal_json(json_document):
    write_log("fatal", json_document)
    exit process with code 1

================================================================================
INSTANCE-BASED LOGGING (per-logger instances)
================================================================================

function cleanup_logger_instance(logger):
    lock logger's mutex

    if logger's file is open:
        close file
        clear reference

    unlock logger's mutex
    destroy logger's mutex

function create_logger_instance(memory_context, working_directory):
    allocate Logger structure from memory context
    initialize mutex for thread safety

    determine log path based on working directory
    rotate existing log file if present
    open log file for writing

    register cleanup_logger_instance as destructor (will run when freed)
    return Logger instance

function reinit_logger_instance(logger, working_directory):
    if logger is null:
        return

    lock logger's mutex

    if logger's file is open:
        close it

    determine log path based on working directory
    rotate existing log file if present
    open log file for writing

    unlock logger's mutex

function write_logger_entry(logger, level, json_document):
    if logger is null or not initialized:
        free document
        return

    serialize document to JSONL format with level and timestamp

    lock logger's mutex
    write JSON string with newline to logger's file
    flush file to disk
    unlock logger's mutex

    free JSON string and document

function logger_debug_json(logger, json_document):
    write_logger_entry(logger, "debug", json_document)

function logger_info_json(logger, json_document):
    write_logger_entry(logger, "info", json_document)

function logger_warn_json(logger, json_document):
    write_logger_entry(logger, "warn", json_document)

function logger_error_json(logger, json_document):
    write_logger_entry(logger, "error", json_document)

function logger_fatal_json(logger, json_document):
    write_logger_entry(logger, "fatal", json_document)
    exit process with code 1

function get_logger_file_descriptor(logger):
    if logger is null or has no file:
        return -1

    return file descriptor of logger's file
```
