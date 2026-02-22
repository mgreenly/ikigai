#include "shared/logger.h"
#include "shared/panic.h"
#include "shared/wrapper.h"
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


#include "shared/poison.h"
struct ik_logger {
    FILE *file;
    pthread_mutex_t mutex;
};

static pthread_mutex_t ik_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE *ik_log_file = NULL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static void ik_log_format_timestamp_impl(char *buf, size_t buf_len, const char *fmt)
{
    assert(buf != NULL);        // LCOV_EXCL_BR_LINE
    assert(buf_len >= 64);      // LCOV_EXCL_BR_LINE

    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);

    int milliseconds = (int)(tv.tv_usec / 1000);
    long offset_seconds = tm.tm_gmtoff;
    int offset_hours = (int)(offset_seconds / 3600);
    int offset_minutes = (int)((offset_seconds % 3600) / 60);

    snprintf(buf, buf_len, fmt,
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             milliseconds, offset_hours, abs(offset_minutes));
}

#pragma GCC diagnostic pop

static void ik_log_format_timestamp(char *buf, size_t buf_len)
{
    ik_log_format_timestamp_impl(buf, buf_len, "%04d-%02d-%02dT%02d:%02d:%02d.%03d%+03d:%02d");
}

static void ik_log_format_archive_timestamp(char *buf, size_t buf_len)
{
    ik_log_format_timestamp_impl(buf, buf_len, "%04d-%02d-%02dT%02d-%02d-%02d.%03d%+03d-%02d");
}

static void ik_log_rotate_if_exists(const char *log_path)
{
    assert(log_path != NULL); // LCOV_EXCL_BR_LINE

    if (posix_access_(log_path, F_OK) != 0) {
        return;
    }

    char timestamp[64];
    ik_log_format_archive_timestamp(timestamp, sizeof(timestamp));

    char archive_path[512];
    int ret = snprintf(archive_path, sizeof(archive_path), "%.*s/%s.log",
                       (int)(strrchr(log_path, '/') - log_path), log_path, timestamp);
    if (ret < 0 || (size_t)ret >= sizeof(archive_path)) {  // LCOV_EXCL_BR_LINE
        PANIC("Path too long for archive file");  // LCOV_EXCL_LINE
    }

    if (posix_rename_(log_path, archive_path) != 0) {
        return;
    }
}

static void ik_log_setup_directories(const char *working_dir, char *log_path)
{
    assert(working_dir != NULL); // LCOV_EXCL_BR_LINE
    assert(log_path != NULL); // LCOV_EXCL_BR_LINE

    const char *env_log_dir = getenv("IKIGAI_LOG_DIR");
    if (env_log_dir != NULL && env_log_dir[0] != '\0') {
        struct stat st;
        if (posix_stat_(env_log_dir, &st) != 0) {  // LCOV_EXCL_BR_LINE
            if (errno == ENOENT) {  // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE
                if (posix_mkdir_(env_log_dir, 0755) != 0) {  // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE
                    PANIC("Failed to create IKIGAI_LOG_DIR directory");  // LCOV_EXCL_LINE
                }
            } else {  // LCOV_EXCL_START
                PANIC("Failed to stat IKIGAI_LOG_DIR directory");
            }  // LCOV_EXCL_STOP
        }

        int ret = snprintf(log_path, 512, "%s/current.log", env_log_dir);
        if (ret < 0 || ret >= 512) {  // LCOV_EXCL_BR_LINE
            PANIC("Path too long for log file");  // LCOV_EXCL_LINE
        }
        return;
    }

    char ikigai_dir[512];
    int ret = snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", working_dir);
    if (ret < 0 || (size_t)ret >= sizeof(ikigai_dir)) {  // LCOV_EXCL_BR_LINE
        PANIC("Path too long for .ikigai directory");  // LCOV_EXCL_LINE
    }

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

    char logs_dir[512];
    ret = snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", working_dir);
    if (ret < 0 || (size_t)ret >= sizeof(logs_dir)) {  // LCOV_EXCL_BR_LINE
        PANIC("Path too long for logs directory");  // LCOV_EXCL_LINE
    }

    if (posix_stat_(logs_dir, &st) != 0) {
        if (errno == ENOENT) {  // LCOV_EXCL_BR_LINE
            if (posix_mkdir_(logs_dir, 0755) != 0) {  // LCOV_EXCL_BR_LINE
                PANIC("Failed to create logs directory");  // LCOV_EXCL_LINE
            }
        } else {  // LCOV_EXCL_START
            PANIC("Failed to stat logs directory");
        }  // LCOV_EXCL_STOP
    }

    ret = snprintf(log_path, 512, "%s/.ikigai/logs/current.log", working_dir);
    if (ret < 0 || ret >= 512) {  // LCOV_EXCL_BR_LINE
        PANIC("Path too long for log file");  // LCOV_EXCL_LINE
    }
}

