// Logger module - simple stdout/stderr logging following systemd conventions
// Thread-safe with atomic log line writes using pthread mutex

#ifndef IK_LOGGER_H
#define IK_LOGGER_H

#include "vendor/yyjson/yyjson.h"

// Legacy printf-style logging (to be deprecated)
void ik_log_debug(const char *fmt, ...);
void ik_log_info(const char *fmt, ...);
void ik_log_warn(const char *fmt, ...);
void ik_log_error(const char *fmt, ...);

// Reset timestamp detection (for testing only)
void ik_log_reset_timestamp_check(void);

// Logger initialization and shutdown
void ik_log_init(const char *working_dir);
void ik_log_shutdown(void);
void ik_log_reinit(const char *working_dir);

// New JSONL logging API
// Create a log document (returns doc with empty root object)
yyjson_mut_doc *ik_log_create(void);

// Log functions (take ownership of doc, wrap it, write, free)
void ik_log_debug_json(yyjson_mut_doc *doc);
void ik_log_info_json(yyjson_mut_doc *doc);
void ik_log_warn_json(yyjson_mut_doc *doc);
void ik_log_error_json(yyjson_mut_doc *doc);
void ik_log_fatal_json(yyjson_mut_doc *doc) __attribute__((noreturn));

#endif // IK_LOGGER_H
