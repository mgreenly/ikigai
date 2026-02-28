/* LCOV_EXCL_START */
#include "apps/ikigai/debug_log.h"

#ifdef DEBUG

#include "shared/panic.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/stat.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>


#include "shared/poison.h"
#define DEBUG_LOG_CURRENT "debug.log"

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

static int mkdir_p(const char *path, mode_t mode)
{
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len == 0 || len >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(tmp, path, len + 1);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

void ik_debug_log_init(void)
{
    const char *log_dir = getenv("IKIGAI_LOG_DIR");
    if (log_dir == NULL || log_dir[0] == '\0') {
        PANIC("IKIGAI_LOG_DIR environment variable is not set");
    }

    struct stat dir_st;
    if (stat(log_dir, &dir_st) != 0) {
        if (mkdir_p(log_dir, 0755) != 0) {
            PANIC("Failed to create IKIGAI_LOG_DIR");
        }
    }

    char current_path[PATH_MAX];
    snprintf(current_path, sizeof(current_path), "%s/%s", log_dir, DEBUG_LOG_CURRENT);

    struct stat cur_st;
    if (stat(current_path, &cur_st) == 0) {
        struct timespec birth = {0};
        struct statx stx = {0};
        if (syscall(__NR_statx, AT_FDCWD, current_path, 0,
                    STATX_BTIME, &stx) == 0 &&
            (stx.stx_mask & STATX_BTIME)) {
            birth.tv_sec  = stx.stx_btime.tv_sec;
            birth.tv_nsec = stx.stx_btime.tv_nsec;
        } else {
            birth.tv_sec  = cur_st.st_mtime;
            birth.tv_nsec = 0;
        }

        struct tm *tm_info = localtime(&birth.tv_sec);
        long off_sec = tm_info->tm_gmtoff;
        char sign = (off_sec >= 0) ? '+' : '-';
        if (off_sec < 0) off_sec = -off_sec;
        int off_h = (int)(off_sec / 3600);
        int off_m = (int)((off_sec % 3600) / 60);

        char ts[64];
        snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d-%02d-%02d%c%02d-%02d",
                 tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                 tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
                 sign, off_h, off_m);

        char archive_path[PATH_MAX];
        snprintf(archive_path, sizeof(archive_path), "%s/%s.log", log_dir, ts);

        struct stat arc_st;
        if (stat(archive_path, &arc_st) != 0) {
            rename(current_path, archive_path);
        }
    }

    g_debug_log = fopen(current_path, "w");
    if (g_debug_log == NULL) {
        PANIC("Failed to create debug log file");
    }
    atexit(ik_debug_log_cleanup);
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
