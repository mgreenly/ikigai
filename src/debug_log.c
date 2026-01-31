/* LCOV_EXCL_START */
#include "debug_log.h"

#ifdef DEBUG

#include "panic.h"

#include <stdarg.h>
#include <stdlib.h>
#include <time.h>


#include "poison.h"
#define DEBUG_LOG_FILENAME "IKIGAI_DEBUG.LOG"

static FILE *g_debug_log = NULL;

// Cleanup handler registered with atexit()
static void ik_debug_log_cleanup(void)
{
    if (g_debug_log != NULL) {
        fflush(g_debug_log);
        fclose(g_debug_log);
        g_debug_log = NULL;
    }
}

void ik_debug_log_init(void)
{
    // Remove existing log file to truncate (ignore errors if doesn't exist)
    remove(DEBUG_LOG_FILENAME);

    // Open in append mode for thread-safe writes (O_APPEND makes writes atomic)
    g_debug_log = fopen(DEBUG_LOG_FILENAME, "a");
    if (g_debug_log == NULL) {
        PANIC("Failed to create debug log file: " DEBUG_LOG_FILENAME);
    }

    // Ensure cleanup happens on any exit path
    atexit(ik_debug_log_cleanup);

    // Write header
    fprintf(g_debug_log, "=== IKIGAI DEBUG LOG ===\n");
    fflush(g_debug_log);
}

void ik_debug_log_write(const char *file, int line, const char *func,
                        const char *fmt, ...)
{
    if (g_debug_log == NULL) {
        return; // Not initialized yet or already cleaned up
    }

    // Get timestamp
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    // Write context: timestamp [file:line:function]
    fprintf(g_debug_log, "[%s] %s:%d:%s: ", timestamp, file, line, func);

    // Write user message
    va_list args;
    va_start(args, fmt);
    vfprintf(g_debug_log, fmt, args);
    va_end(args);

    // Ensure newline
    fprintf(g_debug_log, "\n");

    // Flush immediately so we can tail -f the log
    fflush(g_debug_log);
}

#endif // DEBUG
/* LCOV_EXCL_STOP */
