# Task: Create Common Provider Tests

**Phase:** Testing
**Depends on:** 01-provider-types, 02-shared-utilities, 03-request-builders

## Objective

Create test suite for shared provider infrastructure.

## Deliverables

1. Create `tests/unit/providers/common/`:
   - `test_http_client.c` - HTTP client tests
   - `test_sse_parser.c` - SSE parser tests

2. Create `tests/unit/providers/`:
   - `test_provider_common.c` - Provider creation, credentials
   - `test_request_builders.c` - Request/response building
   - `test_error_handling.c` - Error mapping

3. Create test fixtures:
   - `tests/fixtures/` structure for responses

4. Mock HTTP pattern:
   - Mock `curl_easy_perform_()` wrapper
   - Support for streaming mocks

## Reference

- `scratch/plan/testing-strategy.md` - Full specification

## Verification

- All common utilities have tests
- Mock pattern established for reuse
- Fixtures organized per provider
