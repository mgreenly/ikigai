#include "terminal_pty_helper.h"

#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// Create a PTY pair for testing
// Returns 0 on success, -1 on failure
int create_pty_pair(pty_pair_t *pty)
{
    memset(pty, 0, sizeof(*pty));

    // Use openpty() which handles all the setup for us
    if (openpty(&pty->master_fd, &pty->slave_fd, pty->slave_name, NULL, NULL) < 0) {
        return -1;
    }

    // Set master to non-blocking for easier testing
    int flags = fcntl(pty->master_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(pty->master_fd, F_SETFL, flags | O_NONBLOCK);
    }

    return 0;
}

// Close PTY pair
void close_pty_pair(pty_pair_t *pty)
{
    if (pty->master_fd >= 0) {
        close(pty->master_fd);
        pty->master_fd = -1;
    }
    if (pty->slave_fd >= 0) {
        close(pty->slave_fd);
        pty->slave_fd = -1;
    }
}

// Set terminal size on the PTY slave
int pty_set_size(pty_pair_t *pty, int rows, int cols)
{
    struct winsize ws = {
        .ws_row = (unsigned short)rows,
        .ws_col = (unsigned short)cols,
        .ws_xpixel = 0,
        .ws_ypixel = 0
    };
    return ioctl(pty->slave_fd, TIOCSWINSZ, &ws);
}

// Terminal simulator thread function
// Reads from master fd, sends configured responses
void *term_simulator_thread(void *arg)
{
    term_sim_config_t *cfg = (term_sim_config_t *)arg;
    char buf[256];
    int stage = 0;  // 0 = waiting for probe, 1 = waiting for enable

    while (!atomic_load(&cfg->done)) {
        struct pollfd pfd = { .fd = cfg->master_fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 10);  // 10ms poll

        if (ret <= 0) continue;
        if (!(pfd.revents & POLLIN)) continue;

        ssize_t n = read(cfg->master_fd, buf, sizeof(buf) - 1);
        if (n <= 0) continue;
        buf[n] = '\0';

        // Check what we received and respond accordingly
        // Stage 0: Looking for CSI u probe query (ESC[?u)
        if (stage == 0 && strstr(buf, "\x1b[?u") != NULL) {
            if (cfg->probe_response) {
                if (cfg->probe_delay_ms > 0) {
                    usleep((useconds_t)(cfg->probe_delay_ms * 1000));
                }
                (void)write(cfg->master_fd, cfg->probe_response, strlen(cfg->probe_response));
            }
            stage = 1;
        }
        // Stage 1: Looking for CSI u enable command (ESC[>9u)
        else if (stage == 1 && strstr(buf, "\x1b[>9u") != NULL) {
            if (cfg->enable_response) {
                if (cfg->enable_delay_ms > 0) {
                    usleep((useconds_t)(cfg->enable_delay_ms * 1000));
                }
                (void)write(cfg->master_fd, cfg->enable_response, strlen(cfg->enable_response));
            }
            stage = 2;
        }
    }

    return NULL;
}
