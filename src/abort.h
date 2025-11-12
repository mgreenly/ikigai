#ifndef IK_ABORT_H
#define IK_ABORT_H

#include <stdio.h>
#include <stdlib.h>

// Abort with formatted message, file, and line
// Use for unrecoverable logic errors that indicate corruption
// or impossible states. Should be used sparingly (~1-2 per 1000 LOC).
#define FATAL(msg) \
    do { \
        fprintf(stderr, "FATAL: %s\n  at %s:%d\n", \
                (msg), __FILE__, __LINE__); \
        fflush(stderr); \
        abort(); \
    } while(0)

#endif
