#include "../../test_constants.h"

#include <check.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "../../../src/vendor/yyjson/yyjson.h"

static TALLOC_CTX *test_ctx;
static const char *tool_path = "libexec/ikigai/web-search-google-tool";

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}


static char *run_tool_with_output(const char *input, int32_t *exit_code)
{
    char temp_file[] = "/tmp/web_search_google_test_XXXXXX";
    int32_t fd = mkstemp(temp_file);
    if (fd == -1) {
        return NULL;
    }

    if (input != NULL && strlen(input) > 0) {
        if (write(fd, input, strlen(input)) == -1) {
            close(fd);
            unlink(temp_file);
            return NULL;
        }
    }
    close(fd);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s < %s 2>&1", tool_path, temp_file);

    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        unlink(temp_file);
        return NULL;
    }

    char *output = talloc_zero_array(test_ctx, char, 65536);
    size_t total = 0;
    size_t n;
    while ((n = fread(output + total, 1, 4096, fp)) > 0) {
        total += n;
    }

    int32_t status = pclose(fp);
    if (exit_code != NULL) {
        *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    unlink(temp_file);
    return output;
}

START_TEST(test_schema_flag) {

    FILE *fp = popen("libexec/ikigai/web-search-google-tool --schema", "r");
    ck_assert_ptr_nonnull(fp);

    char schema_output[16384] = {0};
    size_t total = 0;
    size_t n;
    while ((n = fread(schema_output + total, 1, 4096, fp)) > 0) {
        total += n;
    }

    int32_t schema_status = pclose(fp);
    int32_t schema_exit_code = WIFEXITED(schema_status) ? WEXITSTATUS(schema_status) : -1;

    ck_assert_int_eq(schema_exit_code, 0);

    yyjson_doc *doc = yyjson_read(schema_output, strlen(schema_output), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *name = yyjson_obj_get(root, "name");
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(yyjson_get_str(name), "web_search_google");

    yyjson_val *description = yyjson_obj_get(root, "description");
    ck_assert_ptr_nonnull(description);
    const char *desc_str = yyjson_get_str(description);
    ck_assert(strstr(desc_str, "Google Custom Search API") != NULL);

    yyjson_val *parameters = yyjson_obj_get(root, "parameters");
    ck_assert_ptr_nonnull(parameters);

    yyjson_val *properties = yyjson_obj_get(parameters, "properties");
    ck_assert_ptr_nonnull(properties);

    yyjson_val *query = yyjson_obj_get(properties, "query");
    ck_assert_ptr_nonnull(query);

    yyjson_val *num = yyjson_obj_get(properties, "num");
    ck_assert_ptr_nonnull(num);

    yyjson_val *start = yyjson_obj_get(properties, "start");
    ck_assert_ptr_nonnull(start);

    yyjson_val *allowed_domains = yyjson_obj_get(properties, "allowed_domains");
    ck_assert_ptr_nonnull(allowed_domains);

    yyjson_val *blocked_domains = yyjson_obj_get(properties, "blocked_domains");
    ck_assert_ptr_nonnull(blocked_domains);

    yyjson_val *required = yyjson_obj_get(parameters, "required");
    ck_assert_ptr_nonnull(required);
    ck_assert(yyjson_is_arr(required));

    size_t idx, max;
    yyjson_val *val;
    bool found_query = false;
    yyjson_arr_foreach(required, idx, max, val) {
        if (strcmp(yyjson_get_str(val), "query") == 0) {
            found_query = true;
        }
    }
    ck_assert(found_query);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_empty_stdin) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    int32_t exit_code = -1;
    run_tool_with_output("", &exit_code);

    ck_assert_int_eq(exit_code, 1);
}

END_TEST

START_TEST(test_invalid_json) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    int32_t exit_code = -1;
    run_tool_with_output("not json", &exit_code);

    ck_assert_int_eq(exit_code, 1);
}

END_TEST

