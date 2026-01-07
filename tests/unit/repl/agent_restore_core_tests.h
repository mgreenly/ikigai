/**
 * @file agent_restore_core_tests.h
 * @brief Core test functions for agent restore functionality
 */

#pragma once

#include <check.h>

// Test case declarations
START_TEST(test_restore_agents_queries_running_agents);
START_TEST(test_restore_agents_sorts_by_created_at);
START_TEST(test_restore_agents_skips_none_restores_all_running);
START_TEST(test_restore_agents_handles_agent0_specially);
START_TEST(test_restore_agents_populates_conversation);
START_TEST(test_restore_agents_populates_scrollback);
START_TEST(test_restore_agents_handles_mark_events);