#include "shared/wrapper_web.h"

#include <libxml/HTMLparser.h>
#include <libxml/tree.h>


#include "shared/poison.h"
/* LCOV_EXCL_START */
#ifndef NDEBUG

// In debug builds, provide weak implementations that tests can override
// Curl functions are provided by wrapper_curl.c (with VCR integration)

MOCKABLE htmlDocPtr htmlReadMemory_(const char *buffer, int size, const char *URL, const char *encoding, int options)
{
    return htmlReadMemory(buffer, size, URL, encoding, options);
}

MOCKABLE void xmlFreeDoc_(xmlDocPtr cur)
{
    xmlFreeDoc(cur);
}

#endif
/* LCOV_EXCL_STOP */
