#ifndef RESULTS_H
#define RESULTS_H

#include <stdbool.h>

#include "vendor/yyjson/yyjson.h"

bool url_seen(yyjson_mut_val *a, const char *u);
void add_result(yyjson_mut_doc *doc, yyjson_mut_val *results, const char *title, const char *url, const char *snippet);

#endif
