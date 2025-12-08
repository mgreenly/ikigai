#include <check.h>
#include "../../../src/vendor/fzy/match.h"

// Test that fzy compiles and links
START_TEST(test_fzy_has_match_basic)
{
	// Test basic matching
	int result = has_match("abc", "abc");
	ck_assert_int_eq(result, 1);
}
END_TEST

START_TEST(test_fzy_has_match_subsequence)
{
	// Test matching a subsequence
	int result = has_match("abc", "aXbXc");
	ck_assert_int_eq(result, 1);
}
END_TEST

START_TEST(test_fzy_has_match_no_match)
{
	// Test when there's no match
	int result = has_match("abc", "def");
	ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_fzy_match_scoring)
{
	// Test match scoring
	double score = match("abc", "abc");
	ck_assert(score > 0);
}
END_TEST

static Suite *fzy_suite(void)
{
	Suite *s;
	TCase *tc_core;

	s = suite_create("FZY");
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_fzy_has_match_basic);
	tcase_add_test(tc_core, test_fzy_has_match_subsequence);
	tcase_add_test(tc_core, test_fzy_has_match_no_match);
	tcase_add_test(tc_core, test_fzy_match_scoring);

	suite_add_tcase(s, tc_core);

	return s;
}

int main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = fzy_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? 0 : 1;
}
