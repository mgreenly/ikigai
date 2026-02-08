/**
 * @file commands_mail_helpers.c
 * @brief Helper functions for mail command implementations
 */

#include "apps/ikigai/commands_mail_helpers.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>


#include "shared/poison.h"

bool ik_mail_parse_uuid(const char *args, char *uuid_out)
{
    assert(args != NULL);      // LCOV_EXCL_BR_LINE
    assert(uuid_out != NULL);  // LCOV_EXCL_BR_LINE

    // Skip leading whitespace
    const char *p = args;
    while (*p && isspace((unsigned char)*p)) {     // LCOV_EXCL_BR_LINE
        p++;
    }

    const char *uuid_start = p;
    while (*p && !isspace((unsigned char)*p)) {     // LCOV_EXCL_BR_LINE
        p++;
    }

    if (p == uuid_start) {     // LCOV_EXCL_BR_LINE
        return false;
    }

    size_t uuid_len = (size_t)(p - uuid_start);
    if (uuid_len >= 256) {     // LCOV_EXCL_BR_LINE
        return false;
    }

    memcpy(uuid_out, uuid_start, uuid_len);
    uuid_out[uuid_len] = '\0';
    return true;
}

