# Task: Create Common Provider Tests

**Layer:** 2
**Depends on:** provider-types.md, shared-utilities.md, request-builders.md

## Pre-Read

**Skills:**
- `/load tdd`

**Source Files:**
- Existing test files in `tests/unit/`

**Plan Docs:**
- `scratch/plan/testing-strategy.md`

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

## Postconditions

- [ ] Common utilities have tests
- [ ] Mock pattern established
- [ ] Fixtures organized
