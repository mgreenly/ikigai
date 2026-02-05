// yyjson wrapper implementations
// Link seams that tests can override for failure injection
//
// In release builds (NDEBUG), these are defined as static inline in the header.
// In debug/test builds, these are compiled as weak symbols.

#include "shared/wrapper_json.h"


#include "shared/poison.h"
#ifndef NDEBUG
// LCOV_EXCL_START

// ============================================================================
// yyjson wrappers - debug/test builds only
// ============================================================================

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

MOCKABLE bool yyjson_mut_obj_add_str_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                      const char *key, const char *val)
{
    return yyjson_mut_obj_add_str(doc, obj, key, val);
}

MOCKABLE bool yyjson_mut_obj_add_int_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                      const char *key, int64_t val)
{
    return yyjson_mut_obj_add_int(doc, obj, key, val);
}

MOCKABLE bool yyjson_mut_obj_add_uint_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                       const char *key, uint64_t val)
{
    return yyjson_mut_obj_add_uint(doc, obj, key, val);
}

MOCKABLE yyjson_mut_val *yyjson_mut_obj_(yyjson_mut_doc *doc)
{
    return yyjson_mut_obj(doc);
}

MOCKABLE yyjson_mut_val *yyjson_mut_arr_(yyjson_mut_doc *doc)
{
    return yyjson_mut_arr(doc);
}

MOCKABLE bool yyjson_mut_obj_add_val_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                      const char *key, yyjson_mut_val *val)
{
    return yyjson_mut_obj_add_val(doc, obj, key, val);
}

MOCKABLE bool yyjson_mut_arr_add_val_(yyjson_mut_val *arr, yyjson_mut_val *val)
{
    return yyjson_mut_arr_add_val(arr, val);
}

MOCKABLE bool yyjson_mut_obj_add_bool_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                       const char *key, bool val)
{
    return yyjson_mut_obj_add_bool(doc, obj, key, val);
}

MOCKABLE yyjson_mut_val *yyjson_val_mut_copy_(yyjson_mut_doc *doc, yyjson_val *val)
{
    return yyjson_val_mut_copy(doc, val);
}

// LCOV_EXCL_STOP
#endif
