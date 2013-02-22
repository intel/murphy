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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include <check.h>

#include <murphy-db/mqi.h>
#include <murphy-db/mql.h>


#ifndef LOGFILE
#define LOGFILE  "check_libmql.log"
#endif

#define PREREQUISITE(t)   t(_i)

#define TRIGGER_DATA(idx) (void *)0xdeadbeef##idx
#define TRANSACT_TRIGGER_DATA TRIGGER_DATA(1)
#define TABLE_TRIGGER_DATA    TRIGGER_DATA(2)
#define ROW_TRIGGER_DATA      TRIGGER_DATA(3)
#define COLUMN_TRIGGER_DATA   TRIGGER_DATA(4)


typedef struct {
    const char  *sex;
    const char  *first_name;
    const char  *family_name;
    uint32_t     id;
    const char  *email;
} record_t;

static mqi_column_def_t persons_columns[] = {
    {"sex"        , mqi_varchar,  6, 0},
    {"family_name", mqi_varchar, 12, 0},
    {"first_name" , mqi_varchar, 12, 0},
    {"id"         , mqi_unsignd,  4, 0},
    {"email"      , mqi_varchar, 24, 0}
};
static int persons_ncolumn = MQI_DIMENSION(persons_columns);

static record_t persons_rows[] = {
    {"male"  , "Chuck", "Norris" , 1100, "cno@texas.us"  },
    {"male"  , "Gary", "Cooper"  ,  700, "gco@heaven.org"},
    {"male"  , "Elvis", "Presley",  600, "epr@heaven.org"},
    {"male"  , "Tom", "Cruise"   ,  500, "tcr@foo.com"   },
    {"female", "Greta", "Garbo"  , 2000, "gga@heaven.org"},
    {"female", "Rita", "Hayworth",   44, "rha@heaven.org"}
};
static int persons_nrow = MQI_DIMENSION(persons_rows);


static int verbose;
static struct {
    mql_statement_t *begin;
    mql_statement_t *commit;
    mql_statement_t *rollback;
    mql_statement_t *filtered_select;
    mql_statement_t *full_select;
    mql_statement_t *update;
    mql_statement_t *delete;
    mql_statement_t *insert;
} persons;


static Suite *libmql_suite(void);
static TCase *basic_tests(void);

static void transaction_event_cb(mql_result_t *, void *);
static void table_event_cb(mql_result_t *, void *);
static void row_event_cb(mql_result_t *, void *);
static void column_event_cb(mql_result_t *, void *);



int main(int argc, char **argv)
{
    Suite   *s  = libmql_suite();
    SRunner *sr = srunner_create(s);
    int      nf;
    int      i;

    for (i = 1;  i < argc;  i++) {
        if (!strcmp("-v", argv[i]))
            verbose = 1;
        else if (!strcmp("-f", argv[i]))
            srunner_set_fork_status(sr, CK_NOFORK);
        else {
            printf("Usage: %s [-h] [-v] [-f]\n"
                   "  -h  prints this message\n"
                   "  -v  sets verbose mode\n"
                   "  -f  forces no-forking mode\n",
                   basename(argv[0]));
            exit(strcmp("-h", argv[i]) ? 1 : 0);
        }
    }

    srunner_set_log(sr, LOGFILE);

    srunner_run_all(sr, CK_NORMAL);

    nf = srunner_ntests_failed(sr);

    srunner_free(sr);
    // suite_free(s);

    return (nf == 0) ? 0 : 1;
}


START_TEST(open_db)
{
    int sts = mqi_open();

    fail_if(sts, "db open test");
}
END_TEST



START_TEST(create_table_persons)
{
    mql_result_t *r;

    PREREQUISITE(open_db);

    r = mql_exec_string(mql_result_string,
                        "CREATE TEMPORARY TABLE persons ("
                        "   sex          VARCHAR(6), "
                        "   family_name  VARCHAR(12),"
                        "   first_name   VARCHAR(12),"
                        "   id           UNSIGNED,   "
                        "   email        VARCHAR(24) "
                        ")"
    );

    fail_unless(mql_result_is_success(r), "error: %s",
                mql_result_error_get_message(r));

    mql_result_free(r);
}
END_TEST



