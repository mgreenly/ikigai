#ifndef TERMINAL_PTY_HELPERS_H
#define TERMINAL_PTY_HELPERS_H

#include <pthread.h>

// PTY helper structure
typedef struct {
    int master_fd;
    int slave_fd;
    char slave_name[256];
} pty_pair_t;

// Terminal simulator thread configuration
typedef struct {
    int master_fd;
    const char *probe_response;      // Response to CSI u probe query (NULL = no response/timeout)
    const char *enable_response;     // Response to CSI u enable command (NULL = no response)
    int probe_delay_ms;              // Delay before sending probe response
    int enable_delay_ms;             // Delay before sending enable response
    volatile int done;               // Signal to exit
} term_sim_config_t;

// Function prototypes
int create_pty_pair(pty_pair_t *pty);
void close_pty_pair(pty_pair_t *pty);
int pty_set_size(pty_pair_t *pty, int rows, int cols);
void *term_simulator_thread(void *arg);

#endif // TERMINAL_PTY_HELPERS_H