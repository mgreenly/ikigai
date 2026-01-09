#ifndef IK_DEBUG_LOG_H
#define IK_DEBUG_LOG_H

#ifdef DEBUG

#include <stdio.h>

// Initialize debug logging - creates/truncates IKIGAI_DEBUG.LOG
// Must be called early in main() before other initialization
// PANICS if file cannot be created
void ik_debug_log_init(void);

// Internal function - use DEBUG_LOG macro instead
void ik_debug_log_write(const char *file, int line, const char *func,
                        const char *fmt, ...) __attribute__((format(printf, 4, 5)));

// Macro that adds file/line/function context and timestamp
#define DEBUG_LOG(fmt, ...) \
    ik_debug_log_write(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#else

// In non-DEBUG builds, everything compiles away to nothing
#define ik_debug_log_init() ((void)0)
#define DEBUG_LOG(fmt, ...) ((void)0)

#endif // DEBUG

#endif // IK_DEBUG_LOG_H