START_TEST(describe_persons)
{
    mql_result_type_t rt = verbose ? mql_result_string : mql_result_columns;
    mqi_column_def_t *cd;
    mql_result_t *r;
    mqi_data_type_t type;
    const char *name;
    int length;
    int i,n;

    PREREQUISITE(create_table_persons);

    r = mql_exec_string(rt, "DESCRIBE persons");

    fail_unless(mql_result_is_success(r), "error: %s",
                mql_result_error_get_message(r));

    if (verbose)
        printf("%s\n", mql_result_string_get(r));
    else {
        n = mql_result_columns_get_column_count(r);

        fail_if(n  < 1, "invalid column count %d", n);
        fail_if(n != persons_ncolumn, "coulumn count is %d but "
                "it supposed to be %d", n, persons_ncolumn);

        for (i = 0;   i < n;   i++) {
            cd = persons_columns + i;
            name = mql_result_columns_get_name(r, i);
            type = mql_result_columns_get_type(r, i);
            length = mql_result_columns_get_length(r, i);

            fail_if(strcmp(name, cd->name), "column%d name mismatch "
                    "('%s' vs. '%s')", i, cd->name, name);

            fail_if(type != cd->type, "column%d type mismatch (%s vs. %s)",
                    i, mqi_data_type_str(cd->type), mqi_data_type_str(type));

            fail_if(length != cd->length, "column%d length mismatch "
                    "(%d vs. %d)", i, cd->length, length);
        }
    }

    mql_result_free(r);
}
END_TEST




START_TEST(create_index_on_persons)
{
    static bool done;

    mql_result_t *r;

    if (!done) {
        PREREQUISITE(create_table_persons);

        r = mql_exec_string(mql_result_string,
                    "CREATE INDEX ON persons (family_name, first_name)");

        fail_unless(mql_result_is_success(r), "error: %s",
                    mql_result_error_get_message(r));

        done = true;
    }
}
END_TEST



START_TEST(insert_into_persons)
{
    mql_result_t *r;
    record_t *p;
    char statement[512];
    int i;

    PREREQUISITE(create_index_on_persons);

    for (i = 0;  i < persons_nrow;  i++) {
        p = persons_rows + i;

        snprintf(statement, sizeof(statement),
                 "INSERT INTO persons VALUES ('%s', '%s', '%s', %u, '%s')",
                 p->sex, p->family_name, p->first_name, p->id, p->email);

        r = mql_exec_string(mql_result_string, statement);

        fail_unless(mql_result_is_success(r), "error @ row%d: %s",
                    i, mql_result_error_get_message(r));
    }
}
END_TEST


START_TEST(make_persons)
{
    static int done;

    if (!done) {
        PREREQUISITE(insert_into_persons);
        done = 1;
    }
}
END_TEST

START_TEST(precompile_transaction_statements)
{
#define TRID "transaction_1"

    static char *string[] = {
        "BEGIN "    TRID,
        "COMMIT "   TRID,
        "ROLLBACK " TRID
    };

    static mql_statement_t **stmnt[] = {
        &persons.begin,
        &persons.commit,
        &persons.rollback
    };

    static int done;

    int i;

    if (!done) {
        fail_unless(MQI_DIMENSION(string) == MQI_DIMENSION(stmnt),
                    "internal error: dimension mismatch in %s()", __FILE__);

        for (i = 0;  i < (int)MQI_DIMENSION(string);  i++) {
            if (!(*(stmnt[i]) = mql_precompile(string[i]))) {
                fail("precompilation error of '%s' (%s)",
                     string[i], strerror(errno));
            }
        }
    }

#undef TRID
}
END_TEST


START_TEST(precompile_filtered_person_select)
{
    mql_statement_t *stmnt;

    PREREQUISITE(make_persons);

    stmnt = mql_precompile("SELECT id, first_name, family_name FROM persons"
                           " WHERE id > %u & id <= %u");

    fail_if(!stmnt, "precompilation error (%s)", strerror(errno));

    persons.filtered_select = stmnt;
}
END_TEST



START_TEST(precompile_full_person_select)
{
    mql_statement_t *stmnt;

    PREREQUISITE(make_persons);

    if (!persons.full_select) {
        stmnt = mql_precompile("SELECT id, first_name, family_name"
                               " FROM persons");

        fail_if(!stmnt, "precompilation error (%s)", strerror(errno));

        persons.full_select = stmnt;
    }
}
END_TEST



START_TEST(precompile_update_persons)
{
    mql_statement_t *stmnt;

    PREREQUISITE(make_persons);

    if (!persons.update) {
        stmnt = mql_precompile("UPDATE persons "
                               "  SET family_name = %s,"
                               "      first_name  = %s"
                               "  WHERE id = %u");

        fail_if(!stmnt, "precompilation error (%s)", strerror(errno));

        persons.update = stmnt;
    }
}
END_TEST



