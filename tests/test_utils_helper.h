#ifndef IK_TEST_UTILS_H
#define IK_TEST_UTILS_H

#include <talloc.h>
#include "test_constants.h"
#include "test_utils_db_helper.h"
#include "config.h"
#include "../src/db/connection.h"
#include "../src/error.h"
#include "../src/shared.h"

// Forward declarations
typedef struct ik_agent_ctx ik_agent_ctx_t;

// Test utilities for ikigai test suite

// ========== Wrapper Function Overrides ==========
// These override the weak symbols in src/wrapper.c for testing

// Thread-local mocking controls for talloc_realloc_
// Using __thread to ensure each test file running in parallel has its own state
extern __thread int ik_test_talloc_realloc_fail_on_call;  // -1 = don't fail, >= 0 = fail on this call
extern __thread int ik_test_talloc_realloc_call_count;

void *talloc_zero_(TALLOC_CTX *ctx, size_t size);
char *talloc_strdup_(TALLOC_CTX *ctx, const char *str);
void *talloc_array_(TALLOC_CTX *ctx, size_t el_size, size_t count);
void *talloc_realloc_(TALLOC_CTX *ctx, void *ptr, size_t size);
char *talloc_asprintf_(TALLOC_CTX *ctx, const char *fmt, ...);

// ========== Test Config Helper ==========
// Creates a minimal config for testing (does not require config file)
ik_config_t *ik_test_create_config(TALLOC_CTX *ctx);

// ========== File I/O Helpers ==========
// Load a file into a talloc-allocated string
char *load_file_to_string(TALLOC_CTX *ctx, const char *path);

// ========== Database Test Utilities ==========
// Per-file database isolation for parallel test execution
//
// Usage Pattern A (most tests - with migrations, transaction isolation):
//   static const char *DB_NAME;
//   static ik_db_ctx_t *db;
//   static TALLOC_CTX *test_ctx;
//
//   // Suite setup (once per file)
//   DB_NAME = ik_test_db_name(NULL, __FILE__);
//   ik_test_db_create(DB_NAME);
//   ik_test_db_migrate(NULL, DB_NAME);
//
//   // Per-test setup
//   test_ctx = talloc_new(NULL);
//   ik_test_db_connect(test_ctx, DB_NAME, &db);
//   ik_test_db_begin(db);
//
//   // Per-test teardown
//   ik_test_db_rollback(db);
//   talloc_free(test_ctx);
//
//   // Suite teardown (once per file)
//   ik_test_db_destroy(DB_NAME);
//
// Usage Pattern B (migration tests - empty DB, no migrations):
//   DB_NAME = ik_test_db_name(NULL, __FILE__);
//   ik_test_db_create(DB_NAME);  // No migrate call
//   // ... test migration logic ...
//   ik_test_db_destroy(DB_NAME);

/**
 * Derive database name from source file path
 *
 * Extracts the base filename (without extension) and prefixes with "ikigai_test_".
 * Example: "tests/unit/db/session_test.c" → "ikigai_test_session_test"
 *
 * @param ctx Talloc context for allocation (can be NULL for static lifetime)
 * @param file_path Source file path (typically __FILE__)
 * @return Database name string
 */
const char *ik_test_db_name(TALLOC_CTX *ctx, const char *file_path);

/**
 * Build connection string for test database
 *
 * Respects PGHOST environment variable (defaults to localhost if not set).
 * Example: "ikigai_test_foo" → "postgresql://ikigai:ikigai@postgres/ikigai_test_foo"
 *
 * @param buf Buffer to write connection string to
 * @param bufsize Size of buffer
 * @param db_name Database name
 */
void ik_test_db_conn_str(char *buf, size_t bufsize, const char *db_name);

/**
 * Create test database (drops if exists)
 *
 * Connects to PostgreSQL default database and issues DROP/CREATE.
 * Idempotent - safe to call regardless of previous state.
 *
 * @param db_name Database name to create
 * @return OK on success, ERR on failure
 */
res_t ik_test_db_create(const char *db_name);

/**
 * Run migrations on test database
 *
 * Applies all migrations from ./share/ikigai/migrations/ directory.
 *
 * @param ctx Talloc context for temporary allocations
 * @param db_name Database name to migrate
 * @return OK on success, ERR on failure
 */
res_t ik_test_db_migrate(TALLOC_CTX *ctx, const char *db_name);

/**
 * Open connection to test database (no migrations)
 *
 * Creates a raw connection without running migrations.
 * Use this after ik_test_db_create() and optionally ik_test_db_migrate().
 *
 * @param ctx Talloc context for db_ctx allocation
 * @param db_name Database name to connect to
 * @param out Output parameter for database context
 * @return OK on success, ERR on failure
 */