START_TEST(test_missing_both_credentials) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    const char *home = getenv("HOME");
    char cred_path[512];
    snprintf(cred_path, sizeof(cred_path), "%s/.config/ikigai/credentials.json", home);
    char backup_path[1024];
    snprintf(backup_path, sizeof(backup_path), "%s.backup", cred_path);

    struct stat st;
    bool had_credentials = (stat(cred_path, &st) == 0);
    if (had_credentials) {
        rename(cred_path, backup_path);
    }

    int32_t exit_code = -1;
    char *output = run_tool_with_output("{\"query\":\"test\"}", &exit_code);

    if (had_credentials) {
        rename(backup_path, cred_path);
    }

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    yyjson_doc *doc = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(!yyjson_get_bool(success));

    yyjson_val *error_code = yyjson_obj_get(root, "error_code");
    ck_assert_ptr_nonnull(error_code);
    ck_assert_str_eq(yyjson_get_str(error_code), "AUTH_MISSING");

    yyjson_val *event = yyjson_obj_get(root, "_event");
    ck_assert_ptr_nonnull(event);

    yyjson_val *kind = yyjson_obj_get(event, "kind");
    ck_assert_ptr_nonnull(kind);
    ck_assert_str_eq(yyjson_get_str(kind), "config_required");

    yyjson_val *content = yyjson_obj_get(event, "content");
    ck_assert_ptr_nonnull(content);
    const char *content_str = yyjson_get_str(content);
    ck_assert(strstr(content_str, "Configuration Required") != NULL);
    ck_assert(strstr(content_str, "api_key") != NULL);
    ck_assert(strstr(content_str, "engine_id") != NULL);

    yyjson_val *data = yyjson_obj_get(event, "data");
    ck_assert_ptr_nonnull(data);

    yyjson_val *tool = yyjson_obj_get(data, "tool");
    ck_assert_ptr_nonnull(tool);
    ck_assert_str_eq(yyjson_get_str(tool), "web_search_google");

    yyjson_val *credentials = yyjson_obj_get(data, "credentials");
    ck_assert_ptr_nonnull(credentials);
    ck_assert(yyjson_is_arr(credentials));

    bool found_api_key = false;
    bool found_engine_id = false;
    size_t idx, max;
    yyjson_val *val;
    yyjson_arr_foreach(credentials, idx, max, val) {
        const char *cred = yyjson_get_str(val);
        if (strcmp(cred, "api_key") == 0) {
            found_api_key = true;
        }
        if (strcmp(cred, "engine_id") == 0) {
            found_engine_id = true;
        }
    }
    ck_assert(found_api_key);
    ck_assert(found_engine_id);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_missing_api_key_only) {
    unsetenv("GOOGLE_SEARCH_API_KEY");
    setenv("GOOGLE_SEARCH_ENGINE_ID", "test-engine-id", 1);

    int32_t exit_code = -1;
    char *output = run_tool_with_output("{\"query\":\"test\"}", &exit_code);

    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    yyjson_doc *doc = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(!yyjson_get_bool(success));

    yyjson_val *error_code = yyjson_obj_get(root, "error_code");
    ck_assert_ptr_nonnull(error_code);
    ck_assert_str_eq(yyjson_get_str(error_code), "AUTH_MISSING");

    yyjson_val *event = yyjson_obj_get(root, "_event");
    ck_assert_ptr_nonnull(event);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_missing_engine_id_only) {
    setenv("GOOGLE_SEARCH_API_KEY", "test-api-key", 1);
    unsetenv("GOOGLE_SEARCH_ENGINE_ID");

    int32_t exit_code = -1;
    char *output = run_tool_with_output("{\"query\":\"test\"}", &exit_code);

    unsetenv("GOOGLE_SEARCH_API_KEY");

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    yyjson_doc *doc = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(!yyjson_get_bool(success));

    yyjson_val *error_code = yyjson_obj_get(root, "error_code");
    ck_assert_ptr_nonnull(error_code);
    ck_assert_str_eq(yyjson_get_str(error_code), "AUTH_MISSING");

    yyjson_val *event = yyjson_obj_get(root, "_event");
    ck_assert_ptr_nonnull(event);

    yyjson_doc_free(doc);
}

END_TEST

static Suite *web_search_google_suite(void)
{
    Suite *s = suite_create("WebSearchGoogle");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_schema_flag);
    tcase_add_test(tc_core, test_empty_stdin);
    tcase_add_test(tc_core, test_invalid_json);
    tcase_add_test(tc_core, test_missing_both_credentials);
    tcase_add_test(tc_core, test_missing_api_key_only);
    tcase_add_test(tc_core, test_missing_engine_id_only);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = web_search_google_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
