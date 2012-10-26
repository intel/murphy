#include <check.h>

#ifndef LOGFILE
#define LOGFILE  "check_libmdb.log"
#endif

#define ADD_TEST_CASE(s,t)                      \
    do {                                        \
        TCase *tc = tcase_create(#t);           \
        tcase_add_test(tc, t);                  \
        suite_add_tcase(s, tc);                 \
    } while (0)


static Suite *libmdb_suite(void);


int main()
{
    Suite   *s  = libmdb_suite();
    SRunner *sr = srunner_create(s);
    int      nf;

    srunner_set_log(sr, LOGFILE);

    srunner_run_all(sr, CK_NORMAL);

    nf = srunner_ntests_failed(sr);

    srunner_free(sr);
    // suite_free(s);

    return (nf == 0) ? 0 : 1;
}

START_TEST(create_table)
{
    fail_unless(1==1, "create table test");
}
END_TEST


static Suite *libmdb_suite(void)
{
    Suite *s = suite_create("Memory Database - libmdb");

    ADD_TEST_CASE(s, create_table);

    return s;
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
