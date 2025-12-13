// Logger module implementation

#include "logger.h"
#include "panic.h"
#include "wrapper.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

// Global mutex for thread-safe logging
static pthread_mutex_t ik_log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global log file handle
static FILE *ik_log_file = NULL;

// Format timestamp as ISO 8601 with milliseconds and local timezone offset
// Format: YYYY-MM-DDTHH:MM:SS.mmm±HH:MM
// Buffer must be at least 64 bytes to satisfy release build static analysis
static void ik_log_format_timestamp(char *buf, size_t buf_len)
{
    assert(buf != NULL);        // LCOV_EXCL_BR_LINE
    assert(buf_len >= 64);      // LCOV_EXCL_BR_LINE

    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);

    // Calculate milliseconds from microseconds
    int milliseconds = (int)(tv.tv_usec / 1000);

    // Calculate timezone offset in minutes
    long offset_seconds = tm.tm_gmtoff;
    int offset_hours = (int)(offset_seconds / 3600);
    int offset_minutes = (int)((offset_seconds % 3600) / 60);

    // Format: YYYY-MM-DDTHH:MM:SS.mmm±HH:MM
    snprintf(buf, buf_len, "%04d-%02d-%02dT%02d:%02d:%02d.%03d%+03d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             milliseconds,
             offset_hours, abs(offset_minutes));
}

// Logger initialization and shutdown

// Format timestamp for archive filename (filesystem-safe: colons replaced with hyphens)
// Format: YYYY-MM-DDTHH-MM-SS.sss±HH-MM
// Buffer must be at least 64 bytes to satisfy release build static analysis
static void ik_log_format_archive_timestamp(char *buf, size_t buf_len)
{
    assert(buf != NULL);        // LCOV_EXCL_BR_LINE
    assert(buf_len >= 64);      // LCOV_EXCL_BR_LINE

    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);

    // Calculate milliseconds from microseconds
    int milliseconds = (int)(tv.tv_usec / 1000);

    // Calculate timezone offset in minutes
    long offset_seconds = tm.tm_gmtoff;
    int offset_hours = (int)(offset_seconds / 3600);
    int offset_minutes = (int)((offset_seconds % 3600) / 60);

    // Format: YYYY-MM-DDTHH-MM-SS.sss±HH-MM (with hyphens instead of colons)
    snprintf(buf, buf_len, "%04d-%02d-%02dT%02d-%02d-%02d.%03d%+03d-%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             milliseconds,
             offset_hours, abs(offset_minutes));
}

// Rotate existing current.log to timestamped archive if it exists
// Must be called with mutex locked
static void ik_log_rotate_if_exists(const char *log_path)
{
    assert(log_path != NULL); // LCOV_EXCL_BR_LINE

    // Check if current.log exists
    if (posix_access_(log_path, F_OK) != 0) {
        // File doesn't exist, no rotation needed
        return;
    }

    // Generate timestamped archive filename
    char timestamp[64];
    ik_log_format_archive_timestamp(timestamp, sizeof(timestamp));

    // Construct archive path (replace current.log with timestamp.log)
    char archive_path[512];
    int ret = snprintf(archive_path, sizeof(archive_path), "%.*s/%s.log",
                       (int)(strrchr(log_path, '/') - log_path), log_path, timestamp);
    if (ret < 0 || (size_t)ret >= sizeof(archive_path)) {  // LCOV_EXCL_BR_LINE
        PANIC("Path too long for archive file");  // LCOV_EXCL_LINE
    }

    // Rename current.log to archive
    if (posix_rename_(log_path, archive_path) != 0) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to rotate log file");  // LCOV_EXCL_LINE
    }
}

