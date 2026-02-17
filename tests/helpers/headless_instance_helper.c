#include "tests/helpers/headless_instance_helper.h"
#include "tests/helpers/test_utils_helper.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SOCKET_WAIT_TIMEOUT_MS 5000
#define SOCKET_POLL_INTERVAL_US 50000

static char *find_socket_in_dir(const char *dir, pid_t pid)
{
    DIR *d = opendir(dir);
    if (d == NULL) {
        return NULL;
    }

    char expected_prefix[64];
    int n = snprintf(expected_prefix, sizeof(expected_prefix),
                     "ikigai-%d.sock", (int)pid);
    if (n < 0 || (size_t)n >= sizeof(expected_prefix)) {
        closedir(d);
        return NULL;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, expected_prefix) == 0) {
            size_t dir_len = strlen(dir);
            size_t name_len = strlen(entry->d_name);
            char *path = malloc(dir_len + 1 + name_len + 1);
            if (path == NULL) {
                closedir(d);
                return NULL;
            }
            memcpy(path, dir, dir_len);
            path[dir_len] = '/';
            memcpy(path + dir_len + 1, entry->d_name, name_len + 1);
            closedir(d);
            return path;
        }
    }

    closedir(d);
    return NULL;
}

ik_headless_instance_t *ik_headless_start(const char *db_name)
{
    if (db_name == NULL) {
        return NULL;
    }

    // Create and migrate test database
    res_t result = ik_test_db_create(db_name);
    if (is_err(&result)) {
        return NULL;
    }
    result = ik_test_db_migrate(NULL, db_name);
    if (is_err(&result)) {
        ik_test_db_destroy(db_name);
        return NULL;
    }

    // Set up isolated runtime dir for this test process
    char runtime_dir[256];
    int n = snprintf(runtime_dir, sizeof(runtime_dir),
                     "/tmp/ikigai_functional_test_%d", (int)getpid());
    if (n < 0 || (size_t)n >= sizeof(runtime_dir)) {
        ik_test_db_destroy(db_name);
        return NULL;
    }

    mkdir(runtime_dir, 0700);
    setenv("IKIGAI_RUNTIME_DIR", runtime_dir, 1);
    setenv("IKIGAI_DB_NAME", db_name, 1);

    pid_t pid = fork();
    if (pid < 0) {
        ik_test_db_destroy(db_name);
        return NULL;
    }

    if (pid == 0) {
        // Child process - exec ikigai --headless
        execlp("bin/ikigai", "bin/ikigai", "--headless", (char *)NULL);
        _exit(127);
    }

    // Parent - wait for control socket to appear
    int elapsed_ms = 0;
    char *socket_path = NULL;

    while (elapsed_ms < SOCKET_WAIT_TIMEOUT_MS) {
        socket_path = find_socket_in_dir(runtime_dir, pid);
        if (socket_path != NULL) {
            break;
        }

        // Check if child has already exited
        int status;
        pid_t wait_result = waitpid(pid, &status, WNOHANG);
        if (wait_result == pid) {
            ik_test_db_destroy(db_name);
            return NULL;
        }

        usleep(SOCKET_POLL_INTERVAL_US);
        elapsed_ms += SOCKET_POLL_INTERVAL_US / 1000;
    }

    if (socket_path == NULL) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        ik_test_db_destroy(db_name);
        return NULL;
    }

    ik_headless_instance_t *instance = malloc(sizeof(ik_headless_instance_t));
    if (instance == NULL) {
        free(socket_path);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        ik_test_db_destroy(db_name);
        return NULL;
    }

    instance->pid = pid;
    instance->socket_path = socket_path;
    instance->runtime_dir = strdup(runtime_dir);
    instance->db_name = strdup(db_name);

    return instance;
}

void ik_headless_stop(ik_headless_instance_t *instance)
{
    if (instance == NULL) {
        return;
    }

    // Send SIGTERM and wait for process to exit
    if (instance->pid > 0) {
        kill(instance->pid, SIGTERM);

        int status;
        pid_t result = waitpid(instance->pid, &status, 0);
        if (result < 0 && errno == ECHILD) {
            // Already reaped
        }
    }

    // Clean up socket file
    if (instance->socket_path != NULL) {
        unlink(instance->socket_path);
        free(instance->socket_path);
    }

    // Clean up runtime directory
    if (instance->runtime_dir != NULL) {
        rmdir(instance->runtime_dir);
        free(instance->runtime_dir);
    }

    // Drop test database
    if (instance->db_name != NULL) {
        ik_test_db_destroy(instance->db_name);
        free(instance->db_name);
    }

    free(instance);
}
