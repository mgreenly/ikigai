#include "domain_utils.h"

#include <string.h>
#include <strings.h>

int32_t url_matches_domain(const char *url, const char *domain)
{
    if (url == NULL || domain == NULL) {
        return 0;
    }

    const char *start = strstr(url, "://");
    if (start == NULL) {
        start = url;
    } else {
        start += 3;
    }

    const char *end = strchr(start, '/');
    size_t host_len;
    if (end == NULL) {
        host_len = strlen(start);
    } else {
        host_len = (size_t)(end - start);
    }

    size_t domain_len = strlen(domain);

    if (host_len == domain_len) {
        return (strncasecmp(start, domain, host_len) == 0);
    } else if (host_len > domain_len) {
        const char *suffix = start + (host_len - domain_len);
        if (suffix > start && suffix[-1] == '.') {
            return (strncasecmp(suffix, domain, domain_len) == 0);
        }
    }

    return 0;
}
