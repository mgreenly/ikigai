/**
 * @file message_tool_result_helpers.h
 * @brief Unit tests for message.c tool_result message handling
 */

#ifndef MESSAGE_TOOL_RESULT_HELPERS_H
#define MESSAGE_TOOL_RESULT_HELPERS_H

#include <check.h>

/**
 * Creates a test case for tool_result message tests
 * @return TCase for tool_result tests
 */
TCase *create_tool_result_tcase(void);

#endif