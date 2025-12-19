#include "history.h"
#include "file_utils.h"
#include "json_allocator.h"
#include "logger.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include "wrapper.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <time.h>
#include <unistd.h>

ik_history_t *ik_history_create(void *ctx, size_t capacity)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(capacity > 0);      // LCOV_EXCL_BR_LINE

    ik_history_t *hist = talloc_zero(ctx, ik_history_t);
    if (hist == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    hist->capacity = capacity;
    hist->count = 0;
    hist->index = 0;  // Not browsing initially (index == count == 0)
    hist->pending = NULL;

    // Allocate entries array
    hist->entries = talloc_array(hist, char *, (unsigned int)capacity);
    if (hist->entries == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return hist;
}

res_t ik_history_add(ik_history_t *hist, const char *entry)
{
    assert(hist != NULL);      // LCOV_EXCL_BR_LINE
    assert(entry != NULL);     // LCOV_EXCL_BR_LINE

    if (entry[0] == '\0') {
        return OK(NULL);
    }

    // Skip if identical to most recent entry
    if (hist->count > 0 && strcmp(hist->entries[hist->count - 1], entry) == 0) {
        hist->index = hist->count;
        if (hist->pending != NULL) {
            talloc_free(hist->pending);
            hist->pending = NULL;
        }
        return OK(NULL);
    }

    // Check if entry exists - move to end if found
    int existing_index = -1;
    for (size_t i = 0; i < hist->count; i++) {
        if (strcmp(hist->entries[i], entry) == 0) {
            existing_index = (int)i;
            break;
        }
    }

    if (existing_index >= 0) {
        talloc_free(hist->entries[existing_index]);
        for (size_t i = (size_t)existing_index; i < hist->count - 1; i++) {
            hist->entries[i] = hist->entries[i + 1];
        }
        hist->count--;
    } else if (hist->count == hist->capacity) {
        talloc_free(hist->entries[0]);
        for (size_t i = 0; i < hist->count - 1; i++) {
            hist->entries[i] = hist->entries[i + 1];
        }
        hist->count--;
    }

    hist->entries[hist->count] = talloc_strdup(hist, entry);
    if (hist->entries[hist->count] == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    hist->count++;

    hist->index = hist->count;
    if (hist->pending != NULL) {
        talloc_free(hist->pending);
        hist->pending = NULL;
    }

    return OK(NULL);
}

res_t ik_history_start_browsing(ik_history_t *hist, const char *pending_input)
{
    assert(hist != NULL);           // LCOV_EXCL_BR_LINE
    assert(pending_input != NULL);  // LCOV_EXCL_BR_LINE

    // If history is empty, just save pending and stay in non-browsing state
    if (hist->count == 0) {
        if (hist->pending != NULL) {
            talloc_free(hist->pending);
        }
        hist->pending = talloc_strdup(hist, pending_input);
        if (hist->pending == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        hist->index = 0;  // Still not browsing (count is 0)
        return OK(NULL);
    }

    // Free old pending if it exists
    if (hist->pending != NULL) {
        talloc_free(hist->pending);
    }

    // Save pending input
    hist->pending = talloc_strdup(hist, pending_input);
    if (hist->pending == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Move to last entry
    hist->index = hist->count - 1;

    return OK(NULL);
}

const char *ik_history_prev(ik_history_t *hist)
{
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    // If not browsing or at beginning, return NULL
    if (!ik_history_is_browsing(hist) || hist->index == 0) {
        return NULL;
    }

    // Move to previous entry
    hist->index--;

    return hist->entries[hist->index];
}

const char *ik_history_next(ik_history_t *hist)
{
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    // If we're past the last entry, return NULL
    if (hist->index > hist->count) {
        return NULL;
    }

    // If we're exactly at pending position, increment and return NULL
    // (user has already seen pending, trying to go past it)
    if (hist->index == hist->count) {
        hist->index++;
        return NULL;
    }

    // Move forward in history
    hist->index++;

    // If we're still in history entries, return the entry
    if (hist->index < hist->count) {
        return hist->entries[hist->index];
    }

    // We just moved to pending position - return pending
    // Don't increment further so user stays "at" pending
    return hist->pending;
}

void ik_history_stop_browsing(ik_history_t *hist)
{
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    // Move to non-browsing position
    hist->index = hist->count;

    // Free pending input
    if (hist->pending != NULL) {
        talloc_free(hist->pending);
        hist->pending = NULL;
    }
}

const char *ik_history_get_current(const ik_history_t *hist)
{
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    // If not browsing (index == count), return pending or NULL
    if (hist->index >= hist->count) {
        return hist->pending;
    }

    // Otherwise return entry at current index
    return hist->entries[hist->index];
}

bool ik_history_is_browsing(const ik_history_t *hist)
{
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    // Browsing if index is less than count
    return hist->index < hist->count;
}

res_t ik_history_ensure_directory(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    const char *dir = ".ikigai";
    struct stat st;

    // Check if path exists
    if (posix_stat_(dir, &st) == 0) {
        // Path exists - verify it's a directory
        if (S_ISDIR(st.st_mode)) {
            return OK(NULL);
        }
        // Path exists but is not a directory - error
        return ERR(ctx, IO, "Failed to create %s: %s", dir, "File exists");
    }

    // Path doesn't exist - try to create it
    if (posix_mkdir_(dir, 0755) != 0) {
        // If directory already exists (race condition), that's OK
        if (errno == EEXIST) {
            return OK(NULL);
        }
        // Return error for other failure reasons (permissions, disk full, etc.)
        return ERR(ctx, IO, "Failed to create %s: %s", dir, strerror(errno));
    }

    return OK(NULL);
}

// Parse a single JSONL history line and extract the cmd field
// Returns OK(cmd_string) on success, ERR on parse failure, OK(NULL) to skip
static res_t parse_history_line(TALLOC_CTX *ctx, const char *line, ik_logger_t *logger)
{
    // Parse JSON line
    yyjson_doc *doc = yyjson_read_(line, strlen(line), 0);
    if (doc == NULL) {
        // Malformed JSON - log warning and skip
        yyjson_mut_doc *log_doc = ik_log_create();
        yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
        if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_str(log_doc, root, "message", "Skipping malformed history line: not valid json")) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        ik_logger_warn_json(logger, log_doc);
        return OK(NULL);  // Skip this line
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (!root || !yyjson_is_obj(root)) {  // LCOV_EXCL_BR_LINE - vendor function defensive check
        // Not an object - skip
        yyjson_mut_doc *log_doc = ik_log_create();
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);
        if (log_root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_str(log_doc, log_root, "message", "Skipping non-object history line")) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        ik_logger_warn_json(logger, log_doc);
        yyjson_doc_free(doc);
        return OK(NULL);  // Skip this line
    }

    // Extract "cmd" field
    yyjson_val *cmd_val = yyjson_obj_get_(root, "cmd");
    if (!cmd_val || !yyjson_is_str(cmd_val)) {
        // Missing or invalid cmd field - skip
        yyjson_mut_doc *log_doc = ik_log_create();
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);
        if (log_root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_str(log_doc, log_root, "message", "Skipping history line with missing/invalid cmd field")) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        ik_logger_warn_json(logger, log_doc);
        yyjson_doc_free(doc);
        return OK(NULL);  // Skip this line
    }

    const char *cmd = yyjson_get_str_(cmd_val);
    char *result = talloc_strdup_(ctx, cmd);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_doc_free(doc);
    return OK(result);
}

res_t ik_history_load(TALLOC_CTX *ctx, ik_history_t *hist, ik_logger_t *logger)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    // Ensure directory exists
    res_t dir_res = ik_history_ensure_directory(ctx);
    if (dir_res.is_err) {
        return dir_res;
    }

    const char *path = ".ikigai/history";

    // Check if file exists
    struct stat st;
    if (posix_stat_(path, &st) != 0) {
        // File doesn't exist - create empty file
        FILE *f = fopen_(path, "w");
        if (f == NULL) {  // LCOV_EXCL_BR_LINE
            return ERR(ctx, IO, "Failed to create %s: %s", path, strerror(errno));
        }
        fclose_(f);
        return OK(NULL);  // Empty history
    }

    // Read entire file using utility
    char *contents = NULL;
    size_t file_size = 0;
    res_t read_result = ik_file_read_all(ctx, path, &contents, &file_size);

    if (read_result.is_err) {
        return read_result;
    }

    // Handle empty file
    if (file_size == 0) {
        return OK(NULL);
    }

    // Parse JSONL - split by newlines and parse each line
    char *line = contents;
    char *next_line;

    // Create temporary context for parsing
    TALLOC_CTX *parse_ctx = talloc_new(ctx);
    if (parse_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Collect all valid entries first - allocate large array to handle files with more than capacity
    // We'll filter to capacity later
    size_t max_entries = hist->capacity * 2;  // Allow up to 2x capacity in file
    char **temp_entries = talloc_array_(parse_ctx, sizeof(char *), max_entries);
    if (temp_entries == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t temp_count = 0;

    while (*line != '\0') {
        // Find next newline
        next_line = strchr(line, '\n');
        if (next_line != NULL) {
            *next_line = '\0';
            next_line++;
        } else {
            next_line = line + strlen(line);
        }

        // Skip empty lines
        if (*line == '\0') {
            line = next_line;
            continue;
        }

        // Check if we have space
        if (temp_count >= max_entries) {
            break;
        }

        // Parse line and extract cmd
        res_t parse_result = parse_history_line(parse_ctx, line, logger);
        if (parse_result.is_err) {  // LCOV_EXCL_START - parse_history_line never returns error
            talloc_free(parse_ctx);
            return parse_result;
        }  // LCOV_EXCL_STOP

        char *cmd = (char *)parse_result.ok;
        if (cmd != NULL) {
            temp_entries[temp_count] = cmd;
            temp_count++;
        }

        line = next_line;
    }

    // If we have more entries than capacity, keep only the most recent
    size_t start_index = 0;
    if (temp_count > hist->capacity) {
        start_index = temp_count - hist->capacity;
        temp_count = hist->capacity;
    }

    // Add entries to history
    for (size_t i = 0; i < temp_count; i++) {
        res_t add_res = ik_history_add(hist, temp_entries[start_index + i]);
        if (add_res.is_err) {  // LCOV_EXCL_START - ik_history_add never returns error
            talloc_free(parse_ctx);
            return add_res;
        }  // LCOV_EXCL_STOP
    }

    talloc_free(parse_ctx);
    return OK(NULL);
}

// Helper: Format a history entry as JSONL line (returns malloc'd string, caller must free)
static char *format_history_entry(TALLOC_CTX *ctx, const char *cmd)
{
    // Get current timestamp in ISO 8601 format
    time_t now = time(NULL);
    struct tm *tm_info = gmtime_(&now);
    if (tm_info == NULL) return NULL;  // LCOV_EXCL_BR_LINE

    char timestamp[64];
    size_t ts_len = strftime_(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    if (ts_len == 0) return NULL;  // LCOV_EXCL_BR_LINE

    // Create JSON object
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "cmd", cmd);
    yyjson_mut_obj_add_str(doc, root, "ts", timestamp);

    // Write JSON to string (returns malloc'd string)
    size_t json_len;
    return yyjson_mut_write(doc, 0, &json_len);
}

res_t ik_history_save(const ik_history_t *hist)
{
    assert(hist != NULL);  // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    res_t dir_res = ik_history_ensure_directory(tmp_ctx);
    if (dir_res.is_err) {
        talloc_steal(dir_res.err, tmp_ctx);
        return dir_res;
    }

    const char *path = ".ikigai/history";
    const char *tmp_path = ".ikigai/history.tmp";

    FILE *f = fopen_(tmp_path, "w");
    if (f == NULL) {  // LCOV_EXCL_BR_LINE
        err_t *error = _make_error(tmp_ctx, ERR_IO, __FILE__, __LINE__, "Failed to create %s: %s", tmp_path, strerror(errno));
        talloc_steal(error, tmp_ctx);
        return err(error);
    }

    for (size_t i = 0; i < hist->count; i++) {
        char *json_str = format_history_entry(tmp_ctx, hist->entries[i]);
        if (json_str == NULL) {  // LCOV_EXCL_START - format_history_entry only fails on OOM
            fclose_(f);
            unlink(tmp_path);
            err_t *error = _make_error(tmp_ctx, ERR_IO, __FILE__, __LINE__, "Failed to format entry");
            talloc_steal(error, tmp_ctx);
            return err(error);
        }  // LCOV_EXCL_STOP

        if (fprintf(f, "%s\n", json_str) < 0) {  // LCOV_EXCL_START - fprintf rarely fails, hard to mock
            free(json_str);
            fclose_(f);
            unlink(tmp_path);
            err_t *error = _make_error(tmp_ctx, ERR_IO, __FILE__, __LINE__, "Failed to write to %s: %s", tmp_path, strerror(errno));
            talloc_steal(error, tmp_ctx);
            return err(error);
        }  // LCOV_EXCL_STOP

        free(json_str);
    }

    fclose_(f);

    if (posix_rename_(tmp_path, path) != 0) {  // LCOV_EXCL_BR_LINE
        unlink(tmp_path);
        err_t *error = _make_error(tmp_ctx, ERR_IO, __FILE__, __LINE__, "Failed to rename %s to %s: %s", tmp_path, path, strerror(errno));
        talloc_steal(error, tmp_ctx);
        return err(error);
    }

    talloc_free(tmp_ctx);
    return OK(NULL);
}

res_t ik_history_append_entry(const char *entry)
{
    assert(entry != NULL);  // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    res_t dir_res = ik_history_ensure_directory(tmp_ctx);
    if (dir_res.is_err) {
        talloc_steal(dir_res.err, tmp_ctx);
        return dir_res;
    }

    const char *path = ".ikigai/history";

    FILE *f = fopen_(path, "a");
    if (f == NULL) {  // LCOV_EXCL_BR_LINE
        err_t *error = _make_error(tmp_ctx, ERR_IO, __FILE__, __LINE__, "Failed to open %s: %s", path, strerror(errno));
        talloc_steal(error, tmp_ctx);
        return err(error);
    }

    char *json_str = format_history_entry(tmp_ctx, entry);
    if (json_str == NULL) {  // LCOV_EXCL_START - format_history_entry only fails on OOM
        fclose_(f);
        err_t *error = _make_error(tmp_ctx, ERR_IO, __FILE__, __LINE__, "Failed to format entry");
        talloc_steal(error, tmp_ctx);
        return err(error);
    }  // LCOV_EXCL_STOP

    if (fprintf(f, "%s\n", json_str) < 0) {  // LCOV_EXCL_START - fprintf rarely fails, hard to mock
        free(json_str);
        fclose_(f);
        err_t *error = _make_error(tmp_ctx, ERR_IO, __FILE__, __LINE__, "Failed to write to %s: %s", path, strerror(errno));
        talloc_steal(error, tmp_ctx);
        return err(error);
    }  // LCOV_EXCL_STOP

    free(json_str);
    fclose_(f);
    talloc_free(tmp_ctx);
    return OK(NULL);
}
