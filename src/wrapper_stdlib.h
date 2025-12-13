// C standard library wrappers for testing
#ifndef IK_WRAPPER_STDLIB_H
#define IK_WRAPPER_STDLIB_H

#include <stdarg.h>
#include <stddef.h>
#include <time.h>
#include "wrapper_base.h"

#ifdef NDEBUG

MOCKABLE int vsnprintf_(char *str, size_t size, const char *format, va_list ap)
{
    return vsnprintf(str, size, format, ap);
}

MOCKABLE int snprintf_(char *str, size_t size, const char *format, ...)
{
    va_list ap; va_start(ap, format);
    int result = vsnprintf(str, size, format, ap);
    va_end(ap); return result;
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

#else

MOCKABLE int vsnprintf_(char *str, size_t size, const char *format, va_list ap);
MOCKABLE int snprintf_(char *str, size_t size, const char *format, ...);
MOCKABLE struct tm *gmtime_(const time_t *timep);
MOCKABLE size_t strftime_(char *s, size_t max, const char *format, const struct tm *tm);

#endif

#endif // IK_WRAPPER_STDLIB_H
