/**
 * @file message_tool_call_test.h
 * @brief Unit tests for message.c tool_call message handling
 */

#ifndef MESSAGE_TOOL_CALL_TEST_H
#define MESSAGE_TOOL_CALL_TEST_H

#include <check.h>

/**
 * Creates a test case for tool_call message tests
 * @return TCase for tool_call tests
 */
TCase *create_tool_call_tcase(void);

#endif