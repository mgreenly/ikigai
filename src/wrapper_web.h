// Web-related wrappers for libxml2 HTML parsing
// Curl functions are provided by wrapper_curl.h
#ifndef IK_WRAPPER_WEB_H
#define IK_WRAPPER_WEB_H

#include "wrapper_base.h"
#include "wrapper_curl.h"

// Forward declarations for libxml2 types
typedef struct _xmlDoc *htmlDocPtr;
typedef struct _xmlDoc *xmlDocPtr;

#ifdef NDEBUG

MOCKABLE htmlDocPtr htmlReadMemory_(const char *buffer, int size, const char *URL, const char *encoding, int options)
{
    return htmlReadMemory(buffer, size, URL, encoding, options);
}

MOCKABLE void xmlFreeDoc_(xmlDocPtr cur)
{
    xmlFreeDoc(cur);
}

#else

MOCKABLE htmlDocPtr htmlReadMemory_(const char *buffer, int size, const char *URL, const char *encoding, int options);
MOCKABLE void xmlFreeDoc_(xmlDocPtr cur);

#endif

#endif // IK_WRAPPER_WEB_H
