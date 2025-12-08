// External library wrappers for testing
// These provide link seams that tests can override to inject failures
//
// MOCKABLE functions are:
//   - weak symbols in debug/test builds (can be overridden)
//   - always_inline in release builds (zero overhead)

#ifndef IK_WRAPPER_H
#define IK_WRAPPER_H

#include "wrapper_base.h"
#include "wrapper_talloc.h"
#include "wrapper_json.h"
#include "wrapper_curl.h"
#include "wrapper_postgres.h"
#include "wrapper_pthread.h"
#include "wrapper_posix.h"
#include "wrapper_stdlib.h"
#include "wrapper_internal.h"

#endif // IK_WRAPPER_H
