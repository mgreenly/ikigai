// POSIX wrapper implementations
// Link seams that tests can override for failure injection
//
// In release builds (NDEBUG), these are defined as static inline in the header.
// In debug/test builds, these are compiled as weak symbols.

#include "wrapper_posix.h"

#ifndef NDEBUG
// LCOV_EXCL_START

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

// ============================================================================
// POSIX system call wrappers - debug/test builds only
// ============================================================================

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
    return write(fd, buf, count);
}

MOCKABLE ssize_t posix_read_(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
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

// LCOV_EXCL_STOP
#endif
