// External library wrappers for testing
// These provide link seams that tests can override to inject failures
//
// MOCKABLE functions are:
//   - weak symbols in debug/test builds (can be overridden)
//   - always_inline in release builds (zero overhead)

#ifndef IK_WRAPPER_H
#define IK_WRAPPER_H

#include "shared/wrapper_base.h"
#include "shared/wrapper_talloc.h"
#include "shared/wrapper_json.h"
#include "shared/wrapper_curl.h"
#include "shared/wrapper_posix.h"
#include "shared/wrapper_stdlib.h"
#include "shared/wrapper_internal.h"

#endif // IK_WRAPPER_H
