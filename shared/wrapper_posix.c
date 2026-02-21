// POSIX wrapper implementations
// Link seams that tests can override for failure injection
//
// In release builds (NDEBUG), these are defined as static inline in the header.
// In debug/test builds, these are compiled as weak symbols.

#include "shared/wrapper_posix.h"

#ifndef NDEBUG
// LCOV_EXCL_START

#include <ctype.h>
#include <fcntl.h>
#include <glob.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>


#include "shared/poison.h"
// ============================================================================
// Debug helpers
// ============================================================================

#ifdef DEBUG
// Format buffer as hex string with escape sequences visible
// Returns static buffer - not thread safe, only for DEBUG_LOG
// Currently unused but kept for debugging terminal I/O issues
__attribute__((unused))
static const char *format_bytes(const void *buf, size_t count)
{
    static char result[512];
    const unsigned char *bytes = buf;
    size_t out_pos = 0;

    // Limit output to avoid buffer overflow
    size_t max_bytes = (sizeof(result) - 10) / 4;  // Reserve space for "..."
    if (count > max_bytes) count = max_bytes;

    for (size_t i = 0; i < count && out_pos < sizeof(result) - 10; i++) {
        unsigned char c = bytes[i];
        int written;

        if (c == '\x1b') {
            // ESC as \e for readability
            written = snprintf(result + out_pos, sizeof(result) - out_pos, "\\e");
            out_pos += (size_t)written;
        } else if (c == '\r') {
            written = snprintf(result + out_pos, sizeof(result) - out_pos, "\\r");
            out_pos += (size_t)written;
        } else if (c == '\n') {
            written = snprintf(result + out_pos, sizeof(result) - out_pos, "\\n");
            out_pos += (size_t)written;
        } else if (c == '\t') {
            written = snprintf(result + out_pos, sizeof(result) - out_pos, "\\t");
            out_pos += (size_t)written;
        } else if (isprint((int)c)) {
            result[out_pos++] = (char)c;
        } else {
            written = snprintf(result + out_pos, sizeof(result) - out_pos, "\\x%02x", c);
            out_pos += (size_t)written;
        }
    }

    result[out_pos] = '\0';
    return result;
}

#endif

// ============================================================================
// POSIX system call wrappers - debug/test builds only
// ============================================================================

MOCKABLE int posix_socket_(int domain, int type, int protocol)
{
    return socket(domain, type, protocol);
}

MOCKABLE int posix_bind_(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return bind(sockfd, addr, addrlen);
}

MOCKABLE int posix_listen_(int sockfd, int backlog)
{
    return listen(sockfd, backlog);
}

MOCKABLE int posix_open_(const char *pathname, int flags)
{
    return open(pathname, flags);
}

MOCKABLE int posix_close_(int fd)
{
    return close(fd);
}

MOCKABLE int posix_stat_(const char *pathname, struct stat *statbuf)
{
    return stat(pathname, statbuf);
}

MOCKABLE int posix_mkdir_(const char *pathname, mode_t mode)
{
    return mkdir(pathname, mode);
}

MOCKABLE int posix_tcgetattr_(int fd, struct termios *termios_p)
{
    return tcgetattr(fd, termios_p);
}

MOCKABLE int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p)
{
    return tcsetattr(fd, optional_actions, termios_p);
}

MOCKABLE int posix_tcflush_(int fd, int queue_selector)
{
    return tcflush(fd, queue_selector);
}

MOCKABLE int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    return ioctl(fd, request, argp);
}

MOCKABLE ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    // DEBUG_LOG("WRITE fd=%d count=%zu: %s", fd, count, format_bytes(buf, count));
    ssize_t result = write(fd, buf, count);
    // DEBUG_LOG("WRITE fd=%d result=%zd", fd, result);
    return result;
}

MOCKABLE ssize_t posix_send_(int sockfd, const void *buf, size_t len, int flags)
{
    return send(sockfd, buf, len, flags);
}

MOCKABLE ssize_t posix_read_(int fd, void *buf, size_t count)
{
    // DEBUG_LOG("READ  fd=%d count=%zu", fd, count);
    ssize_t result = read(fd, buf, count);
    // if (result > 0) {
    //     DEBUG_LOG("READ  fd=%d result=%zd: %s", fd, result, format_bytes(buf, (size_t)result));
    // } else {
    //     DEBUG_LOG("READ  fd=%d result=%zd", fd, result);
    // }
    return result;
}

MOCKABLE int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    return select(nfds, readfds, writefds, exceptfds, timeout);
}

MOCKABLE int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    return sigaction(signum, act, oldact);
}

MOCKABLE int posix_pipe_(int pipefd[2])
{
    return pipe(pipefd);
}

MOCKABLE int posix_fcntl_(int fd, int cmd, int arg)
{
    return fcntl(fd, cmd, arg);
}

MOCKABLE FILE *posix_fdopen_(int fd, const char *mode)
{
    return fdopen(fd, mode);
}

MOCKABLE size_t fread_(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fread(ptr, size, nmemb, stream);
}

MOCKABLE size_t fwrite_(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

MOCKABLE FILE *fopen_(const char *pathname, const char *mode)
{
    return fopen(pathname, mode);
}

MOCKABLE int fseek_(FILE *stream, long offset, int whence)
{
    return fseek(stream, offset, whence);
}

MOCKABLE long ftell_(FILE *stream)
{
    return ftell(stream);
}

MOCKABLE int fclose_(FILE *stream)
{
    return fclose(stream);
}

MOCKABLE FILE *popen_(const char *command, const char *mode)
{
    return popen(command, mode);
}

MOCKABLE int pclose_(FILE *stream)
{
    return pclose(stream);
}

MOCKABLE DIR *opendir_(const char *name)
{
    return opendir(name);
}

MOCKABLE int posix_access_(const char *pathname, int mode)
{
    return access(pathname, mode);
}

MOCKABLE int posix_rename_(const char *oldpath, const char *newpath)
{
    return rename(oldpath, newpath);
}

MOCKABLE char *posix_getcwd_(char *buf, size_t size)
{
    return getcwd(buf, size);
}

MOCKABLE char *getenv_(const char *name)
{
    return getenv(name);
}

MOCKABLE struct passwd *getpwuid_(uid_t uid)
{
    return getpwuid(uid);
}

MOCKABLE int glob_(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob)
{
    return glob(pattern, flags, errfunc, pglob);
}

MOCKABLE void globfree_(glob_t *pglob)
{
    globfree(pglob);
}

MOCKABLE int kill_(pid_t pid, int sig)
{
    return kill(pid, sig);
}

MOCKABLE pid_t waitpid_(pid_t pid, int *status, int options)
{
    return waitpid(pid, status, options);
}

MOCKABLE int usleep_(useconds_t usec)
{
    return usleep(usec);
}

// LCOV_EXCL_STOP
#endif
