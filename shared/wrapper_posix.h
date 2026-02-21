// POSIX system call and stdio wrappers for testing
#ifndef IK_WRAPPER_POSIX_H
#define IK_WRAPPER_POSIX_H

#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "shared/wrapper_base.h"

#ifdef NDEBUG

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
    return write(fd, buf, count);
}

MOCKABLE ssize_t posix_send_(int sockfd, const void *buf, size_t len, int flags)
{
    return send(sockfd, buf, len, flags);
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

#else

MOCKABLE int posix_socket_(int domain, int type, int protocol);
MOCKABLE int posix_bind_(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
MOCKABLE int posix_listen_(int sockfd, int backlog);
MOCKABLE int posix_open_(const char *pathname, int flags);
MOCKABLE int posix_close_(int fd);
MOCKABLE int posix_stat_(const char *pathname, struct stat *statbuf);
MOCKABLE int posix_mkdir_(const char *pathname, mode_t mode);
MOCKABLE int posix_tcgetattr_(int fd, struct termios *termios_p);
MOCKABLE int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
MOCKABLE int posix_tcflush_(int fd, int queue_selector);
MOCKABLE int posix_ioctl_(int fd, unsigned long request, void *argp);
MOCKABLE ssize_t posix_write_(int fd, const void *buf, size_t count);
MOCKABLE ssize_t posix_send_(int sockfd, const void *buf, size_t len, int flags);
MOCKABLE ssize_t posix_read_(int fd, void *buf, size_t count);
MOCKABLE int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
MOCKABLE int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact);
MOCKABLE int posix_pipe_(int pipefd[2]);
MOCKABLE int posix_fcntl_(int fd, int cmd, int arg);
MOCKABLE FILE *posix_fdopen_(int fd, const char *mode);
MOCKABLE size_t fread_(void *ptr, size_t size, size_t nmemb, FILE *stream);
MOCKABLE size_t fwrite_(const void *ptr, size_t size, size_t nmemb, FILE *stream);
MOCKABLE FILE *fopen_(const char *pathname, const char *mode);
MOCKABLE int fseek_(FILE *stream, long offset, int whence);
MOCKABLE long ftell_(FILE *stream);
MOCKABLE int fclose_(FILE *stream);
MOCKABLE FILE *popen_(const char *command, const char *mode);
MOCKABLE int pclose_(FILE *stream);
MOCKABLE DIR *opendir_(const char *name);
MOCKABLE int posix_access_(const char *pathname, int mode);
MOCKABLE int posix_rename_(const char *oldpath, const char *newpath);
MOCKABLE char *posix_getcwd_(char *buf, size_t size);
MOCKABLE char *getenv_(const char *name);
MOCKABLE struct passwd *getpwuid_(uid_t uid);
MOCKABLE int glob_(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob);
MOCKABLE void globfree_(glob_t *pglob);
MOCKABLE int kill_(pid_t pid, int sig);
MOCKABLE pid_t waitpid_(pid_t pid, int *status, int options);
MOCKABLE int usleep_(useconds_t usec);

#endif

#endif // IK_WRAPPER_POSIX_H