START_TEST(precompile_delete_from_persons)
{
    mql_statement_t *stmnt;

    PREREQUISITE(make_persons);

    if (!persons.delete) {
        stmnt = mql_precompile("DELETE FROM persons WHERE family_name = %s");

        fail_if(!stmnt, "precompilation error (%s)", strerror(errno));

        persons.delete = stmnt;
    }
}
END_TEST



START_TEST(precompile_insert_into_persons)
{
    mql_statement_t *stmnt;

    PREREQUISITE(make_persons);

    if (!persons.insert) {
        stmnt = mql_precompile("INSERT INTO persons VALUES ("
                               " 'male', 'Baltzar','Veijo', 855, 'vba@pdf.org'"
                               ")");

        fail_if(!stmnt, "precompilation error (%s)", strerror(errno));

        persons.insert = stmnt;
    }
}
END_TEST



START_TEST(exec_precompiled_filtered_select_from_persons)
{
    mql_result_type_t rt = verbose ? mql_result_string : mql_result_rows;
    mql_result_t *r;
    int n;

    PREREQUISITE(precompile_filtered_person_select);

    if (mql_bind_value(persons.filtered_select, 1, mqi_unsignd,  200) < 0 ||
        mql_bind_value(persons.filtered_select, 2, mqi_unsignd, 1100) < 0  )
    {
        fail("bind error (%s)", strerror(errno));
    }

    r = mql_exec_statement(rt, persons.filtered_select);

    fail_unless(mql_result_is_success(r), "exec error: %s",
                mql_result_error_get_message(r));

    if (verbose)
        printf("%s\n", mql_result_string_get(r));
    else {
        if ((n = mql_result_rows_get_row_count(r)) != 4)
            fail("row number mismatch (4 vs. %d)", n);
    }

    mql_result_free(r);

    mql_statement_free(persons.filtered_select);
    persons.filtered_select = NULL;
}
END_TEST



START_TEST(exec_precompiled_full_select_from_persons)
{
    mql_result_type_t rt = verbose ? mql_result_string : mql_result_rows;
    mql_result_t *r;
    int n;

    PREREQUISITE(precompile_full_person_select);

    r = mql_exec_statement(rt, persons.full_select);

    fail_unless(mql_result_is_success(r), "exec error: %s",
                mql_result_error_get_message(r));

    if (verbose)
        printf("%s\n", mql_result_string_get(r));
    else {
        if ((n = mql_result_rows_get_row_count(r)) != persons_nrow)
            fail("row number mismatch (%d vs. %d)", persons_nrow, n);
    }

    mql_result_free(r);

    mql_statement_free(persons.full_select);
    persons.full_select = NULL;
}
END_TEST

START_TEST(exec_precompiled_update_persons)
{
    static uint32_t    id         = 2000;
    static const char *new_first  = "Marilyn";
    static const char *new_family = "Monroe";

    PREREQUISITE(precompile_update_persons);

    mql_result_type_t rt = verbose ? mql_result_string : mql_result_rows;
    mql_result_t *r;
    record_t *p;
    const char *first;
    const char *family;
    int updated;
    int i, n;

    /* 2000: Greta Garbo => Marilyn Monroe */
    if (mql_bind_value(persons.update, 1, mqi_string , new_family) < 0 ||
        mql_bind_value(persons.update, 2, mqi_string ,  new_first) < 0 ||
        mql_bind_value(persons.update, 3, mqi_unsignd,         id) < 0   )
    {
        fail("bind error (%s)", strerror(errno));
    }


    r = mql_exec_statement(mql_result_string, persons.update);

    fail_unless(mql_result_is_success(r), "exec error: %s",
                mql_result_error_get_message(r));

    mql_result_free(r);

    /* verification */
    r = mql_exec_string(rt, "SELECT id, first_name, family_name FROM persons");

    fail_unless(mql_result_is_success(r), "exec error @ verifying select: %s",
                mql_result_error_get_message(r));

    if (verbose)
        printf("%s\n", mql_result_string_get(r));
    else {
        for (p = NULL, i = 0;  i < persons_nrow;  i++) {
            if (persons_rows[i].id == id) {
                p = persons_rows + i;
                break;
            }
        }

        n = mql_result_rows_get_row_count(r);

        for (updated = 0, i = 0;   i < n;   i++) {
            family = mql_result_rows_get_string(r, 1, i, NULL,0);
            first  = mql_result_rows_get_string(r, 2, i, NULL,0);

            if (p) {
                fail_if(!strcmp(first, p->first_name), "found original "
                        "first name '%s'", p->first_name);
                fail_if(!strcmp(family, p->family_name), "found original "
                        "family name '%s'", p->family_name);
            }
            else {
                fail_if(!strcmp(first, new_first), "found new "
                        "first name '%s'", first);
                fail_if(!strcmp(family, new_family), "found new "
                        "family name '%s'", family);
            }

            if (id == mql_result_rows_get_unsigned(r, 0, i)) {
                if (strcmp(first, new_first) || strcmp(family, new_family)) {
                    updated = 1;
                }
            }
        }

        if (p)
            fail_unless(updated, "result is success but no actual update");
        else
            fail_unless(!updated, "update happened but it not supposed to");
    }

    mql_result_free(r);


    mql_statement_free(persons.update);
    persons.update = NULL;
}
END_TEST


