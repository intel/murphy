/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
