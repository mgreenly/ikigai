# Task: Implement Provider Error Handling

**Layer:** 1
**Depends on:** provider-types.md

## Pre-Read

- **Skills:** `/load errors`
- **Source:** `src/error.c`, `src/error.h`
- **Plan:** `scratch/plan/error-handling.md`
- **Research:** `scratch/research/anthropic.md`, `scratch/research/openai.md`, `scratch/research/google.md` (error sections)

## Objective

Implement error mapping and retry logic for all providers.

## Deliverables

1. Create `src/providers/error.c`:
   - `ik_error_category_name()` - Category to string
   - `ik_error_is_retryable()` - Check if error can be retried
   - `ik_error_user_message()` - Generate user-facing message

2. Implement per-provider error handling:
   - `ik_anthropic_handle_error()` - Map Anthropic HTTP errors
   - `ik_openai_handle_error()` - Map OpenAI HTTP errors
   - `ik_google_handle_error()` - Map Google HTTP errors

3. Extract retry-after from headers:
   - Anthropic: `retry-after` header
   - OpenAI: `x-ratelimit-reset-*` headers
   - Google: `retryDelay` in response body

4. User-facing error messages:
   - Authentication errors with fix instructions
   - Rate limit with retry countdown
   - Server errors with retry status

## Reference

- `scratch/plan/error-handling.md` - Full specification

## Error Categories

- ERR_AUTH - Invalid credentials (no retry)
- ERR_RATE_LIMIT - Rate limited (retry with delay)
- ERR_INVALID_ARG - Bad request (no retry)
- ERR_NOT_FOUND - Model not found (no retry)
- ERR_SERVER - Server error (retry with backoff)
- ERR_TIMEOUT - Timeout (retry immediately)
- ERR_CONTENT_FILTER - Content blocked (no retry)

## Verification

- All HTTP status codes mapped correctly
- Retry-after extracted from each provider
- User messages are helpful

## Postconditions

- [ ] All categories mapped
- [ ] Retry logic works
- [ ] User messages are helpful
