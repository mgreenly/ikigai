/**
 * @file vcr.h
 * @brief VCR (Video Cassette Recorder) - HTTP recording/replay for deterministic tests
 *
 * VCR provides a testing infrastructure that records HTTP interactions to JSONL
 * fixture files and replays them in subsequent test runs. This enables fast,
 * deterministic tests without making real API calls.
 *
 * ## Modes
 * - **Record mode** (VCR_RECORD=1): Makes real HTTP calls, writes to fixtures
 * - **Playback mode** (default): Reads from fixtures, no real HTTP calls
 *
 * ## Usage
 *
 * ```c
 * // In test setup
 * vcr_init("test_name", "provider");
 *
 * // Use vcr_ck_assert_* macros instead of ck_assert_*
 * vcr_ck_assert_int_eq(status, 200);
 *
 * // In test teardown
 * vcr_finish();
 * ```
 *
 * ## Fixture Format
 *
 * JSONL (one JSON object per line):
 * - `_request`: HTTP request metadata
 * - `_response`: HTTP response metadata
 * - `_body`: Complete response body (non-streaming)
 * - `_chunk`: Raw chunk as delivered by curl (streaming)
 */

#ifndef VCR_H
#define VCR_H

#include <stdbool.h>
#include <stddef.h>
#include <check.h>

/**
 * Global flag indicating VCR recording mode
 *
 * Set by vcr_init() based on VCR_RECORD environment variable.
 * Used by vcr_ck_assert_* macros to suppress assertions during recording.
 */
extern bool vcr_recording;

/**
 * Initialize VCR for a test
 *
 * Opens fixture file for reading (playback) or writing (record mode).
 * In playback mode, parses entire fixture into memory.
 *
 * @param test_name Test name (used in fixture filename)
 * @param provider Provider name (subdirectory: anthropic, google, brave, openai)
 */
void vcr_init(const char *test_name, const char *provider);

/**
 * Clean up VCR resources
 *
 * Closes fixture file, frees all allocated memory.
 * Must be called after vcr_init().
 */
void vcr_finish(void);

/**
 * Check if VCR is active
 *
 * @return true if vcr_init() was called and vcr_finish() has not been called
 */
bool vcr_is_active(void);

/**
 * Check if VCR is in recording mode
 *
 * @return true if VCR_RECORD=1, false otherwise
 */
bool vcr_is_recording(void);

/**
 * Get HTTP response status code from fixture
 *
 * Returns the status code from the _response line in playback mode.
 * Only valid after vcr_init() and before vcr_finish().
 *
 * @return HTTP status code (e.g., 200, 404, 500), or 0 if not available
 */
int vcr_get_response_status(void);

/**
 * Disable request verification for this test
 *
 * Call after vcr_init() to skip request matching in playback mode.
 * Useful when request order or content varies between runs.
 */
void vcr_skip_request_verification(void);

/**
 * Get next chunk from playback queue
 *
 * Returns next chunk as a null-terminated string. The chunk remains valid
 * until vcr_finish() is called.
 *
 * @param data_out Output parameter for chunk data (null-terminated string)
 * @param len_out Output parameter for chunk length (strlen of data_out)
 * @return true if chunk was retrieved, false if no more chunks
 */
bool vcr_next_chunk(const char **data_out, size_t *len_out);

/**
 * Check if more chunks are available
 *
 * @return true if vcr_next_chunk() will return data, false otherwise
 */
bool vcr_has_more(void);

/**
 * Record HTTP request to fixture
 *
 * Writes `_request` line with redacted credentials.
 * Only effective in record mode.
 *
 * @param method HTTP method (GET, POST, etc.)
 * @param url Request URL
 * @param headers Request headers (newline-separated)
 * @param body Request body (NULL for no body)
 */
void vcr_record_request(const char *method, const char *url,
                       const char *headers, const char *body);

/**
 * Record HTTP response metadata to fixture
 *
 * Writes `_response` line with status and headers.
 * Only effective in record mode.
 *
 * @param status HTTP status code
 * @param headers Response headers (newline-separated)
 */
void vcr_record_response(int status, const char *headers);

/**
 * Record streaming chunk to fixture
 *
 * Writes `_chunk` line with raw chunk data.
 * Only effective in record mode.
 *
 * @param data Chunk data
 * @param len Chunk length
 */
void vcr_record_chunk(const char *data, size_t len);

/**
 * Record complete response body to fixture
 *
 * Writes `_body` line with entire response.
 * Only effective in record mode.
 *
 * @param data Response body data
 * @param len Response body length
 */
void vcr_record_body(const char *data, size_t len);

/**
 * Verify request matches recorded request
 *
 * Compares method/url/body against recorded request in playback mode.
 * Logs warning on mismatch but doesn't fail test.
 * Only effective in playback mode (unless verification skipped).
 *
 * @param method HTTP method
 * @param url Request URL
 * @param body Request body (NULL for no body)
 */
void vcr_verify_request(const char *method, const char *url, const char *body);

/**
 * VCR-aware assertion macros
 *
 * These macros behave like standard Check framework assertions in playback mode,
 * but become no-ops in record mode. This allows tests to run and make HTTP calls
 * during recording without failing on dynamic response data.
 */

#define vcr_ck_assert(expr) \
    do { if (!vcr_recording) ck_assert(expr); } while(0)

#define vcr_ck_assert_int_eq(a, b) \
    do { if (!vcr_recording) ck_assert_int_eq(a, b); } while(0)

#define vcr_ck_assert_str_eq(a, b) \
    do { if (!vcr_recording) ck_assert_str_eq(a, b); } while(0)

#define vcr_ck_assert_ptr_nonnull(ptr) \
    do { if (!vcr_recording) ck_assert_ptr_nonnull(ptr); } while(0)

#define vcr_ck_assert_ptr_null(ptr) \
    do { if (!vcr_recording) ck_assert_ptr_null(ptr); } while(0)

#endif // VCR_H
