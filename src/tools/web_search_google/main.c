#include <curl/curl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "credentials.h"
#include "output.h"
#include "http.h"
#include "input.h"
#include "json_allocator.h"
#include "results.h"
#include "schema.h"

#include "vendor/yyjson/yyjson.h"

#define BASE "https://customsearch.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%" PRId64 "&start=%" PRId64
#define CK(x) if((x)==NULL)exit(1)

struct api_c {
    CURL *h;
    struct resp_buf r;
    char *dm;
    int64_t nd;
    bool ok;
    char *u;
};

int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        printf("%s\n", SCHEMA_JSON);
        talloc_free(ctx);
        return 0;
    }

    size_t total = 0;
    char *in = read_stdin_input(ctx, &total);

    if (total == 0) {
        fprintf(stderr, "empty\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_alc a = ik_make_talloc_allocator(ctx);
    yyjson_doc *d = yyjson_read_opts(in, total, 0, &a, NULL);
    if (d == NULL) {
        fprintf(stderr, "bad JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *r = yyjson_doc_get_root(d);
    yyjson_val *qv = yyjson_obj_get(r, "query");
    if (qv == NULL || !yyjson_is_str(qv)) {
        fprintf(stderr, "no query\n");
        talloc_free(ctx);
        return 1;
    }

    const char *q = yyjson_get_str(qv);

    int64_t n = 10;
    yyjson_val *nv = yyjson_obj_get(r, "num");
    if (nv != NULL && yyjson_is_int(nv)) {
        n = yyjson_get_int(nv);
    }

    int64_t st = 1;
    yyjson_val *sv = yyjson_obj_get(r, "start");
    if (sv != NULL && yyjson_is_int(sv)) {
        st = yyjson_get_int(sv);
    }

    yyjson_val *av = yyjson_obj_get(r, "allowed_domains");
    yyjson_val *bv = yyjson_obj_get(r, "blocked_domains");

    size_t al = 0;
    size_t bl = 0;
    if (av != NULL && yyjson_is_arr(av)) {
        al = yyjson_arr_size(av);
    }
    if (bv != NULL && yyjson_is_arr(bv)) {
        bl = yyjson_arr_size(bv);
    }

    char *key = NULL;
    char *eng = NULL;
    if (load_credentials(ctx, &key, &eng) != 0) {
        output_error_with_event(ctx, "Need key+ID. 100/day.\ndevelopers.google.com/custom-search\n~/.config/ikigai/credentials.json", "AUTH_MISSING");
        talloc_free(ctx);
        return 0;
    }

    char *eq = url_encode(ctx, q);
    if (eq == NULL) {
        output_error(ctx, "Encode failed", "NETWORK_ERROR");
        talloc_free(ctx);
        return 0;
    }

    char *ek = url_encode(ctx, key);
    if (ek == NULL) {
        output_error(ctx, "Encode failed", "NETWORK_ERROR");
        talloc_free(ctx);
        return 0;
    }

    char *ee = url_encode(ctx, eng);
    if (ee == NULL) {
        output_error(ctx, "Encode failed", "NETWORK_ERROR");
        talloc_free(ctx);
        return 0;
    }

    struct api_c *calls = NULL;
    size_t nc = 0;

    if (al > 1) {
        nc = al;
        calls = talloc_array(ctx, struct api_c, (unsigned int)nc);
        CK(calls);

        int64_t pd = n / (int64_t)al;
        int64_t rem = n % (int64_t)al;

        for (size_t i = 0; i < al; i++) {
            yyjson_val *dv = yyjson_arr_get(av, i);
            if (dv == NULL || !yyjson_is_str(dv)) {
                continue;
            }

            const char *dom = yyjson_get_str(dv);
            int64_t nd = pd + ((int64_t)i < rem ? 1 : 0);
            if (nd == 0) continue;
            char *ed = url_encode(ctx, dom);
            if (ed == NULL) continue;
            char *url = talloc_asprintf(ctx, BASE "&siteSearch=%s&siteSearchFilter=i", ek, ee, eq, nd, st, ed);
            if (url == NULL) continue;

            calls[i].dm = talloc_strdup(ctx, dom);
            calls[i].nd = nd;
            calls[i].u = url;
            calls[i].ok = false;
            calls[i].r.c = ctx;
            calls[i].r.d = talloc_array(ctx, char, 1);
            if (calls[i].r.d == NULL) continue;
            calls[i].r.d[0] = '\0';
            calls[i].r.s = 0;
            calls[i].h = curl_easy_init();
            if (calls[i].h == NULL) continue;

            curl_easy_setopt(calls[i].h, CURLOPT_URL, url);
            curl_easy_setopt(calls[i].h, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(calls[i].h, CURLOPT_WRITEDATA, (void *)&calls[i].r);
            curl_easy_setopt(calls[i].h, CURLOPT_TIMEOUT, 30L);
        }

        CURLM *mh = curl_multi_init();
        if (mh == NULL) {
            for (size_t i = 0; i < nc; i++) {
                if (calls[i].h != NULL) {
                    curl_easy_cleanup(calls[i].h);
                }
            }
            output_error(ctx, "HTTP init failed", "NETWORK_ERROR");
            talloc_free(ctx);
            return 0;
        }

        for (size_t i = 0; i < nc; i++)
            if (calls[i].h != NULL) curl_multi_add_handle(mh, calls[i].h);

        int32_t run = 0;
        curl_multi_perform(mh, &run);

        while (run > 0) {
            int32_t nf = 0;
            CURLMcode mc = curl_multi_wait(mh, NULL, 0, 1000, &nf);
            if (mc != CURLM_OK) break;
            curl_multi_perform(mh, &run);
        }

        for (size_t i = 0; i < nc; i++) {
            if (calls[i].h != NULL) {
                int64_t hc = 0;
                curl_easy_getinfo(calls[i].h, CURLINFO_RESPONSE_CODE, &hc);
                if (hc == 200) {
                    calls[i].ok = true;
                }
                curl_multi_remove_handle(mh, calls[i].h);
                curl_easy_cleanup(calls[i].h);
                calls[i].h = NULL;
            }
        }

        curl_multi_cleanup(mh);
    } else {
        nc = 1;
        calls = talloc_array(ctx, struct api_c, 1);
        CK(calls);

        char *url = NULL;
        if (al == 1) {
            yyjson_val *dv = yyjson_arr_get(av, 0);
            if (dv != NULL && yyjson_is_str(dv)) {
                const char *dom = yyjson_get_str(dv);
                char *ed = url_encode(ctx, dom);
                if (ed != NULL) {
                    url = talloc_asprintf(ctx, BASE "&siteSearch=%s&siteSearchFilter=i", ek, ee, eq, n, st, ed);
                }
            }
        } else if (bl == 1) {
            yyjson_val *dv = yyjson_arr_get(bv, 0);
            if (dv != NULL && yyjson_is_str(dv)) {
                const char *dom = yyjson_get_str(dv);
                char *ed = url_encode(ctx, dom);
                if (ed != NULL) {
                    url = talloc_asprintf(ctx, BASE "&siteSearch=%s&siteSearchFilter=e", ek, ee, eq, n, st, ed);
                }
            }
        } else {
            url = talloc_asprintf(ctx, BASE, ek, ee, eq, n, st);
        }
        CK(url);

        calls[0].u = url;
        calls[0].ok = false;
        calls[0].r.c = ctx;
        calls[0].r.d = talloc_array(ctx, char, 1);
        CK(calls[0].r.d);
        calls[0].r.d[0] = '\0';
        calls[0].r.s = 0;

        CURL *cu = curl_easy_init();
        if (cu == NULL) {
            output_error(ctx, "HTTP init failed", "NETWORK_ERROR");
            talloc_free(ctx);
            return 0;
        }

        curl_easy_setopt(cu, CURLOPT_URL, url);
        curl_easy_setopt(cu, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(cu, CURLOPT_WRITEDATA, (void *)&calls[0].r);
        curl_easy_setopt(cu, CURLOPT_TIMEOUT, 30L);

        CURLcode res = curl_easy_perform(cu);

        if (res != CURLE_OK) {
            curl_easy_cleanup(cu);
            char em[64];
            snprintf(em, 64, "HTTP: %s", curl_easy_strerror(res));
            output_error(ctx, em, "NETWORK_ERROR");
            talloc_free(ctx);
            return 0;
        }

        int64_t hc = 0;
        curl_easy_getinfo(cu, CURLINFO_RESPONSE_CODE, &hc);
        curl_easy_cleanup(cu);

        if (hc != 200) {
            yyjson_alc ra = ik_make_talloc_allocator(ctx);
            yyjson_doc *rd = yyjson_read_opts(calls[0].r.d, calls[0].r.s, 0, &ra, NULL);
            if (rd != NULL) {
                yyjson_val *rr = yyjson_doc_get_root(rd);
                yyjson_val *eo = yyjson_obj_get(rr, "error");
                if (eo != NULL) {
                    yyjson_val *mv = yyjson_obj_get(eo, "message");
                    const char *am = NULL;
                    if (mv != NULL && yyjson_is_str(mv)) {
                        am = yyjson_get_str(mv);
                    }

                    yyjson_val *ea = yyjson_obj_get(eo, "errors");
                    if (ea != NULL && yyjson_is_arr(ea)) {
                        yyjson_val *fe = yyjson_arr_get_first(ea);
                        if (fe != NULL) {
                            yyjson_val *rv = yyjson_obj_get(fe, "reason");
                            if (rv != NULL && yyjson_is_str(rv)) {
                                const char *rsn = yyjson_get_str(rv);
                                if (strcmp(rsn, "dailyLimitExceeded") == 0 || strcmp(rsn, "quotaExceeded") == 0) {
                                    output_error(ctx, "Quota exceeded (100/day)", "RATE_LIMIT");
                                    talloc_free(ctx);
                                    return 0;
                                }
                            }

                            if (am == NULL) {
                                yyjson_val *mv2 = yyjson_obj_get(fe, "message");
                                if (mv2 != NULL && yyjson_is_str(mv2)) {
                                    am = yyjson_get_str(mv2);
                                }
                            }
                        }
                    }

                    if (am != NULL) {
                        char em[64];
                        snprintf(em, 64, "API(%ld): %s", (long)hc, am);
                        output_error(ctx, em, "API_ERROR");
                        talloc_free(ctx);
                        return 0;
                    }
                }
            }

            char em[64];
            snprintf(em, 64, "API: %ld", (long)hc);
            output_error(ctx, em, "API_ERROR");
            talloc_free(ctx);
            return 0;
        }

        calls[0].ok = true;
    }

    yyjson_alc oa = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *od = yyjson_mut_doc_new(&oa);
    CK(od);
    yyjson_mut_val *obj = yyjson_mut_obj(od);
    CK(obj);
    yyjson_mut_val *succ = yyjson_mut_bool(od, true);
    CK(succ);
    yyjson_mut_val *results = yyjson_mut_arr(od);
    CK(results);

    if (al > 1) {
        yyjson_val **ia = talloc_array(ctx, yyjson_val *, (unsigned int)nc);
        CK(ia);
        size_t *ix = talloc_zero_array(ctx, size_t, (unsigned int)nc);
        CK(ix);
        size_t *s = talloc_zero_array(ctx, size_t, (unsigned int)nc);
        CK(s);

        for (size_t i = 0; i < nc; i++) {
            ia[i] = NULL;
            if (calls[i].ok) {
                yyjson_alc ra = ik_make_talloc_allocator(ctx);
                yyjson_doc *rd = yyjson_read_opts(calls[i].r.d, calls[i].r.s, 0, &ra, NULL);
                if (rd != NULL) {
                    yyjson_val *rr = yyjson_doc_get_root(rd);
                    yyjson_val *its = yyjson_obj_get(rr, "items");
                    if (its != NULL && yyjson_is_arr(its)) {
                        ia[i] = its;
                        s[i] = yyjson_arr_size(its);
                    }
                }
            }
        }

        int64_t c = 0;
        bool h = true;
        while (h && c < n) {
            h = false;
            for (size_t i = 0; i < nc; i++) {
                if (ia[i] != NULL && ix[i] < s[i]) {
                    yyjson_val *it = yyjson_arr_get(ia[i], ix[i]);
                    ix[i]++;

                    if (it != NULL) {
                        yyjson_val *tv = yyjson_obj_get(it, "title");
                        yyjson_val *lv = yyjson_obj_get(it, "link");
                        yyjson_val *svv = yyjson_obj_get(it, "snippet");

                        if (tv != NULL && yyjson_is_str(tv) && lv != NULL && yyjson_is_str(lv)) {
                            const char *t = yyjson_get_str(tv);
                            const char *l = yyjson_get_str(lv);

                            if (url_seen(results, l)) {
                                h = true;
                                continue;
                            }

                            const char *sn = "";
                            if (svv != NULL && yyjson_is_str(svv)) {
                                sn = yyjson_get_str(svv);
                            }

                            add_result(od, results, t, l, sn);
                            c++;

                            if (c >= n) {
                                break;
                            }
                        }
                    }

                    h = true;
                }
            }
        }
    } else {
        if (calls[0].ok) {
            yyjson_alc ra = ik_make_talloc_allocator(ctx);
            yyjson_doc *rd = yyjson_read_opts(calls[0].r.d, calls[0].r.s, 0, &ra, NULL);
            if (rd == NULL) {
                output_error(ctx, "Parse failed", "API_ERROR");
                talloc_free(ctx);
                return 0;
            }

            yyjson_val *rr = yyjson_doc_get_root(rd);
            yyjson_val *its = yyjson_obj_get(rr, "items");

            if (its != NULL && yyjson_is_arr(its)) {
                size_t i, m;
                yyjson_val *it;
                yyjson_arr_foreach(its, i, m, it) {
                    yyjson_val *tv = yyjson_obj_get(it, "title");
                    yyjson_val *lv = yyjson_obj_get(it, "link");
                    yyjson_val *svv = yyjson_obj_get(it, "snippet");

                    if (tv != NULL && yyjson_is_str(tv) && lv != NULL && yyjson_is_str(lv)) {
                        const char *t = yyjson_get_str(tv);
                        const char *l = yyjson_get_str(lv);
                        const char *sn = "";
                        if (svv != NULL && yyjson_is_str(svv)) {
                            sn = yyjson_get_str(svv);
                        }

                        if (bl > 1) {
                            bool blk = false;
                            for (size_t j = 0; j < bl; j++) {
                                yyjson_val *bdv = yyjson_arr_get(bv, j);
                                if (bdv != NULL && yyjson_is_str(bdv)) {
                                    const char *bd = yyjson_get_str(bdv);
                                    if (strstr(l, bd) != NULL) {
                                        blk = true;
                                        break;
                                    }
                                }
                            }
                            if (blk) {
                                continue;
                            }
                        }

                        add_result(od, results, t, l, sn);
                    }
                }
            }
        }
    }

    int64_t c = (int64_t)yyjson_mut_arr_size(results);
    yyjson_mut_val *cv = yyjson_mut_int(od, c);
    CK(cv);

    yyjson_mut_obj_add_val(od, obj, "success", succ);
    yyjson_mut_obj_add_val(od, obj, "results", results);
    yyjson_mut_obj_add_val(od, obj, "count", cv);

    yyjson_mut_doc_set_root(od, obj);
    char *js = yyjson_mut_write(od, 0, NULL);
    CK(js);
    printf("%s\n", js);
    free(js);

    talloc_free(ctx);
    return 0;
}
