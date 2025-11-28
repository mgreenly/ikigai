// Logger module - simple stdout/stderr logging following systemd conventions
// Thread-safe with atomic log line writes using pthread mutex

#ifndef IK_LOGGER_H
#define IK_LOGGER_H

void ik_log_debug(const char *fmt, ...);

void ik_log_info(const char *fmt, ...);

void ik_log_warn(const char *fmt, ...);

void ik_log_error(const char *fmt, ...);

// Reset timestamp detection (for testing only)
void ik_log_reset_timestamp_check(void);

#endif // IK_LOGGER_H
