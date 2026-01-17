#include "tool_discovery.h"

#include "debug_log.h"
#include "error.h"
#include "json_allocator.h"
#include "panic.h"
#include "tool_registry.h"

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <talloc.h>
#include <unistd.h>

// Check if file is executable
static bool is_executable(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {  // LCOV_EXCL_BR_LINE
        return false;  // LCOV_EXCL_LINE
    }
    return (st.st_mode & S_IXUSR) != 0;
}

// Spawn tool with --schema flag and read JSON output
// Returns NULL on failure (timeout, crash, invalid JSON)
static yyjson_doc *call_tool_schema(TALLOC_CTX *ctx, const char *tool_path)
{
    int32_t pipefd[2];
    if (pipe(pipefd) == -1) {  // LCOV_EXCL_BR_LINE
        return NULL;  // LCOV_EXCL_LINE
    }

    pid_t pid = fork();
    if (pid == -1) {  // LCOV_EXCL_BR_LINE
        close(pipefd[0]);  // LCOV_EXCL_LINE
        close(pipefd[1]);  // LCOV_EXCL_LINE
        return NULL;  // LCOV_EXCL_LINE
    }

    if (pid == 0) {  // LCOV_EXCL_BR_LINE
        // Child process - LCOV_EXCL_START
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        // Redirect stderr to /dev/null
        int32_t null_fd = open("/dev/null", O_WRONLY);
        if (null_fd != -1) {
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }

        execl(tool_path, tool_path, "--schema", NULL);
        _exit(1);
        // LCOV_EXCL_STOP
    }

    // Parent process
    close(pipefd[1]);

    // Set 1 second timeout using alarm
    alarm(1);

    // Read output
    char buffer[8192];
    ssize_t total_read = 0;
    ssize_t n;

    while ((n = read(pipefd[0], buffer + total_read, sizeof(buffer) - (size_t)total_read - 1)) > 0) {
        total_read += n;
        if (total_read >= (ssize_t)(sizeof(buffer) - 1)) {
            break;
        }
    }

    alarm(0);
    close(pipefd[0]);

    // Wait for child
    int32_t status;
    waitpid(pid, &status, 0);

    if (total_read == 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {  // LCOV_EXCL_BR_LINE
        return NULL;
    }

    buffer[total_read] = '\0';

    // Parse JSON
    yyjson_read_err err;
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(buffer, (size_t)total_read, 0, &allocator, &err);

    return doc;
}

// Extract tool name from path (last component, strip -tool suffix, convert hyphens to underscores)
// e.g., "/path/to/bash-tool" -> "bash"
// e.g., "/path/to/file-read-tool" -> "file_read"
// Precondition: path must end with "-tool" (enforced by scan_directory caller)
static char *extract_tool_name(TALLOC_CTX *ctx, const char *path)
{
    const char *basename = strrchr(path, '/');
    if (basename == NULL) {  // LCOV_EXCL_BR_LINE - path always contains '/' from scan_directory
        basename = path;  // LCOV_EXCL_LINE
    } else {
        basename++;
    }

    // Strip "-tool" suffix (caller guarantees this exists)
    size_t len = strlen(basename);
    const char *suffix = "-tool";
    size_t suffix_len = strlen(suffix);

    // Precondition check - basename must end with "-tool"
    if (len <= suffix_len || strcmp(basename + len - suffix_len, suffix) != 0) {  // LCOV_EXCL_BR_LINE
        PANIC("extract_tool_name called with path not ending in -tool");  // LCOV_EXCL_LINE
    }

    char *name = talloc_strndup(ctx, basename, len - suffix_len);
    if (name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Convert hyphens to underscores
    for (size_t i = 0; i < strlen(name); i++) {
        if (name[i] == '-') {
            name[i] = '_';
        }
    }

    return name;
}

// Scan a single directory and add tools to registry
// Returns OK even if directory doesn't exist or is empty
static res_t scan_directory(TALLOC_CTX *ctx, const char *dir_path, ik_tool_registry_t *registry)
{
    DEBUG_LOG("scan_directory: scanning %s", dir_path);
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        // Directory doesn't exist - this is OK
        DEBUG_LOG("scan_directory: %s does not exist or cannot be opened", dir_path);
        return OK(NULL);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full path
        char *full_path = talloc_asprintf(ctx, "%s/%s", dir_path, entry->d_name);
        if (full_path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        DEBUG_LOG("scan_directory: checking %s", full_path);

        // Check if executable
        if (!is_executable(full_path)) {
            DEBUG_LOG("scan_directory: %s is not executable, skipping", full_path);
            talloc_free(full_path);
            continue;
        }

        // Only consider files ending in "-tool"
        const char *suffix = "-tool";
        size_t name_len = strlen(entry->d_name);
        size_t suffix_len = strlen(suffix);
        if (name_len <= suffix_len || strcmp(entry->d_name + name_len - suffix_len, suffix) != 0) {
            DEBUG_LOG("scan_directory: %s does not end in '-tool', skipping", full_path);
            talloc_free(full_path);
            continue;
        }

        DEBUG_LOG("scan_directory: calling %s --schema", full_path);
        // Call tool with --schema
        yyjson_doc *schema_doc = call_tool_schema(ctx, full_path);
        if (schema_doc == NULL) {
            // Failed to get schema - skip this tool
            talloc_free(full_path);
            continue;
        }

        // Extract tool name from filename
        char *tool_name = extract_tool_name(ctx, full_path);
        if (tool_name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        // Add to registry
        res_t result = ik_tool_registry_add(registry, tool_name, full_path, schema_doc);
        talloc_free(tool_name);
        talloc_free(full_path);

        if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - OOM or corruption
            closedir(dir);  // LCOV_EXCL_LINE
            return result;  // LCOV_EXCL_LINE
        }
    }

    closedir(dir);
    return OK(NULL);
}

res_t ik_tool_discovery_run(TALLOC_CTX *ctx,
                            const char *system_dir,
                            const char *user_dir,
                            const char *project_dir,
                            ik_tool_registry_t *registry)
{
    DEBUG_LOG("ik_tool_discovery_run: system_dir=%s, user_dir=%s, project_dir=%s",
              system_dir, user_dir, project_dir);

    // Scan all three directories in order: system, user, project
    // Override precedence: project > user > system (later scans override earlier)

    // Scan system directory
    res_t result = scan_directory(ctx, system_dir, registry);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - OOM or corruption
        return result;  // LCOV_EXCL_LINE
    }

    // Scan user directory
    result = scan_directory(ctx, user_dir, registry);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - OOM or corruption
        return result;  // LCOV_EXCL_LINE
    }

    // Scan project directory
    result = scan_directory(ctx, project_dir, registry);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - OOM or corruption
        return result;  // LCOV_EXCL_LINE
    }

    // Sort registry entries alphabetically by name
    ik_tool_registry_sort(registry);

    return OK(NULL);
}