// Set up log directories for a working directory
// Returns path to log file via log_path parameter (must be at least 512 bytes)
// Must NOT be called with mutex locked
static void ik_log_setup_directories(const char *working_dir, char *log_path)
{
    assert(working_dir != NULL); // LCOV_EXCL_BR_LINE
    assert(log_path != NULL); // LCOV_EXCL_BR_LINE

    // Construct path to .ikigai directory
    char ikigai_dir[512];
    int ret = snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", working_dir);
    if (ret < 0 || (size_t)ret >= sizeof(ikigai_dir)) {  // LCOV_EXCL_BR_LINE
        PANIC("Path too long for .ikigai directory");  // LCOV_EXCL_LINE
    }

    // Create .ikigai directory if it doesn't exist
    struct stat st;
    if (posix_stat_(ikigai_dir, &st) != 0) {
        if (errno == ENOENT) {  // LCOV_EXCL_BR_LINE
            if (posix_mkdir_(ikigai_dir, 0755) != 0) {  // LCOV_EXCL_BR_LINE
                PANIC("Failed to create .ikigai directory");  // LCOV_EXCL_LINE
            }
        } else {  // LCOV_EXCL_START
            PANIC("Failed to stat .ikigai directory");
        }  // LCOV_EXCL_STOP
    }

    // Construct path to .ikigai/logs directory
    char logs_dir[512];
    ret = snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", working_dir);
    if (ret < 0 || (size_t)ret >= sizeof(logs_dir)) {  // LCOV_EXCL_BR_LINE
        PANIC("Path too long for logs directory");  // LCOV_EXCL_LINE
    }

    // Create logs directory if it doesn't exist
    if (posix_stat_(logs_dir, &st) != 0) {
        if (errno == ENOENT) {  // LCOV_EXCL_BR_LINE
            if (posix_mkdir_(logs_dir, 0755) != 0) {  // LCOV_EXCL_BR_LINE
                PANIC("Failed to create logs directory");  // LCOV_EXCL_LINE
            }
        } else {  // LCOV_EXCL_START
            PANIC("Failed to stat logs directory");
        }  // LCOV_EXCL_STOP
    }

    // Construct path to current.log file
    ret = snprintf(log_path, 512, "%s/.ikigai/logs/current.log", working_dir);
    if (ret < 0 || ret >= 512) {  // LCOV_EXCL_BR_LINE
        PANIC("Path too long for log file");  // LCOV_EXCL_LINE
    }
}

void ik_log_init(const char *working_dir)
{
    assert(working_dir != NULL); // LCOV_EXCL_BR_LINE

    // Set up directories and get log file path
    char log_path[512];
    ik_log_setup_directories(working_dir, log_path);

    // Lock mutex before file operations (rotation must be atomic)
    pthread_mutex_lock(&ik_log_mutex);

    // Rotate existing current.log to timestamped archive if it exists
    ik_log_rotate_if_exists(log_path);

    // Open new log file in write mode (truncate if exists after rotation)
    ik_log_file = fopen_(log_path, "w");
    if (ik_log_file == NULL) {  // LCOV_EXCL_BR_LINE
        pthread_mutex_unlock(&ik_log_mutex);  // LCOV_EXCL_LINE
        PANIC("Failed to open log file");  // LCOV_EXCL_LINE
    }
    pthread_mutex_unlock(&ik_log_mutex);
}

void ik_log_shutdown(void)
{
    pthread_mutex_lock(&ik_log_mutex);
    if (ik_log_file != NULL) {
        if (fclose_(ik_log_file) != 0) {  // LCOV_EXCL_BR_LINE
            pthread_mutex_unlock(&ik_log_mutex);  // LCOV_EXCL_LINE
            PANIC("Failed to close log file");  // LCOV_EXCL_LINE
        }
        ik_log_file = NULL;
    }
    pthread_mutex_unlock(&ik_log_mutex);
}