START_TEST(exec_precompiled_delete_from_persons)
{
    const char *del_family = "Cruise";

    mql_result_type_t rt = verbose ? mql_result_string : mql_result_rows;
    mql_result_t *r;
    record_t *p;
    uint32_t id;
    const char *first;
    const char *family;
    int i,n;

    PREREQUISITE(precompile_delete_from_persons);

    /* delete Tom Cruise */
    if (mql_bind_value(persons.delete, 1, mqi_string , del_family) < 0)
        fail("bind error (%s)", strerror(errno));

    r = mql_exec_statement(mql_result_string, persons.delete);

    fail_unless(mql_result_is_success(r), "exec error: %s",
                mql_result_error_get_message(r));

    mql_result_free(r);


    /* verification */
    r = mql_exec_string(rt, "SELECT id, first_name, family_name FROM persons");

    fail_unless(mql_result_is_success(r), "exec error @ verifying select: %s",
                mql_result_error_get_message(r));

    if (verbose)
        printf("%s\n", mql_result_string_get(r));
    else {
        for (p = NULL, i = 0;  i < persons_nrow;  i++) {
            if (!strcmp(persons_rows[i].family_name, del_family)) {
                p = persons_rows + i;
                break;
            }
        }

        n = mql_result_rows_get_row_count(r);

        for (i = 0;   i < n;   i++) {
            id     = mql_result_rows_get_unsigned(r, 0, i);
            first  = mql_result_rows_get_string(r, 1, i, NULL,0);
            family = mql_result_rows_get_string(r, 2, i, NULL,0);

            if (p) {
                /* supposed to be deleted */
                fail_if(id == p->id, "found id %u of the presumably "
                        "deleted row", id);
                fail_if(!strcmp(first, p->first_name), "found first name '%s' "
                        "of the presumably deleted row", first);
                fail_if(!strcmp(family, p->family_name), "found family name "
                        "'%s' of the presumably deleted row", family);
            }
            else {
                /* nothing supposed to be deleted */
                fail_if(!strcmp(family, del_family), "found family name '%s'"
                        "what not supposed to be there", family);
            }
        }
   }

    mql_result_free(r);

    mql_statement_free(persons.delete);
    persons.delete = NULL;
}
END_TEST



START_TEST(exec_precompiled_insert_into_persons)
{
    mql_result_type_t rt = verbose ? mql_result_string : mql_result_rows;
    mql_result_t *r;
    record_t *p;
    const char *first;
    const char *family;
    int inserted;
    int i,n;

    PREREQUISITE(precompile_insert_into_persons);


    for (p = NULL, i = 0;  i < persons_nrow;  i++) {
        if (!strcmp(persons_rows[i].family_name, "Baltzar") &&
            !strcmp(persons_rows[i].first_name ,   "Veijo")   )
        {
            p = persons_rows + i;
            break;
        }
    }

    /* insert Veijo Baltzar */
    r = mql_exec_statement(mql_result_string, persons.insert);

    if (p)
        fail_if(mql_result_is_success(r), "manage to insert a duplicate");
    else {
        fail_unless(mql_result_is_success(r), "exec error: %s",
                    mql_result_error_get_message(r));
    }

    mql_result_free(r);


    /* verification */
    r = mql_exec_string(rt, "SELECT id, first_name, family_name FROM persons");

    fail_unless(mql_result_is_success(r), "exec error @ verifying select: %s",
                mql_result_error_get_message(r));

    if (verbose)
        printf("%s\n", mql_result_string_get(r));
    else {
        if (!p) {
            n = mql_result_rows_get_row_count(r);

            for (inserted = 0, i = 0;   i < n;   i++) {
                first  = mql_result_rows_get_string(r, 1, i, NULL,0);
                family = mql_result_rows_get_string(r, 2, i, NULL,0);

                if (!strcmp(first, "Veijo") && !strcmp(family, "Baltzar")) {
                    inserted = 1;
                    break;
                }
            }

            fail_unless(inserted, "Veijo does not seem to be an the artist");
        }
    }




    mql_result_free(r);


    mql_statement_free(persons.insert);
    persons.insert = NULL;
}
END_TEST

