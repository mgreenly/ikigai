#include "apps/ikigai/agent.h"
#include "apps/ikigai/shared.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;
static ik_shared_ctx_t *shared;
static ik_agent_ctx_t *agent;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    shared = talloc_zero(test_ctx, ik_shared_ctx_t);

    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
    shared = NULL;
    agent = NULL;
}

/* Test: agent starts with empty loaded_skills */
START_TEST(test_loaded_skills_initial_state) {
    ck_assert_ptr_null(agent->loaded_skills);
    ck_assert(agent->loaded_skill_count == 0);
}
END_TEST

/* Test: ik_loaded_skill_t can be allocated with agent as parent */
START_TEST(test_loaded_skill_allocate) {
    ik_loaded_skill_t *skill = talloc_zero(agent, ik_loaded_skill_t);
    ck_assert_ptr_nonnull(skill);

    skill->name = talloc_strdup(skill, "database");
    skill->content = talloc_strdup(skill, "# Database Skill\n\nContent here.");
    skill->load_position = agent->message_count;

    ck_assert_str_eq(skill->name, "database");
    ck_assert_str_eq(skill->content, "# Database Skill\n\nContent here.");
    ck_assert(skill->load_position == 0);
}
END_TEST

/* Test: loaded_skills array can be grown and skill added */
START_TEST(test_loaded_skills_add_entry) {
    ik_loaded_skill_t *skill = talloc_zero(agent, ik_loaded_skill_t);
    ck_assert_ptr_nonnull(skill);
    skill->name = talloc_strdup(skill, "database");
    skill->content = talloc_strdup(skill, "# Database");
    skill->load_position = 0;

    agent->loaded_skills = talloc_realloc(agent, agent->loaded_skills,
                                          ik_loaded_skill_t *,
                                          (unsigned int)(agent->loaded_skill_count + 1));
    ck_assert_ptr_nonnull(agent->loaded_skills);
    agent->loaded_skills[agent->loaded_skill_count] = skill;
    agent->loaded_skill_count++;

    ck_assert(agent->loaded_skill_count == 1);
    ck_assert_ptr_eq(agent->loaded_skills[0], skill);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "database");
}
END_TEST

/* Test: multiple skills can be added */
START_TEST(test_loaded_skills_multiple_entries) {
    const char *names[] = {"database", "errors", "style"};

    for (size_t i = 0; i < 3; i++) {
        ik_loaded_skill_t *skill = talloc_zero(agent, ik_loaded_skill_t);
        ck_assert_ptr_nonnull(skill);
        skill->name = talloc_strdup(skill, names[i]);
        skill->content = talloc_strdup(skill, "content");
        skill->load_position = i;

        agent->loaded_skills = talloc_realloc(agent, agent->loaded_skills,
                                              ik_loaded_skill_t *,
                                              (unsigned int)(agent->loaded_skill_count + 1));
        ck_assert_ptr_nonnull(agent->loaded_skills);
        agent->loaded_skills[agent->loaded_skill_count] = skill;
        agent->loaded_skill_count++;
    }

    ck_assert(agent->loaded_skill_count == 3);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "database");
    ck_assert_str_eq(agent->loaded_skills[1]->name, "errors");
    ck_assert_str_eq(agent->loaded_skills[2]->name, "style");
    ck_assert(agent->loaded_skills[2]->load_position == 2);
}
END_TEST

static Suite *loaded_skills_suite(void)
{
    Suite *s = suite_create("Loaded Skills");

    TCase *tc = tcase_create("Core");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_loaded_skills_initial_state);
    tcase_add_test(tc, test_loaded_skill_allocate);
    tcase_add_test(tc, test_loaded_skills_add_entry);
    tcase_add_test(tc, test_loaded_skills_multiple_entries);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = loaded_skills_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/agent/loaded_skills_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
