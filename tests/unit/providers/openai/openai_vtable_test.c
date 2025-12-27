/**
 * @file openai_vtable_test.c
 * @brief Unit tests for OpenAI provider vtable methods
 */

#include <check.h>
#include <talloc.h>
#include <sys/select.h>
#include "providers/common/http_multi.h"
#include "providers/openai/openai.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "wrapper.h"

static TALLOC_CTX *test_ctx;
static ik_provider_t *provider;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

static void setup_provider(void)
{
    test_ctx = talloc_new(NULL);
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(provider);
}

static void teardown_provider(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Provider Creation Tests
 * ================================================================ */

START_TEST(test_create_with_empty_api_key_fails) {
    ik_provider_t *p = NULL;
    res_t r = ik_openai_create(test_ctx, "", &p);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(p);
}
END_TEST START_TEST(test_create_with_options_responses_api)
{
    ik_provider_t *p = NULL;
    res_t r = ik_openai_create_with_options(test_ctx, "sk-test", true, &p);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(p);
    ck_assert_str_eq(p->name, "openai");
}

END_TEST START_TEST(test_create_with_options_chat_api)
{
    ik_provider_t *p = NULL;
    res_t r = ik_openai_create_with_options(test_ctx, "sk-test", false, &p);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(p);
    ck_assert_str_eq(p->name, "openai");
}

END_TEST
/* ================================================================
 * Vtable Method Tests
 * ================================================================ */

START_TEST(test_cleanup_method)
{
    // cleanup is a void function that currently does nothing
    // but we should still call it to ensure coverage
    provider->vt->cleanup(provider->ctx);

    // If we get here without crashing, the test passed
    ck_assert(1);
}

END_TEST START_TEST(test_cancel_method)
{
    // cancel is a void function that currently does nothing
    // but we should still call it to ensure coverage
    provider->vt->cancel(provider->ctx);

    // If we get here without crashing, the test passed
    ck_assert(1);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_vtable_suite(void)
{
    Suite *s = suite_create("OpenAI Vtable");

    TCase *tc_create = tcase_create("Provider Creation");
    tcase_add_unchecked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_create_with_empty_api_key_fails);
    tcase_add_test(tc_create, test_create_with_options_responses_api);
    tcase_add_test(tc_create, test_create_with_options_chat_api);
    suite_add_tcase(s, tc_create);

    TCase *tc_vtable = tcase_create("Vtable Methods");
    tcase_add_unchecked_fixture(tc_vtable, setup_provider, teardown_provider);
    tcase_add_test(tc_vtable, test_cleanup_method);
    tcase_add_test(tc_vtable, test_cancel_method);
    suite_add_tcase(s, tc_vtable);

    return s;
}

int main(void)
{
    Suite *s = openai_vtable_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