START_TEST(register_transaction_event_cb)
{
    int sts;

    PREREQUISITE(open_db);

    sts = mql_register_callback("transaction_event_cb", mql_result_string,
                                transaction_event_cb, TRANSACT_TRIGGER_DATA);

    fail_if(sts < 0, "failed to create 'table_event_cb': %s",
            strerror(errno));
}
END_TEST


START_TEST(register_table_event_cb)
{
    int sts;

    sts = mql_register_callback("table_event_cb", mql_result_string,
                                table_event_cb, TABLE_TRIGGER_DATA);

    fail_if(sts < 0, "failed to create 'table_event_cb': %s",
            strerror(errno));
}
END_TEST

START_TEST(register_row_event_cb)
{
    int sts;

    PREREQUISITE(make_persons);

    sts = mql_register_callback("row_event_cb", mql_result_string,
                                row_event_cb, ROW_TRIGGER_DATA);

    fail_if(sts < 0, "failed to create 'row_event_cb': %s",
            strerror(errno));
}
END_TEST

START_TEST(register_column_event_cb)
{
    int sts;

    PREREQUISITE(make_persons);

    sts = mql_register_callback("column_event_cb", mql_result_string,
                                column_event_cb, COLUMN_TRIGGER_DATA);

    fail_if(sts < 0, "failed to create 'column_event_cb': %s",
            strerror(errno));
}
END_TEST

START_TEST(table_trigger)
{
    static char *mqlstr = "CREATE TRIGGER table_trigger"
                          " ON TABLES CALLBACK table_event_cb";

    mql_result_t *r;

    PREREQUISITE(open_db);
    PREREQUISITE(register_table_event_cb);

    r = mql_exec_string(mql_result_dontcare, mqlstr);

    fail_unless(mql_result_is_success(r),"failed to exec '%s': (%d) %s",mqlstr,
                mql_result_error_get_code(r), mql_result_error_get_message(r));

    PREREQUISITE(make_persons);
}
END_TEST

START_TEST(row_trigger)
{
    static char *mqlstr = "CREATE TRIGGER row_trigger"
                          " ON ROWS IN persons"
                          " CALLBACK row_event_cb"
                          " SELECT id, first_name, family_name";


    mql_result_t *r;

    PREREQUISITE(register_row_event_cb);
    PREREQUISITE(precompile_transaction_statements);

    r = mql_exec_statement(mql_result_string, persons.begin);

    fail_unless(mql_result_is_success(r), "failed to begin transaction: %s",
                strerror(errno));

    r = mql_exec_string(mql_result_dontcare, mqlstr);

    fail_unless(mql_result_is_success(r),"failed to exec '%s': (%d) %s",mqlstr,
                mql_result_error_get_code(r), mql_result_error_get_message(r));

    PREREQUISITE(exec_precompiled_insert_into_persons);
    PREREQUISITE(exec_precompiled_delete_from_persons);

    r = mql_exec_statement(mql_result_string, persons.commit);

    fail_unless(mql_result_is_success(r), "failed to commit transaction: %s",
                strerror(errno));
}
END_TEST

START_TEST(column_trigger)
{
    static char *mqlstr = "CREATE TRIGGER column_trigger"
                          " ON COLUMN first_name IN persons"
                          " CALLBACK column_event_cb"
                          " SELECT id, first_name, family_name";

    mql_result_t *r;

    PREREQUISITE(register_column_event_cb);
    PREREQUISITE(precompile_transaction_statements);

    r = mql_exec_statement(mql_result_string, persons.begin);

    fail_unless(mql_result_is_success(r), "failed to begin transaction: %s",
                strerror(errno));

    r = mql_exec_string(mql_result_dontcare, mqlstr);

    fail_unless(mql_result_is_success(r),"failed to exec '%s': (%d) %s",mqlstr,
                mql_result_error_get_code(r), mql_result_error_get_message(r));

    PREREQUISITE(exec_precompiled_update_persons);

    r = mql_exec_statement(mql_result_string, persons.commit);

    fail_unless(mql_result_is_success(r), "failed to commit transaction: %s",
                strerror(errno));
}
END_TEST


