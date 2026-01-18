#include "credentials.h"

#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

#include "json_allocator.h"

#include "vendor/yyjson/yyjson.h"

static int32_t load_config(void *ctx, char *path, const char *key, char **out)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }

    size_t bsz = 2048;
    size_t total = 0;
    char *c = talloc_array(ctx, char, (unsigned int)bsz);
    if (c == NULL) {
        fclose(fp);
        return -1;
    }

    size_t nr;
    while ((nr = fread(c + total, 1, bsz - total, fp)) > 0) {
        total += nr;

        if (total >= bsz) {
            bsz *= 2;
            c = talloc_realloc(ctx, c, char, (unsigned int)bsz);
            if (c == NULL) {
                fclose(fp);
                return -1;
            }
        }
    }

    fclose(fp);

    if (total < bsz) {
        c[total] = '\0';
    } else {
        c = talloc_realloc(ctx, c, char, (unsigned int)(total + 1));
        if (c == NULL) {
            return -1;
        }
        c[total] = '\0';
    }

    yyjson_alc a = ik_make_talloc_allocator(ctx);
    yyjson_doc *d = yyjson_read_opts(c, total, 0, &a, NULL);
    if (d == NULL) {
        return -1;
    }

    yyjson_val *r = yyjson_doc_get_root(d);
    yyjson_val *ws = yyjson_obj_get(r, "web_search");
    if (ws == NULL) {
        return -1;
    }

    yyjson_val *g = yyjson_obj_get(ws, "google");
    if (g == NULL) {
        return -1;
    }

    yyjson_val *k = yyjson_obj_get(g, key);
    if (k == NULL || !yyjson_is_str(k)) {
        return -1;
    }

    *out = talloc_strdup(ctx, yyjson_get_str(k));

    return 0;
}

static const char *get_home(void)
{
    const char *h = getenv("HOME");
    if (h == NULL) {
        struct passwd *pw = getpwuid(getuid());
        if (pw != NULL) h = pw->pw_dir;
    }
    return h;
}

int32_t load_credentials(void *c, char **k, char **e)
{
    const char *ke = getenv("GOOGLE_SEARCH_API_KEY");
    if (ke != NULL) {
        *k = talloc_strdup(c, ke);
    } else {
        const char *h = get_home();
        if (h != NULL) {
            char *p = talloc_asprintf(c, "%s/.config/ikigai/credentials.json", h);
            if (load_config(c, p, "api_key", k) != 0) *k = NULL;
        } else {
            *k = NULL;
        }
    }
    const char *ee = getenv("GOOGLE_SEARCH_ENGINE_ID");
    if (ee != NULL) {
        *e = talloc_strdup(c, ee);
    } else {
        const char *h = get_home();
        if (h != NULL) {
            char *p = talloc_asprintf(c, "%s/.config/ikigai/credentials.json", h);
            if (load_config(c, p, "engine_id", e) != 0) *e = NULL;
        } else {
            *e = NULL;
        }
    }
    if (*k == NULL || *e == NULL) return -1;
    if (strlen(*k) == 0 || strlen(*e) == 0) return -1;
    return 0;
}
