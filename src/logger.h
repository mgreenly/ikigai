// Logger module - simple stdout/stderr logging following systemd conventions
// Thread-safe with atomic log line writes using pthread mutex

#ifndef IK_LOGGER_H
#define IK_LOGGER_H

#include "vendor/yyjson/yyjson.h"
#include <talloc.h>

// Opaque logger context type (dependency injection pattern)
typedef struct ik_logger ik_logger_t;

// Create logger instance with explicit context injection
// ctx: talloc parent for memory management
// working_dir: directory where .ikigai/logs will be created
// Returns: logger instance (owned by ctx)
ik_logger_t *ik_logger_create(TALLOC_CTX *ctx, const char *working_dir);

// Reinitialize logger for a new working directory
void ik_logger_reinit(ik_logger_t *logger, const char *working_dir);

// Legacy global logger initialization (for backward compatibility during migration)
// Prefer ik_logger_create() for new code
void ik_log_init(const char *working_dir);
void ik_log_shutdown(void);
void ik_log_reinit(const char *working_dir);

// New JSONL logging API
// Create a log document (returns doc with empty root object)
yyjson_mut_doc *ik_log_create(void);

// Log functions with explicit logger context (DI pattern)
// logger: logger instance (must not be NULL)
// doc: JSON document (ownership transferred - will be freed)
void ik_logger_debug_json(ik_logger_t *logger, yyjson_mut_doc *doc);
void ik_logger_info_json(ik_logger_t *logger, yyjson_mut_doc *doc);
void ik_logger_warn_json(ik_logger_t *logger, yyjson_mut_doc *doc);
void ik_logger_error_json(ik_logger_t *logger, yyjson_mut_doc *doc);
void ik_logger_fatal_json(ik_logger_t *logger, yyjson_mut_doc *doc) __attribute__((noreturn));

// Legacy log functions using global logger (for backward compatibility during migration)
// Prefer ik_logger_*_json() with explicit logger context for new code
void ik_log_debug_json(yyjson_mut_doc *doc);
void ik_log_info_json(yyjson_mut_doc *doc);
void ik_log_warn_json(yyjson_mut_doc *doc);
void ik_log_error_json(yyjson_mut_doc *doc);
void ik_log_fatal_json(yyjson_mut_doc *doc) __attribute__((noreturn));

// Get file descriptor for low-level writes (panic handler)
// Returns -1 if logger or file is NULL
int ik_logger_get_fd(ik_logger_t *logger);

#endif // IK_LOGGER_H