res_t ik_test_db_connect(TALLOC_CTX *ctx, const char *db_name, ik_db_ctx_t **out);

/**
 * Begin transaction (for test isolation within a file)
 *
 * @param db Database context
 * @return OK on success, ERR on failure
 */
res_t ik_test_db_begin(ik_db_ctx_t *db);

/**
 * Rollback transaction (discard test changes)
 *
 * @param db Database context
 * @return OK on success, ERR on failure
 */
res_t ik_test_db_rollback(ik_db_ctx_t *db);

/**
 * Truncate all application tables
 *
 * Resets sessions and messages tables. Use when transaction isolation
 * is not suitable (e.g., testing commit behavior).
 *
 * @param db Database context
 * @return OK on success, ERR on failure
 */
res_t ik_test_db_truncate_all(ik_db_ctx_t *db);

/**
 * Drop test database completely
 *
 * Should be called as last action of test file.
 *
 * @param db_name Database name to destroy
 * @return OK on success, ERR on failure
 */
res_t ik_test_db_destroy(const char *db_name);

/**
 * Set IKIGAI_LOG_DIR to a unique path based on test file.
 * Call this in suite_setup() before any logger is created.
 *
 * @param file_path  Pass __FILE__ to derive unique directory name
 */
void ik_test_set_log_dir(const char *file_path);

// ========== Terminal Utilities ==========

/**
 * Sanitize ANSI escape sequences for safe display
 *
 * Replaces ESC (0x1b) characters with "<ESC>" so terminal escape sequences
 * can be logged without executing them. Useful for debug output in tests
 * that capture render output.
 *
 * @param ctx Talloc context for allocation
 * @param input Input buffer (may contain binary data)
 * @param len Length of input buffer
 * @return Sanitized string (null-terminated), or NULL on allocation failure
 */
char *ik_test_sanitize_ansi(TALLOC_CTX *ctx, const char *input, size_t len);

/**
 * Reset terminal state after tests that may emit escape sequences.
 *
 * Call this in suite teardown for any test file that:
 * - Mocks posix_write_ for terminal output
 * - Tests rendering code
 * - Exercises cursor visibility
 *
 * Safe to call even if terminal is in normal state.
 */
void ik_test_reset_terminal(void);

// ========== Agent Test Utilities ==========

/**
 * Create a minimal agent for testing
 *
 * Creates an agent context with a minimal shared context.
 * The agent will have display state (scrollback, layers, etc.) initialized.
 *
 * @param ctx Talloc context for allocation
 * @param out Output parameter for agent context
 * @return OK on success, ERR on failure
 */
res_t ik_test_create_agent(TALLOC_CTX *ctx, ik_agent_ctx_t **out);

// ========== Tool JSON Test Helpers ==========

// Forward declaration for yyjson types
typedef struct yyjson_doc yyjson_doc;
typedef struct yyjson_val yyjson_val;

/**
 * Parse JSON tool response and verify success=true
 *
 * @param json JSON string to parse
 * @param out_doc Output parameter for document (caller must free)
 * @return data object from response
 */
yyjson_val *ik_test_tool_parse_success(const char *json, yyjson_doc **out_doc);

/**
 * Parse JSON tool response and verify success=false
 *
 * @param json JSON string to parse
 * @param out_doc Output parameter for document (caller must free)
 * @return error message string
 */
const char *ik_test_tool_parse_error(const char *json, yyjson_doc **out_doc);

/**
 * Extract output field from tool data object
 *
 * @param data Data object from tool response
 * @return output string
 */
const char *ik_test_tool_get_output(yyjson_val *data);

/**
 * Extract exit_code field from tool data object
 *
 * @param data Data object from tool response
 * @return exit code integer
 */
int64_t ik_test_tool_get_exit_code(yyjson_val *data);

// ========== Paths Test Helpers ==========

/**
 * Set up isolated PID-based path environment for testing
 *
 * Creates unique test directories under /tmp/ikigai_test_${PID}/ and sets
 * IKIGAI_*_DIR environment variables. Each test process gets unique
 * directories to prevent cross-test interference during parallel execution.
 *
 * @return Path prefix (/tmp/ikigai_test_${PID}) or NULL on error
 */
const char *test_paths_setup_env(void);

/**
 * Clean up test path environment
 *
 * Unsets IKIGAI_*_DIR environment variables and removes test directory tree.
 * Safe to call multiple times (idempotent).
 */
void test_paths_cleanup_env(void);

#endif // IK_TEST_UTILS_H
