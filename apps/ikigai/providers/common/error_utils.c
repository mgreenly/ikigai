#include "apps/ikigai/providers/common/error_utils.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/panic.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>


#include "shared/poison.h"
bool ik_error_is_retryable(int category)
{
    switch (category) {
        case IK_ERR_CAT_RATE_LIMIT:
        case IK_ERR_CAT_SERVER:
        case IK_ERR_CAT_TIMEOUT:
        case IK_ERR_CAT_NETWORK:
            return true;
        case IK_ERR_CAT_AUTH:
        case IK_ERR_CAT_INVALID_ARG:
        case IK_ERR_CAT_NOT_FOUND:
        case IK_ERR_CAT_CONTENT_FILTER:
        case IK_ERR_CAT_UNKNOWN:
            return false;
        default:
            return false;
    }
}

