/* LCOV_EXCL_START */
#include "dev_dump.h"

#ifdef IKIGAI_DEV

#include <stdio.h>
#include <string.h>

void ik_dev_dump_buffer(const char *path, const char *header, const char *buf, size_t len)
{
    // Try to open file - silently skip if directory doesn't exist or can't write
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return;
    }

    // Write header
    if (header != NULL && header[0] != '\0') {
        fwrite(header, 1, strlen(header), f);
    }

    // Write buffer
    if (buf != NULL && len > 0) {
        fwrite(buf, 1, len, f);
    }

    fclose(f);
}

#endif // IKIGAI_DEV
/* LCOV_EXCL_STOP */
