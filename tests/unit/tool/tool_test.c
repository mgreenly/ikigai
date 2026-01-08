#include <check.h>
#include <talloc.h>

#include "../../../src/tool.h"

// Test fixtures
static TALLOC_CTX *ctx = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
}

// Test: ik_tool_add_string_parameter adds parameter correctly
START_TEST(test_tool_add_string_param) {
    // Create yyjson document
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    // Create properties object
    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    ck_assert_ptr_nonnull(properties);

    // Call function under test
    ik_tool_add_string_parameter(doc, properties, "test_param", "Test description");

    // Verify parameter was added
    yyjson_mut_val *param = yyjson_mut_obj_get(properties, "test_param");
    ck_assert_ptr_nonnull(param);
    ck_assert(yyjson_mut_is_obj(param));

    // Verify type field
    yyjson_mut_val *type = yyjson_mut_obj_get(param, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert(yyjson_mut_is_str(type));
    ck_assert_str_eq(yyjson_mut_get_str(type), "string");

    // Verify description field
    yyjson_mut_val *description = yyjson_mut_obj_get(param, "description");
    ck_assert_ptr_nonnull(description);
    ck_assert(yyjson_mut_is_str(description));
    ck_assert_str_eq(yyjson_mut_get_str(description), "Test description");

    yyjson_mut_doc_free(doc);
}
END_TEST

// Test suite
static Suite *tool_suite(void)
{
    Suite *s = suite_create("Tool");

    TCase *tc_helper = tcase_create("Helper Functions");
    tcase_set_timeout(tc_helper, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_helper, setup, teardown);
    tcase_add_test(tc_helper, test_tool_add_string_param);
    suite_add_tcase(s, tc_helper);

    return s;
}

int main(void)
{
    Suite *s = tool_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
