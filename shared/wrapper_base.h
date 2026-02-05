// Base definitions for wrapper system
// MOCKABLE functions are:
//   - weak symbols in debug/test builds (can be overridden)
//   - always_inline in release builds (zero overhead)

#ifndef IK_WRAPPER_BASE_H
#define IK_WRAPPER_BASE_H

// MOCKABLE: weak (debug) or inline (release)
#ifdef NDEBUG
#define MOCKABLE static inline
#else
#define MOCKABLE __attribute__((weak))
#endif

#endif // IK_WRAPPER_BASE_H
