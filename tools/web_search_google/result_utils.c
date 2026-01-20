#include "result_utils.h"

#include "vendor/yyjson/yyjson.h"

#include <stdbool.h>
#include <string.h>

bool url_already_seen(yyjson_mut_val *results_arr, const char *url)
{
    size_t idx, max;
    yyjson_mut_val *item;
    yyjson_mut_arr_foreach(results_arr, idx, max, item) {  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *url_val = yyjson_mut_obj_get(item, "url");
        if (url_val != NULL) {
            const char *existing_url = yyjson_mut_get_str(url_val);
            if (existing_url != NULL && strcmp(existing_url, url) == 0) {
                return true;
            }
        }
    }
    return false;
}
