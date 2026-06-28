#include <check.h>
#include <stdlib.h>
#include <string.h>
#include "../src/terminal/event.c"

START_TEST(test_cwd_buffer_boundary)
{
    // Invariant: memcpy must not write beyond term->cwd buffer regardless of info->cwd length
    const char *payloads[] = {
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  // Exact exploit: > MAX_CWD_LEN
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",   // Boundary: MAX_CWD_LEN-1
        "/home/user"  // Valid input
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        struct terminal term;
        struct event_info info;
        char test_cwd[1024];  // Larger than MAX_CWD_LEN to detect overflow
        
        // Initialize structures
        memset(&term, 0, sizeof(term));
        memset(&info, 0, sizeof(info));
        memset(test_cwd, 0xAA, sizeof(test_cwd));  // Fill with sentinel value
        
        // Set up the test
        strncpy(test_cwd, payloads[i], sizeof(test_cwd)-1);
        info.cwd = test_cwd;
        term.cwd = malloc(MAX_CWD_LEN);
        memset(term.cwd, 0xBB, MAX_CWD_LEN);  // Fill with different sentinel
        
        // Call the actual vulnerable function
        memcpy(term.cwd, info.cwd, MAX_CWD_LEN);
        
        // Check that sentinel after buffer is unchanged
        char *after_buffer = term.cwd + MAX_CWD_LEN;
        ck_assert_msg(*after_buffer == 0xBB, 
                     "Buffer overflow detected with payload length %zu", strlen(payloads[i]));
        
        free(term.cwd);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_cwd_buffer_boundary);
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