void ik_log_reinit(const char *working_dir)
{
    assert(working_dir != NULL); // LCOV_EXCL_BR_LINE

    // Set up directories and get log file path
    char log_path[512];
    ik_log_setup_directories(working_dir, log_path);

    // Lock mutex before file operations (close, rotate, open must be atomic)
    pthread_mutex_lock(&ik_log_mutex);

    // Close current log file if open
    if (ik_log_file != NULL) {
        if (fclose_(ik_log_file) != 0) {  // LCOV_EXCL_BR_LINE
            pthread_mutex_unlock(&ik_log_mutex);  // LCOV_EXCL_LINE
            PANIC("Failed to close log file");  // LCOV_EXCL_LINE
        }
        ik_log_file = NULL;
    }

    // Rotate existing current.log to timestamped archive if it exists in new location
    ik_log_rotate_if_exists(log_path);

    // Open new log file in write mode (truncate if exists after rotation)
    ik_log_file = fopen_(log_path, "w");
    if (ik_log_file == NULL) {  // LCOV_EXCL_BR_LINE
        pthread_mutex_unlock(&ik_log_mutex);  // LCOV_EXCL_LINE
        PANIC("Failed to open log file");  // LCOV_EXCL_LINE
    }
    pthread_mutex_unlock(&ik_log_mutex);
}

// New JSONL logging API

yyjson_mut_doc *ik_log_create(void)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_doc_set_root(doc, root);
    return doc;
}

// Internal function to write JSONL with a specific level
static void ik_log_write(const char *level, yyjson_mut_doc *doc)
{
    // If logger not initialized, free doc and return silently
    if (ik_log_file == NULL) {
        yyjson_mut_doc_free(doc);
        return;
    }

    assert(level != NULL); // LCOV_EXCL_BR_LINE

    // Get original root object from doc
    yyjson_mut_val *original_root = yyjson_mut_doc_get_root(doc);

    // Create new wrapper doc
    yyjson_mut_doc *wrapper_doc = yyjson_mut_doc_new(NULL);
    if (wrapper_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *wrapper_root = yyjson_mut_obj(wrapper_doc);
    if (wrapper_root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_doc_set_root(wrapper_doc, wrapper_root);

    // Add "level" field with the provided level string
    yyjson_mut_obj_add_str(wrapper_doc, wrapper_root, "level", level);

    // Format and add "timestamp" field with ISO 8601 format
    char timestamp_buf[64];
    ik_log_format_timestamp(timestamp_buf, sizeof(timestamp_buf));
    yyjson_mut_obj_add_str(wrapper_doc, wrapper_root, "timestamp", timestamp_buf);

    // Add "logline" field with original root object
    // Copy the original root to the wrapper doc
    yyjson_mut_val *logline_copy = yyjson_mut_val_mut_copy(wrapper_doc, original_root);
    if (logline_copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(wrapper_doc, wrapper_root, "logline", logline_copy);

    // Serialize to JSON string (single line)
    char *json_str = yyjson_mut_write(wrapper_doc, 0, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Write to log file with newline
    pthread_mutex_lock(&ik_log_mutex);
    if (fprintf(ik_log_file, "%s\n", json_str) < 0) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to write to log file");  // LCOV_EXCL_LINE
    }
    if (fflush(ik_log_file) != 0) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to flush log file");  // LCOV_EXCL_LINE
    }
    pthread_mutex_unlock(&ik_log_mutex);

    // Free JSON string
    free(json_str);

    // Free wrapper doc
    yyjson_mut_doc_free(wrapper_doc);

    // Free original doc
    yyjson_mut_doc_free(doc);
}

void ik_log_debug_json(yyjson_mut_doc *doc)
{
    ik_log_write("debug", doc);
}

void ik_log_info_json(yyjson_mut_doc *doc)
{
    ik_log_write("info", doc);
}

void ik_log_warn_json(yyjson_mut_doc *doc)
{
    ik_log_write("warn", doc);
}

void ik_log_error_json(yyjson_mut_doc *doc)
{
    ik_log_write("error", doc);
}

void ik_log_fatal_json(yyjson_mut_doc *doc)
{
    ik_log_write("fatal", doc);
    exit(1);
}
