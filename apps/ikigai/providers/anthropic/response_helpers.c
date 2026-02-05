/**
 * @file response_helpers.c
 * @brief Anthropic response parsing helper functions
 */

#include "apps/ikigai/providers/anthropic/response_helpers.h"

#include "shared/json_allocator.h"
#include "shared/panic.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include "shared/poison.h"
