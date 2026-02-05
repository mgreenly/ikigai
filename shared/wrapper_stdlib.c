// C standard library wrapper implementations
// Link seams that tests can override for failure injection
//
// In release builds (NDEBUG), these are defined as static inline in the header.
// In debug/test builds, these are compiled as weak symbols.

#include "shared/wrapper_stdlib.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#include "shared/poison.h"
#ifndef NDEBUG
// LCOV_EXCL_START

// ============================================================================
// C standard library wrappers - debug/test builds only
// ============================================================================

MOCKABLE int vsnprintf_(char *str, size_t size, const char *format, va_list ap)
{
    return vsnprintf(str, size, format, ap);
}

MOCKABLE int snprintf_(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vsnprintf(str, size, format, ap);
    va_end(ap);
    return result;
}

MOCKABLE struct tm *gmtime_(const time_t *timep)
{
    return gmtime(timep);
}

MOCKABLE size_t strftime_(char *s, size_t max, const char *format, const struct tm *tm)
{
    // Suppress format-nonliteral warning - format string comes from caller
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    return strftime(s, max, format, tm);
#pragma GCC diagnostic pop
}

MOCKABLE char *getenv_(const char *name)
{
    return getenv(name);
}

// LCOV_EXCL_STOP
#endif
