#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../src/config.h"
#include "../../src/error.h"
#include "../test_utils.h"

START_TEST(test_config_full_flow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Use a test directory in /tmp
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_integration_%d", getpid());
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "%s/config.json", test_dir);

    // Clean up if it exists from previous run
    unlink(test_config);
    rmdir(test_dir);

    // First call: config doesn't exist, should create defaults
    ik_result_t result1 = ik_cfg_load(ctx, test_config);
    ck_assert(!result1.is_err);

    ik_cfg_t *cfg1 = result1.ok;
    ck_assert_ptr_nonnull(cfg1);
    ck_assert_str_eq(cfg1->openai_api_key, "YOUR_API_KEY_HERE");
    ck_assert_str_eq(cfg1->listen_address, "127.0.0.1");
    ck_assert_int_eq(cfg1->listen_port, 1984);

    // Verify file was created
    struct stat st;
    ck_assert_int_eq(stat(test_config, &st), 0);
    ck_assert(S_ISREG(st.st_mode));

    // Second call: config exists, should load the same defaults
    ik_result_t result2 = ik_cfg_load(ctx, test_config);
    ck_assert(!result2.is_err);

    ik_cfg_t *cfg2 = result2.ok;
    ck_assert_ptr_nonnull(cfg2);
    ck_assert_str_eq(cfg2->openai_api_key, "YOUR_API_KEY_HERE");
    ck_assert_str_eq(cfg2->listen_address, "127.0.0.1");
    ck_assert_int_eq(cfg2->listen_port, 1984);

    // Modify the file with custom values
    FILE *f = fopen(test_config, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\n");
    fprintf(f, "  \"openai_api_key\": \"custom_key_123\",\n");
    fprintf(f, "  \"listen_address\": \"0.0.0.0\",\n");
    fprintf(f, "  \"listen_port\": 3000\n");
    fprintf(f, "}\n");
    fclose(f);

    // Third call: should load modified values
    ik_result_t result3 = ik_cfg_load(ctx, test_config);
    ck_assert(!result3.is_err);

    ik_cfg_t *cfg3 = result3.ok;
    ck_assert_ptr_nonnull(cfg3);
    ck_assert_str_eq(cfg3->openai_api_key, "custom_key_123");
    ck_assert_str_eq(cfg3->listen_address, "0.0.0.0");
    ck_assert_int_eq(cfg3->listen_port, 3000);

    // Clean up
    unlink(test_config);
    rmdir(test_dir);
    talloc_free(ctx);
}

END_TEST Suite *config_integration_suite(void)
{
    Suite *s = suite_create("ConfigIntegration");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_config_full_flow);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = config_integration_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