START_TEST(transaction_trigger)
{
    static char *mqlstr = "CREATE TRIGGER transaction_trigger ON TRANSACTIONS"
                          " CALLBACK transaction_event_cb";

    mql_result_t *r;

    PREREQUISITE(register_transaction_event_cb);

    r = mql_exec_string(mql_result_dontcare, mqlstr);

    fail_unless(mql_result_is_success(r),"failed to exec '%s': (%d) %s",mqlstr,
                mql_result_error_get_code(r), mql_result_error_get_message(r));

    PREREQUISITE(column_trigger);
}
END_TEST

static Suite *libmql_suite(void)
{
    Suite *s = suite_create("Murphy Query Language - libmql");
    TCase *tc_basic = basic_tests();

    suite_add_tcase(s, tc_basic);

    return s;
}


static TCase *basic_tests(void)
{
    TCase *tc = tcase_create("basic tests");

    tcase_add_test(tc, open_db);
    tcase_add_test(tc, create_table_persons);
    tcase_add_test(tc, describe_persons);
    tcase_add_test(tc, create_index_on_persons);
    tcase_add_test(tc, insert_into_persons);
    tcase_add_test(tc, precompile_transaction_statements);
    tcase_add_test(tc, precompile_filtered_person_select);
    tcase_add_test(tc, precompile_full_person_select);
    tcase_add_test(tc, precompile_update_persons);
    tcase_add_test(tc, precompile_delete_from_persons);
    tcase_add_test(tc, precompile_insert_into_persons);
    tcase_add_test(tc, exec_precompiled_filtered_select_from_persons);
    tcase_add_test(tc, exec_precompiled_full_select_from_persons);
    tcase_add_test(tc, exec_precompiled_update_persons);
    tcase_add_test(tc, exec_precompiled_delete_from_persons);
    tcase_add_test(tc, exec_precompiled_insert_into_persons);
    tcase_add_test(tc, register_transaction_event_cb);
    tcase_add_test(tc, register_table_event_cb);
    tcase_add_test(tc, register_row_event_cb);
    tcase_add_test(tc, register_column_event_cb);
    tcase_add_test(tc, table_trigger);
    tcase_add_test(tc, row_trigger);
    tcase_add_test(tc, column_trigger);
    tcase_add_test(tc, transaction_trigger);

    return tc;
}


static void transaction_event_cb(mql_result_t *result, void *user_data)
{
    MQI_UNUSED(user_data);

    if (result->type == mql_result_string) {
        if (verbose)
            printf("---\n%s\n", mql_result_string_get(result));
    }
    else if (result->type == mql_result_event) {
    }
    else {
        if (verbose)
            printf("%s: invalid result type %d\n", __FUNCTION__, result->type);
    }
}

static void table_event_cb(mql_result_t *result, void *user_data)
{
    MQI_UNUSED(user_data);

    if (result->type == mql_result_string) {
        if (verbose)
            printf("---\n%s\n", mql_result_string_get(result));
    }
    else if (result->type == mql_result_event) {
    }
    else {
        if (verbose)
            printf("%s: invalid result type %d\n", __FUNCTION__, result->type);
    }
}

static void row_event_cb(mql_result_t *result, void *user_data)
{
    MQI_UNUSED(user_data);

    if (result->type == mql_result_string) {
        if (verbose)
            printf("---\n%s\n", mql_result_string_get(result));
    }
    else if (result->type == mql_result_event) {
    }
    else {
        if (verbose)
            printf("%s: invalid result type %d\n", __FUNCTION__, result->type);
    }
}

static void column_event_cb(mql_result_t *result, void *user_data)
{
    MQI_UNUSED(user_data);

    if (result->type == mql_result_string) {
        if (verbose)
            printf("---\n%s\n", mql_result_string_get(result));
    }
    else if (result->type == mql_result_event) {
    }
    else {
        if (verbose)
            printf("%s: invalid result type %d\n", __FUNCTION__, result->type);
    }
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
