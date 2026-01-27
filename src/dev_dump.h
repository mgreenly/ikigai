#ifndef IK_DEV_DUMP_H
#define IK_DEV_DUMP_H

#ifdef IKIGAI_DEV

#include <inttypes.h>
#include <stddef.h>

// Dump buffer to file with header metadata
// path: file path (e.g., ".ikigai/debug/repl_viewport.framebuffer")
// header: formatted header string (e.g., "# rows=24 cols=80 cursor=10,5 len=1234\n")
// buf: buffer to dump
// len: length of buffer
// Silently skips if parent directory does not exist
void ik_dev_dump_buffer(const char *path, const char *header, const char *buf, size_t len);

// Macro wrapper for conditional compilation
#define DEV_DUMP_BUFFER(path, header, buf, len) \
        ik_dev_dump_buffer(path, header, buf, len)

#else

// In non-IKIGAI_DEV builds, everything compiles away to nothing
#define DEV_DUMP_BUFFER(path, header, buf, len) ((void)0)

#endif // IKIGAI_DEV

#endif // IK_DEV_DUMP_H
