/**
 * @file commands_mail_helpers.h
 * @brief Helper functions for mail command implementations
 */

#ifndef IK_COMMANDS_MAIL_HELPERS_H
#define IK_COMMANDS_MAIL_HELPERS_H

#include "error.h"
#include "mail/msg.h"
#include "scrollback.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Format a relative timestamp string
 * @param diff Time difference in seconds
 * @param buf Output buffer
 * @param buf_size Size of output buffer
 */
void ik_mail_format_timestamp(int64_t diff, char *buf, size_t buf_size);

/**
 * @brief Render a mail message list to scrollback
 * @param ctx Memory context
 * @param scrollback Target scrollback
 * @param inbox Array of messages
 * @param count Number of messages
 * @return Result
 */
res_t ik_mail_render_list(void *ctx, ik_scrollback_t *scrollback, ik_mail_msg_t **inbox, size_t count);

/**
 * @brief Parse UUID from argument string
 * @param args Input string
 * @param uuid_out Output buffer (must be at least 256 bytes)
 * @return true if parsed successfully
 */
bool ik_mail_parse_uuid(const char *args, char *uuid_out);

/**
 * @brief Parse message index from argument string
 * @param args Input string
 * @param index_out Output for parsed index (1-based)
 * @return true if parsed successfully
 */
bool ik_mail_parse_index(const char *args, int64_t *index_out);

#endif  // IK_COMMANDS_MAIL_HELPERS_H
