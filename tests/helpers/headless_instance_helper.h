#ifndef IK_HEADLESS_INSTANCE_HELPER_H
#define IK_HEADLESS_INSTANCE_HELPER_H

#include <sys/types.h>

// Headless ikigai instance for functional testing
typedef struct {
    pid_t pid;            // Child process PID
    char *socket_path;    // Path to control socket
    char *runtime_dir;    // Runtime directory (for cleanup)
    char *db_name;        // Test database name (for cleanup)
} ik_headless_instance_t;

// Start ikigai in headless mode with an isolated test database
// db_name: unique database name for this test (will be created and migrated)
// Returns NULL on failure (timeout waiting for socket)
ik_headless_instance_t *ik_headless_start(const char *db_name);

// Stop headless ikigai instance
// Sends SIGTERM, waits for exit, removes socket, drops database, frees memory
void ik_headless_stop(ik_headless_instance_t *instance);

#endif // IK_HEADLESS_INSTANCE_HELPER_H
