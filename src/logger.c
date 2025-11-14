// Logger module implementation

#include "logger.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// Global mutex for thread-safe logging
static pthread_mutex_t ik_log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Timestamp control - checked once on first log call
static bool ik_log_timestamps_enabled = true;
static bool ik_log_timestamps_checked = false;

static void ik_log_check_timestamp_mode(void)
{
    if (ik_log_timestamps_checked)
        return;

    ik_log_timestamps_checked = true;

    // If JOURNAL_STREAM is set, we're running under systemd
    if (getenv("JOURNAL_STREAM") != NULL) {
        ik_log_timestamps_enabled = false;
    }
}

static void ik_log_print_timestamp(FILE *stream)
{
    assert(stream != NULL); // LCOV_EXCL_BR_LINE

    if (!ik_log_timestamps_enabled)
        return;

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    fprintf(stream, "%04d-%02d-%02d %02d:%02d:%02d ",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void ik_log_debug(const char *fmt, ...)
{
    assert(fmt != NULL); // LCOV_EXCL_BR_LINE

    pthread_mutex_lock(&ik_log_mutex);
    ik_log_check_timestamp_mode();
    ik_log_print_timestamp(stdout);
    fprintf(stdout, "DEBUG: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
    fflush(stdout);
    pthread_mutex_unlock(&ik_log_mutex);
}

void ik_log_info(const char *fmt, ...)
{
    assert(fmt != NULL); // LCOV_EXCL_BR_LINE

    pthread_mutex_lock(&ik_log_mutex);
    ik_log_check_timestamp_mode();
    ik_log_print_timestamp(stdout);
    fprintf(stdout, "INFO: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
    fflush(stdout);
    pthread_mutex_unlock(&ik_log_mutex);
}

void ik_log_warn(const char *fmt, ...)
{
    assert(fmt != NULL); // LCOV_EXCL_BR_LINE

    pthread_mutex_lock(&ik_log_mutex);
    ik_log_check_timestamp_mode();
    ik_log_print_timestamp(stdout);
    fprintf(stdout, "WARN: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
    fflush(stdout);
    pthread_mutex_unlock(&ik_log_mutex);
}

void ik_log_error(const char *fmt, ...)
{
    assert(fmt != NULL); // LCOV_EXCL_BR_LINE

    pthread_mutex_lock(&ik_log_mutex);
    ik_log_check_timestamp_mode();
    ik_log_print_timestamp(stderr);
    fprintf(stderr, "ERROR: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
    pthread_mutex_unlock(&ik_log_mutex);
}

void ik_log_reset_timestamp_check(void)
{
    pthread_mutex_lock(&ik_log_mutex);
    ik_log_timestamps_checked = false;
    ik_log_timestamps_enabled = true;
    pthread_mutex_unlock(&ik_log_mutex);
}
