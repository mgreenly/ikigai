/**
 * @file commands_mail_helpers.h
 * @brief Helper functions for mail command implementations
 */

#ifndef IK_COMMANDS_MAIL_HELPERS_H
#define IK_COMMANDS_MAIL_HELPERS_H

#include <stdbool.h>

/**
 * @brief Parse UUID from argument string
 * @param args Input string
 * @param uuid_out Output buffer (must be at least 256 bytes)
 * @return true if parsed successfully
 */
bool ik_mail_parse_uuid(const char *args, char *uuid_out);

#endif  // IK_COMMANDS_MAIL_HELPERS_H
