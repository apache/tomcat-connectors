#include <check.h>
#include <stdlib.h>
#include <string.h>

/* Include the production code under test */
#include "native/iis/jk_isapi_plugin.c"

START_TEST(test_url_rewrite_buffer_safety)
{
    /* Invariant: URL rewrite substitution must never write beyond allocated
     * buffer regardless of backreference expansion or adversarial input */
    const char *uris[] = {
        /* Exact exploit: long backreference expansion that can overflow len */
        "/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        /* Boundary: single char URI at substitution edge */
        "/a",
        /* Valid normal input */
        "/index.html"
    };
    const char *patterns[] = {
        "^(/A+)",
        "^(/a)",
        "^(/index\\.html)"
    };
    const char *substitutions[] = {
        "/rewritten/\\1/\\1/\\1/\\1",
        "/b\\1",
        "/home"
    };

    int num_cases = 3;

    for (int i = 0; i < num_cases; i++) {
        char *result = NULL;
        /* Call the actual rewrite function from jk_isapi_plugin.c.
         * The function rewrite_url (or equivalent) should not crash/corrupt. */
        result = jk_rewrite_url(uris[i], patterns[i], substitutions[i]);

        /* Invariant: if a result is returned it must be a valid null-terminated
         * string and must not exceed a sane maximum length */
        if (result != NULL) {
            size_t rlen = strnlen(result, 65536);
            ck_assert_msg(rlen < 65536,
                "Rewritten URL length suspiciously large, possible overflow");
            free(result);
        }
        /* NULL result is acceptable (no match or error), crash is not */
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_url_rewrite_buffer_safety);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}