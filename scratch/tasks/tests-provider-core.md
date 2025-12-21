# Task: Create Provider Core Tests

**Model:** sonnet/thinking
**Depends on:** provider-factory.md, error-core.md, credentials-core.md, tests-mock-infrastructure.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns
- `/load errors` - Error handling patterns

**Source:**
- `src/providers/factory.c` - Provider factory implementation
- `src/providers/common/error.c` - Error handling implementation
- `src/credentials.c` - Credentials API

**Plan:**
- `scratch/plan/testing-strategy.md` - Test patterns

## Objective

Create unit tests for provider creation, error category handling, and credentials lookup. These tests verify the core provider abstraction works correctly.

## Interface

**Files to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/test_provider_factory.c` | Provider creation tests |
| `tests/unit/providers/common/test_error_handling.c` | Error category tests |
| `tests/unit/test_credentials.c` | Credentials lookup tests |

## Behaviors

**Provider Factory Tests (`test_provider_factory.c`):**

```c
START_TEST(test_create_openai_provider)
{
    setenv("OPENAI_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider->name, "openai");
    ck_assert_ptr_nonnull(provider->vtable);
    ck_assert_ptr_nonnull(provider->vtable->send);

    unsetenv("OPENAI_API_KEY");
}
END_TEST

START_TEST(test_create_anthropic_provider)
{
    setenv("ANTHROPIC_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "anthropic", &provider);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(provider->name, "anthropic");

    unsetenv("ANTHROPIC_API_KEY");
}
END_TEST

START_TEST(test_create_google_provider)
{
    setenv("GOOGLE_API_KEY", "test-key", 1);

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "google", &provider);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(provider->name, "google");

    unsetenv("GOOGLE_API_KEY");
}
END_TEST

START_TEST(test_create_unknown_provider_fails)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "unknown", &provider);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

START_TEST(test_create_provider_missing_credentials)
{
    unsetenv("OPENAI_API_KEY");

    ik_provider_t *provider = NULL;
    res_t result = ik_provider_create(test_ctx, "openai", &provider);

    ck_assert(is_err(&result));
    // Should indicate missing credentials
}
END_TEST
```

**Error Handling Tests (`test_error_handling.c`):**

```c
START_TEST(test_error_category_names)
{
    ck_assert_str_eq(ik_error_category_name(ERR_CAT_AUTH), "authentication");
    ck_assert_str_eq(ik_error_category_name(ERR_CAT_RATE_LIMIT), "rate_limit");
    ck_assert_str_eq(ik_error_category_name(ERR_CAT_SERVICE), "service");
    ck_assert_str_eq(ik_error_category_name(ERR_CAT_NETWORK), "network");
    ck_assert_str_eq(ik_error_category_name(ERR_CAT_INVALID_REQUEST), "invalid_request");
}
END_TEST

START_TEST(test_error_is_retryable)
{
    ck_assert(ik_error_is_retryable(ERR_CAT_RATE_LIMIT));
    ck_assert(ik_error_is_retryable(ERR_CAT_SERVICE));
    ck_assert(ik_error_is_retryable(ERR_CAT_NETWORK));

    ck_assert(!ik_error_is_retryable(ERR_CAT_AUTH));
    ck_assert(!ik_error_is_retryable(ERR_CAT_INVALID_REQUEST));
}
END_TEST

START_TEST(test_http_status_to_category)
{
    ck_assert_int_eq(ik_http_status_to_category(401), ERR_CAT_AUTH);
    ck_assert_int_eq(ik_http_status_to_category(403), ERR_CAT_AUTH);
    ck_assert_int_eq(ik_http_status_to_category(429), ERR_CAT_RATE_LIMIT);
    ck_assert_int_eq(ik_http_status_to_category(500), ERR_CAT_SERVICE);
    ck_assert_int_eq(ik_http_status_to_category(502), ERR_CAT_SERVICE);
    ck_assert_int_eq(ik_http_status_to_category(503), ERR_CAT_SERVICE);
    ck_assert_int_eq(ik_http_status_to_category(400), ERR_CAT_INVALID_REQUEST);
}
END_TEST

START_TEST(test_error_user_message)
{
    const char *msg = ik_error_user_message(ERR_CAT_RATE_LIMIT);
    ck_assert_ptr_nonnull(msg);
    ck_assert(strstr(msg, "rate") != NULL || strstr(msg, "limit") != NULL);
}
END_TEST
```

**Credentials Tests (`test_credentials.c`):**

```c
START_TEST(test_credentials_from_env_openai)
{
    setenv("OPENAI_API_KEY", "sk-test123", 1);

    const char *key = NULL;
    res_t result = ik_credentials_get(test_ctx, "openai", &key);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(key, "sk-test123");

    unsetenv("OPENAI_API_KEY");
}
END_TEST

START_TEST(test_credentials_from_env_anthropic)
{
    setenv("ANTHROPIC_API_KEY", "sk-ant-test", 1);

    const char *key = NULL;
    res_t result = ik_credentials_get(test_ctx, "anthropic", &key);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(key, "sk-ant-test");

    unsetenv("ANTHROPIC_API_KEY");
}
END_TEST

START_TEST(test_credentials_from_env_google)
{
    setenv("GOOGLE_API_KEY", "AIza-test", 1);

    const char *key = NULL;
    res_t result = ik_credentials_get(test_ctx, "google", &key);

    ck_assert(is_ok(&result));
    ck_assert_str_eq(key, "AIza-test");

    unsetenv("GOOGLE_API_KEY");
}
END_TEST

START_TEST(test_credentials_missing_returns_error)
{
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *key = NULL;
    res_t result = ik_credentials_get(test_ctx, "openai", &key);

    ck_assert(is_err(&result));
}
END_TEST

START_TEST(test_credentials_unknown_provider)
{
    const char *key = NULL;
    res_t result = ik_credentials_get(test_ctx, "unknown_provider", &key);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST
```

## Test Scenarios

**Provider Factory (5 tests):**
1. Create OpenAI provider successfully
2. Create Anthropic provider successfully
3. Create Google provider successfully
4. Unknown provider returns error
5. Missing credentials returns error

**Error Handling (4 tests):**
1. Category names are correct
2. Retryable categories identified
3. HTTP status codes mapped correctly
4. User-friendly messages exist

**Credentials (5 tests):**
1. OpenAI key from environment
2. Anthropic key from environment
3. Google key from environment
4. Missing key returns error
5. Unknown provider returns error

## Postconditions

- [ ] `tests/unit/providers/test_provider_factory.c` created with 5 tests
- [ ] `tests/unit/providers/common/test_error_handling.c` created with 4 tests
- [ ] `tests/unit/test_credentials.c` created with 5 tests
- [ ] All tests use environment variables (no hardcoded keys)
- [ ] Tests clean up environment after each test
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] `make check` passes
