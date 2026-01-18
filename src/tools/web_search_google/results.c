#include "results.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "vendor/yyjson/yyjson.h"

#define CK(x) if((x)==NULL)exit(1)

bool url_seen(yyjson_mut_val *a, const char *u)
{
    size_t i, m;
    yyjson_mut_val *it;
    yyjson_mut_arr_foreach(a, i, m, it) {
        yyjson_mut_val *v = yyjson_mut_obj_get(it, "url");
        if (v != NULL) {
            const char *s = yyjson_mut_get_str(v);
            if (s != NULL && strcmp(s, u) == 0) return true;
        }
    }
    return false;
}

void add_result(yyjson_mut_doc *doc, yyjson_mut_val *results, const char *title, const char *url, const char *snippet)
{
    yyjson_mut_val *ro = yyjson_mut_obj(doc);
    CK(ro);
    yyjson_mut_val *t1 = yyjson_mut_str(doc, title);
    CK(t1);
    yyjson_mut_val *u1 = yyjson_mut_str(doc, url);
    CK(u1);
    yyjson_mut_val *s1 = yyjson_mut_str(doc, snippet);
    CK(s1);

    yyjson_mut_obj_add_val(doc, ro, "title", t1);
    yyjson_mut_obj_add_val(doc, ro, "url", u1);
    yyjson_mut_obj_add_val(doc, ro, "snippet", s1);

    yyjson_mut_arr_append(results, ro);
}