void ik_log_init(const char *working_dir)
{
    assert(working_dir != NULL); // LCOV_EXCL_BR_LINE

    char log_path[512];
    ik_log_setup_directories(working_dir, log_path);

    pthread_mutex_lock(&ik_log_mutex);
    ik_log_rotate_if_exists(log_path);

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

yyjson_mut_doc *ik_log_create(void)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_doc_set_root(doc, root);
    return doc;
}

static char *ik_log_create_jsonl(const char *level, yyjson_mut_doc *doc)
{
    assert(level != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *original_root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_doc *wrapper_doc = yyjson_mut_doc_new(NULL);
    if (wrapper_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *wrapper_root = yyjson_mut_obj(wrapper_doc);
    if (wrapper_root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_doc_set_root(wrapper_doc, wrapper_root);
    yyjson_mut_obj_add_str(wrapper_doc, wrapper_root, "level", level);

    char timestamp_buf[64];
    ik_log_format_timestamp(timestamp_buf, sizeof(timestamp_buf));
    yyjson_mut_obj_add_str(wrapper_doc, wrapper_root, "timestamp", timestamp_buf);

    yyjson_mut_val *logline_copy = yyjson_mut_val_mut_copy(wrapper_doc, original_root);
    if (logline_copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(wrapper_doc, wrapper_root, "logline", logline_copy);

    char *json_str = yyjson_mut_write(wrapper_doc, 0, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_doc_free(wrapper_doc);
    return json_str;
}

static void ik_log_write(const char *level, yyjson_mut_doc *doc)
{
    if (ik_log_file == NULL) {
        yyjson_mut_doc_free(doc);
        return;
    }

    char *json_str = ik_log_create_jsonl(level, doc);

    pthread_mutex_lock(&ik_log_mutex);
    if (fprintf(ik_log_file, "%s\n", json_str) < 0) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to write to log file");  // LCOV_EXCL_LINE
    }
    if (fflush(ik_log_file) != 0) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to flush log file");  // LCOV_EXCL_LINE
    }
    pthread_mutex_unlock(&ik_log_mutex);

    free(json_str);
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

static int logger_destructor(ik_logger_t *logger)
{
    pthread_mutex_lock(&logger->mutex);
    if (logger->file != NULL) {  // LCOV_EXCL_BR_LINE
        if (fclose_(logger->file) != 0) {  // LCOV_EXCL_BR_LINE
            pthread_mutex_unlock(&logger->mutex);  // LCOV_EXCL_LINE
            PANIC("Failed to close log file");  // LCOV_EXCL_LINE
        }
        logger->file = NULL;
    }
    pthread_mutex_unlock(&logger->mutex);
    pthread_mutex_destroy(&logger->mutex);
    return 0;
}

ik_logger_t *ik_logger_create(TALLOC_CTX *ctx, const char *working_dir)
{
    assert(ctx != NULL);         // LCOV_EXCL_BR_LINE
    assert(working_dir != NULL); // LCOV_EXCL_BR_LINE

    ik_logger_t *logger = talloc_zero(ctx, ik_logger_t);
    if (logger == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (pthread_mutex_init(&logger->mutex, NULL) != 0) {  // LCOV_EXCL_BR_LINE
        talloc_free(logger);  // LCOV_EXCL_LINE
        PANIC("Failed to initialize mutex");  // LCOV_EXCL_LINE
    }

    char log_path[512];
    ik_log_setup_directories(working_dir, log_path);

    pthread_mutex_lock(&logger->mutex);
    ik_log_rotate_if_exists(log_path);

    logger->file = fopen_(log_path, "w");
    if (logger->file == NULL) {  // LCOV_EXCL_BR_LINE
        pthread_mutex_unlock(&logger->mutex);  // LCOV_EXCL_LINE
        pthread_mutex_destroy(&logger->mutex);  // LCOV_EXCL_LINE
        talloc_free(logger);  // LCOV_EXCL_LINE
        PANIC("Failed to open log file");  // LCOV_EXCL_LINE
    }

    pthread_mutex_unlock(&logger->mutex);
    talloc_set_destructor(logger, logger_destructor);
    return logger;
}

void ik_logger_reinit(ik_logger_t *logger, const char *working_dir)
{
    if (logger == NULL) return;  // LCOV_EXCL_LINE
    assert(working_dir != NULL); // LCOV_EXCL_BR_LINE

    char log_path[512];
    ik_log_setup_directories(working_dir, log_path);

    pthread_mutex_lock(&logger->mutex);
    if (logger->file != NULL) {  // LCOV_EXCL_BR_LINE
        if (fclose_(logger->file) != 0) {  // LCOV_EXCL_BR_LINE
            pthread_mutex_unlock(&logger->mutex);  // LCOV_EXCL_LINE
            PANIC("Failed to close log file");  // LCOV_EXCL_LINE
        }
        logger->file = NULL;
    }

    ik_log_rotate_if_exists(log_path);
    logger->file = fopen_(log_path, "w");
    if (logger->file == NULL) {  // LCOV_EXCL_BR_LINE
        pthread_mutex_unlock(&logger->mutex);  // LCOV_EXCL_LINE
        PANIC("Failed to open log file");  // LCOV_EXCL_LINE
    }
    pthread_mutex_unlock(&logger->mutex);
}

static void ik_logger_write(ik_logger_t *logger, const char *level, yyjson_mut_doc *doc)
{
    if (logger == NULL) {  // LCOV_EXCL_START
        yyjson_mut_doc_free(doc);
        return;
    }  // LCOV_EXCL_STOP

    char *json_str = ik_log_create_jsonl(level, doc);

    pthread_mutex_lock(&logger->mutex);
    if (fprintf(logger->file, "%s\n", json_str) < 0) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to write to log file");  // LCOV_EXCL_LINE
    }
    if (fflush(logger->file) != 0) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to flush log file");  // LCOV_EXCL_LINE
    }
    pthread_mutex_unlock(&logger->mutex);

    free(json_str);
    yyjson_mut_doc_free(doc);
}

void ik_logger_debug_json(ik_logger_t *logger, yyjson_mut_doc *doc)
{
    ik_logger_write(logger, "debug", doc);
}

void ik_logger_info_json(ik_logger_t *logger, yyjson_mut_doc *doc)
{
    ik_logger_write(logger, "info", doc);
}

void ik_logger_warn_json(ik_logger_t *logger, yyjson_mut_doc *doc)
{
    ik_logger_write(logger, "warn", doc);
}

void ik_logger_error_json(ik_logger_t *logger, yyjson_mut_doc *doc)
{
    ik_logger_write(logger, "error", doc);
}

int ik_logger_get_fd(ik_logger_t *logger)
{
    if (logger == NULL || logger->file == NULL) {  // LCOV_EXCL_BR_LINE
        return -1;  // LCOV_EXCL_LINE
    }  // LCOV_EXCL_LINE
    return fileno(logger->file);
}
