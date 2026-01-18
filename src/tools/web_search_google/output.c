#include "output.h"

#include "json_allocator.h"
#include "panic.h"

#include "vendor/yyjson/yyjson.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void output_error_with_event(void *ctx, const char *err, const char *code)
{
    yyjson_alc a = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *d = yyjson_mut_doc_new(&a);
    if (d == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *o = yyjson_mut_obj(d);
    if (o == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *s = yyjson_mut_bool(d, false);
    if (s == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *e = yyjson_mut_str(d, err);
    if (e == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *ec = yyjson_mut_str(d, code);
    if (ec == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_val(d, o, "success", s);
    yyjson_mut_obj_add_val(d, o, "error", e);
    yyjson_mut_obj_add_val(d, o, "error_code", ec);
    if (strcmp(code, "AUTH_MISSING") == 0) {
        const char *ev_c = "Need api_key+engine_id. 100/day.\ndevelopers.google.com/custom-search\n~/.config/ikigai/credentials.json:\n{\"web_search\":{\"google\":{\"api_key\":\"k\",\"engine_id\":\"e\"}}}";
        yyjson_mut_val *ev_o = yyjson_mut_obj(d);
        if (ev_o == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *k = yyjson_mut_str(d, "config_required");
        if (k == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *c = yyjson_mut_str(d, ev_c);
        if (c == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *da = yyjson_mut_obj(d);
        if (da == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *t = yyjson_mut_str(d, "web_search_google");
        if (t == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *cr = yyjson_mut_arr(d);
        if (cr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *c1 = yyjson_mut_str(d, "api_key");
        if (c1 == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *c2 = yyjson_mut_str(d, "engine_id");
        if (c2 == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_arr_append(cr, c1);
        yyjson_mut_arr_append(cr, c2);
        yyjson_mut_obj_add_val(d, da, "tool", t);
        yyjson_mut_obj_add_val(d, da, "credentials", cr);
        yyjson_mut_obj_add_val(d, ev_o, "kind", k);
        yyjson_mut_obj_add_val(d, ev_o, "content", c);
        yyjson_mut_obj_add_val(d, ev_o, "data", da);
        yyjson_mut_obj_add_val(d, o, "_event", ev_o);
    }
    yyjson_mut_doc_set_root(d, o);
    char *js = yyjson_mut_write(d, 0, NULL);
    if (js == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    printf("%s\n", js);
    free(js);
}

void output_error(void *ctx, const char *err, const char *code)
{
    yyjson_alc a = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *d = yyjson_mut_doc_new(&a);
    if (d == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *o = yyjson_mut_obj(d);
    if (o == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *s = yyjson_mut_bool(d, false);
    if (s == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *e = yyjson_mut_str(d, err);
    if (e == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *ec = yyjson_mut_str(d, code);
    if (ec == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_val(d, o, "success", s);
    yyjson_mut_obj_add_val(d, o, "error", e);
    yyjson_mut_obj_add_val(d, o, "error_code", ec);
    yyjson_mut_doc_set_root(d, o);
    char *js = yyjson_mut_write(d, 0, NULL);
    if (js == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    printf("%s\n", js);
    free(js);
}
