#include "apps/ikigai/config.h"
#include "apps/ikigai/paths.h"

#include "shared/error.h"
#include "vendor/yyjson/yyjson.h"
#include "shared/wrapper.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

START_TEST(test_config_with_db_fields) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with individual database fields
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with individual database fields
    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50,\n"
            "  \"max_output_size\": 1048576,\n"
            "  \"db_host\": \"testhost\",\n"
            "  \"db_port\": 5433,\n"
            "  \"db_name\": \"testdb\",\n"
            "  \"db_user\": \"testuser\"\n"
            "}\n");
    fclose(f);

    // Load config
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    ck_assert_ptr_nonnull(cfg->db_host);
    ck_assert_str_eq(cfg->db_host, "testhost");
    ck_assert_int_eq(cfg->db_port, 5433);
    ck_assert_ptr_nonnull(cfg->db_name);
    ck_assert_str_eq(cfg->db_name, "testdb");
    ck_assert_ptr_nonnull(cfg->db_user);
    ck_assert_str_eq(cfg->db_user, "testuser");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

START_TEST(test_config_without_db_fields) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file without database fields
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config without database fields
    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50,\n"
            "  \"max_output_size\": 1048576\n"
            "}\n");
    fclose(f);

    // Load config - should succeed with defaults
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    // Should have defaults
    ck_assert_ptr_nonnull(cfg->db_host);
    ck_assert_str_eq(cfg->db_host, "localhost");
    ck_assert_int_eq(cfg->db_port, 5432);
    ck_assert_ptr_nonnull(cfg->db_name);
    ck_assert_str_eq(cfg->db_name, "ikigai");
    ck_assert_ptr_nonnull(cfg->db_user);
    ck_assert_str_eq(cfg->db_user, "ikigai");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_partial_db_fields) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with some database fields
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with only db_host and db_port
    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50,\n"
            "  \"max_output_size\": 1048576,\n"
            "  \"db_host\": \"customhost\",\n"
            "  \"db_port\": 9999\n"
            "}\n");
    fclose(f);

    // Load config
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    // Custom values
    ck_assert_ptr_nonnull(cfg->db_host);
    ck_assert_str_eq(cfg->db_host, "customhost");
    ck_assert_int_eq(cfg->db_port, 9999);
    // Defaults for missing fields
    ck_assert_ptr_nonnull(cfg->db_name);
    ck_assert_str_eq(cfg->db_name, "ikigai");
    ck_assert_ptr_nonnull(cfg->db_user);
    ck_assert_str_eq(cfg->db_user, "ikigai");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_empty_db_fields) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with empty database fields
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with empty strings
    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50,\n"
            "  \"max_output_size\": 1048576,\n"
            "  \"db_host\": \"\",\n"
            "  \"db_name\": \"\",\n"
            "  \"db_user\": \"\"\n"
            "}\n");
    fclose(f);

    // Load config - empty strings should fall back to defaults
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    // Empty strings should be replaced with defaults
    ck_assert_ptr_nonnull(cfg->db_host);
    ck_assert_str_eq(cfg->db_host, "localhost");
    ck_assert_int_eq(cfg->db_port, 5432);
    ck_assert_ptr_nonnull(cfg->db_name);
    ck_assert_str_eq(cfg->db_name, "ikigai");
    ck_assert_ptr_nonnull(cfg->db_user);
    ck_assert_str_eq(cfg->db_user, "ikigai");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_with_explicit_null_db_fields) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create a test config file with explicit null database fields
    const char *config_dir = ik_paths_get_config_dir(paths);
    char *test_config = talloc_asprintf(ctx, "%s/config.json", config_dir);

    // Write config with explicit nulls
    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n"
            "  \"openai_model\": \"gpt-5-mini\",\n"
            "  \"openai_temperature\": 1.0,\n"
            "  \"openai_max_completion_tokens\": 4096,\n"
            "  \"openai_system_message\": null,\n"
            "  \"listen_address\": \"127.0.0.1\",\n"
            "  \"listen_port\": 1984,\n"
            "  \"max_tool_turns\": 50,\n"
            "  \"max_output_size\": 1048576,\n"
            "  \"db_host\": null,\n"
            "  \"db_port\": null,\n"
            "  \"db_name\": null,\n"
            "  \"db_user\": null\n"
            "}\n");
    fclose(f);

    // Load config - should succeed with defaults
    ik_config_t *cfg = NULL;

    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(!result.is_err);
    ck_assert_ptr_nonnull(cfg);
    // Null values should be replaced with defaults
    ck_assert_ptr_nonnull(cfg->db_host);
    ck_assert_str_eq(cfg->db_host, "localhost");
    ck_assert_int_eq(cfg->db_port, 5432);
    ck_assert_ptr_nonnull(cfg->db_name);
    ck_assert_str_eq(cfg->db_name, "ikigai");
    ck_assert_ptr_nonnull(cfg->db_user);
    ck_assert_str_eq(cfg->db_user, "ikigai");

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

START_TEST(test_config_structure_has_db_fields) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Test that we can directly access the database fields
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Fields should be NULL/0 by default
    ck_assert_ptr_null(cfg->db_host);
    ck_assert_int_eq(cfg->db_port, 0);
    ck_assert_ptr_null(cfg->db_name);
    ck_assert_ptr_null(cfg->db_user);

    // We can assign to them
    cfg->db_host = talloc_strdup(cfg, "testhost");
    cfg->db_port = 5433;
    cfg->db_name = talloc_strdup(cfg, "testdb");
    cfg->db_user = talloc_strdup(cfg, "testuser");

    ck_assert_ptr_nonnull(cfg->db_host);
    ck_assert_str_eq(cfg->db_host, "testhost");
    ck_assert_int_eq(cfg->db_port, 5433);
    ck_assert_ptr_nonnull(cfg->db_name);
    ck_assert_str_eq(cfg->db_name, "testdb");
    ck_assert_ptr_nonnull(cfg->db_user);
    ck_assert_str_eq(cfg->db_user, "testuser");

    talloc_free(ctx);
}

END_TEST

static Suite *config_suite(void)
{
    Suite *s = suite_create("Config Database Fields");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_config_with_db_fields);
    tcase_add_test(tc_core, test_config_without_db_fields);
    tcase_add_test(tc_core, test_config_with_partial_db_fields);
    tcase_add_test(tc_core, test_config_with_empty_db_fields);
    tcase_add_test(tc_core, test_config_with_explicit_null_db_fields);
    tcase_add_test(tc_core, test_config_structure_has_db_fields);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = config_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/config/config_db_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
