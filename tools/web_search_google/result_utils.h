#ifndef RESULT_UTILS_H
#define RESULT_UTILS_H

#include "vendor/yyjson/yyjson.h"

#include <stdbool.h>

bool url_already_seen(yyjson_mut_val *results_arr, const char *url);

#endif
