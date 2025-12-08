// yyjson wrappers for testing
#ifndef IK_WRAPPER_JSON_H
#define IK_WRAPPER_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include "vendor/yyjson/yyjson.h"
#include "wrapper_base.h"

#ifdef NDEBUG
MOCKABLE yyjson_doc *yyjson_read_file_(const char *path, yyjson_read_flag flg,
                                       const yyjson_alc *allocator, yyjson_read_err *err)
{
    return yyjson_read_file(path, flg, allocator, err);
}

MOCKABLE bool yyjson_mut_write_file_(const char *path, const yyjson_mut_doc *doc,
                                     yyjson_write_flag flg, const yyjson_alc *allocator,
                                     yyjson_write_err *err)
{
    return yyjson_mut_write_file(path, doc, flg, allocator, err);
}

MOCKABLE yyjson_doc *yyjson_read_(const char *dat, size_t len, yyjson_read_flag flg)
{
    return yyjson_read(dat, len, flg);
}

MOCKABLE yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc)
{
    return yyjson_doc_get_root(doc);
}

MOCKABLE yyjson_val *yyjson_obj_get_(yyjson_val *obj, const char *key)
{
    return yyjson_obj_get(obj, key);
}

MOCKABLE int64_t yyjson_get_sint_(yyjson_val *val)
{
    return yyjson_get_sint(val);
}

MOCKABLE const char *yyjson_get_str_(yyjson_val *val)
{
    return yyjson_get_str(val);
}

#else
MOCKABLE yyjson_doc *yyjson_read_file_(const char *path,
                                       yyjson_read_flag flg,
                                       const yyjson_alc *allocator,
                                       yyjson_read_err *err);
MOCKABLE bool yyjson_mut_write_file_(const char *path,
                                     const yyjson_mut_doc *doc,
                                     yyjson_write_flag flg,
                                     const yyjson_alc *allocator,
                                     yyjson_write_err *err);
MOCKABLE yyjson_doc *yyjson_read_(const char *dat, size_t len, yyjson_read_flag flg);
MOCKABLE yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc);
MOCKABLE yyjson_val *yyjson_obj_get_(yyjson_val *obj, const char *key);
MOCKABLE int64_t yyjson_get_sint_(yyjson_val *val);
MOCKABLE const char *yyjson_get_str_(yyjson_val *val);
#endif

#endif // IK_WRAPPER_JSON_H
