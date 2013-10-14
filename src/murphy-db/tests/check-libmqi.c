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

#ifndef LOGFILE
#define LOGFILE  "check_libmqi.log"
#endif

#define PREREQUISITE(t)   t(_i)

#define TRIGGER_DATA(idx)   (void *)0xdeadbeef##idx
#define TRANSACT_TRIGGER_DATA TRIGGER_DATA(1)
#define TABLE_TRIGGER_DATA    TRIGGER_DATA(2)
#define ROW_TRIGGER_DATA      TRIGGER_DATA(3)
#define COLUMN_TRIGGER_DATA   TRIGGER_DATA(4)

typedef struct {
    mqi_event_type_t  event;
    struct {
        mqi_handle_t handle;
        char name[256];
    } table;
    struct {
        uint32_t id;
        char     first_name[14];
        char     family_name[14];
    } row;
    struct {
        int index;
        char name[14];
        char value[32];
    } col;
} trigger_t;

typedef struct {
    const char  *sex;
    const char  *first_name;
    const char  *family_name;
    uint32_t     id;
    const char  *email;
} record_t;

typedef struct {
    uint32_t       id;
    const char    *family_name;
    const char    *first_name;
} query_t;


MQI_COLUMN_DEFINITION_LIST(persons_coldefs,
    MQI_COLUMN_DEFINITION( "sex"        , MQI_VARCHAR(6)  ),
    MQI_COLUMN_DEFINITION( "family_name", MQI_VARCHAR(12) ),
    MQI_COLUMN_DEFINITION( "first_name" , MQI_VARCHAR(12) ),
    MQI_COLUMN_DEFINITION( "id"         , MQI_UNSIGNED    ),
    MQI_COLUMN_DEFINITION( "email"      , MQI_VARCHAR(24) )
);

MQI_INDEX_DEFINITION(persons_indexdef,
    MQI_INDEX_COLUMN("first_name")
    MQI_INDEX_COLUMN("family_name")
);

MQI_COLUMN_SELECTION_LIST(persons_insert_columns,
    MQI_COLUMN_SELECTOR( 0, record_t, sex         ),
    MQI_COLUMN_SELECTOR( 2, record_t, first_name  ),
    MQI_COLUMN_SELECTOR( 1, record_t, family_name ),
    MQI_COLUMN_SELECTOR( 3, record_t, id          ),
    MQI_COLUMN_SELECTOR( 4, record_t, email       )
);

MQI_COLUMN_SELECTION_LIST(persons_select_columns,
    MQI_COLUMN_SELECTOR( 3, query_t, id         ),
    MQI_COLUMN_SELECTOR( 1, query_t, family_name ),
    MQI_COLUMN_SELECTOR( 2, query_t, first_name  )
);

static record_t chuck = {"male"  , "Chuck", "Norris" , 1100, "cno@texas.us"  };
static record_t gary  = {"male"  , "Gary", "Cooper"  ,  700, "gco@heaven.org"};
static record_t elvis = {"male"  , "Elvis", "Presley",  600, "epr@heaven.org"};
static record_t tom   = {"male"  , "Tom", "Cruise"   ,  500, "tcr@foo.com"   };
static record_t greta = {"female", "Greta", "Garbo"  , 2000, "gga@heaven.org"};
static record_t rita  = {"female", "Rita", "Hayworth",   44, "rha@heaven.org"};

static record_t *artists[] = {&chuck, &gary, &elvis, &tom, &greta, &rita,NULL};



static int          verbose;
static mqi_handle_t transactions[MQI_TXDEPTH_MAX - 1];
static int          txdepth;
static mqi_handle_t persons = MQI_HANDLE_INVALID;
static int          columns_no_in_persons = -1;
static int          rows_no_in_persons = -1;

static int          ntrigger;
static trigger_t    triggers[256];
static int          nseq = 32;
static int          nnest = MQI_TXDEPTH_MAX - 1;


static Suite *libmqi_suite(void);
static TCase *basic_tests(void);
static void   print_rows(int, query_t *);
static void   print_triggers(void);
static void   transaction_event_cb(mqi_event_t *, void *);
static void   table_event_cb(mqi_event_t *, void *);
static void   row_event_cb(mqi_event_t *, void *);
static void   column_event_cb(mqi_event_t *, void *);


int main(int argc, char **argv)
{
    Suite   *s  = libmqi_suite();
    SRunner *sr = srunner_create(s);
    int      nf;
    int      i;

    for (i = 1;  i < argc;  i++) {
        if (!strcmp("-v", argv[i]))
            verbose = 1;
        else if (!strcmp("-f", argv[i]))
            srunner_set_fork_status(sr, CK_NOFORK);
        else if (!strcmp("-nseq", argv[i]) && i < argc - 1) {
            nseq = atoi(argv[i + 1]);
            i++;
        }
        else if (!strcmp("-nnest", argv[i]) && i < argc - 1) {
            nnest = atoi(argv[i + 1]);
            i++;
        }
        else {
            printf("Usage: %s [-h] [-v] [-f]\n"
                   "  -h     prints this message\n"
                   "  -v     sets verbose mode\n"
                   "  -f     forces no-forking mode\n"
                   "  -nseq  number of sequential transactions\n"
                   "  -nnest number of nested transactions (1 - %d)\n",
                   basename(argv[0]), MQI_TXDEPTH_MAX - 1);
            exit(strcmp("-h", argv[i]) ? 1 : 0);
        }
    }

    srunner_set_log(sr, LOGFILE);

    srunner_run_all(sr, CK_NORMAL);

    nf = srunner_ntests_failed(sr);

    srunner_free(sr);

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
    if (persons == MQI_HANDLE_INVALID) {
        PREREQUISITE(open_db);

        persons = MQI_CREATE_TABLE("persons", MQI_TEMPORARY,
                                   persons_coldefs, persons_indexdef);

        fail_if(persons == MQI_HANDLE_INVALID, "errno (%s)", strerror(errno));

        columns_no_in_persons = MQI_DIMENSION(persons_coldefs) - 1;
    }
}
END_TEST



START_TEST(table_handle)
{
    mqi_handle_t handle = MQI_HANDLE_INVALID;

    PREREQUISITE(create_table_persons);

    handle = mqi_get_table_handle("persons");

    fail_if(handle == MQI_HANDLE_INVALID, "failed to obtain handle for "
            "'persons' (%s)", strerror(errno));

    fail_if(handle != persons, "handle mismatch (0x%x vs. 0x%x)",
            persons, handle);
}
END_TEST


START_TEST(describe_persons)
{
    mqi_column_def_t  cols[32];
    mqi_column_def_t *def, *col;
    int deflgh;
    int i,ncolumn;

    PREREQUISITE(create_table_persons);

    ncolumn = MQI_DESCRIBE(persons, cols);

    fail_if(ncolumn < 0, "errno (%s)", strerror(errno));

    fail_if(ncolumn != columns_no_in_persons, "mismatching column number "
            "(%d vs. %d)", columns_no_in_persons, ncolumn);

    if (verbose) {
        printf("-----------------------------\n");
        printf("name         type      length\n");
        printf("-----------------------------\n");
        for (i = 0;  i < ncolumn;  i++) {
            col = cols + i;
            printf("%-12s %-9s     %2d\n", col->name,
                   mqi_data_type_str(col->type), col->length);
        }
        printf("-----------------------------\n");
    }

    for (i = 0;  i < ncolumn;  i++) {
        def = persons_coldefs + i;
        col = cols + i;

        fail_if(strcmp(def->name, col->name), "mismatching column names @ "
                "column %d ('%s' vs. '%s')", i, def->name, col->name);

        fail_if(def->type != col->type, "mismatching column types @ "
                "column %d (%d/'%s' vs. %d/'%s')", i,
                def->type, mqi_data_type_str(def->type),
                col->type, mqi_data_type_str(col->type));

        switch (def->type) {
        case mqi_varchar:   deflgh = def->length;       break;
        case mqi_integer:   deflgh = sizeof(int32_t);   break;
        case mqi_unsignd:   deflgh = sizeof(uint32_t);  break;
        case mqi_floating:  deflgh = sizeof(double);    break;
        case mqi_blob:      deflgh = def->length;       break;
        default:            deflgh = -1;                break;
        };

        fail_if(deflgh != col->length, "mismatching column length @ "
                "column %d (%d vs. %d)", i, deflgh, col->length);
    }
}
END_TEST


START_TEST(insert_into_persons)
{
    int n;

    PREREQUISITE(create_table_persons);

    n = MQI_INSERT_INTO(persons, persons_insert_columns, artists);

    fail_if(n < 0, "errno (%s)", strerror(errno));

    fail_if(n != MQI_DIMENSION(artists)-1, "some insertion failed. "
            "Attempted %d succeeded %d", MQI_DIMENSION(artists)-1, n);

    rows_no_in_persons = n;
}
END_TEST


START_TEST(row_count_in_persons)
{
    int n;

    PREREQUISITE(insert_into_persons);

    n = mqi_get_table_size(persons);

    fail_if(n < 0, "error (%s)", strerror(errno));

    fail_if(n != rows_no_in_persons, "mismatch in row numbers: "
            "Inserted %d reported %d", rows_no_in_persons, n);
}
END_TEST

START_TEST(insert_duplicate_into_persons)
{
    static record_t  gary = {"male", "Gary","Cooper", 200, "gary@att.com"};
    static record_t *duplicate[] = {&gary, NULL};

    int n;

    PREREQUISITE(insert_into_persons);

    n = MQI_INSERT_INTO(persons, persons_insert_columns, duplicate);

    fail_if(n == 1, "managed to insert a duplicate");

    fail_if(n < 0 && errno != EEXIST, "error (%s)", strerror(errno));
}
END_TEST

START_TEST(transaction_begin)
{
    mqi_handle_t tx;

    fail_if(txdepth >= (int)MQI_DIMENSION(transactions), "too many nested "
            "transactions. Only %d allowed", MQI_DIMENSION(transactions));

    tx = MQI_BEGIN;

    fail_if(tx == MQI_HANDLE_INVALID, "error (%d)", strerror(errno));

    transactions[txdepth++] = tx;
}
END_TEST


START_TEST(replace_in_persons)
{
    static record_t  gary = {"male", "Gary","Cooper", 200, "gary@att.com"};
    static record_t *duplicate[] = {&gary, NULL};

    int n;

    PREREQUISITE(insert_into_persons);
    PREREQUISITE(transaction_begin);

    n = MQI_REPLACE(persons, persons_insert_columns, duplicate);

    fail_if(n < 0, "error (%s)", strerror(errno));

    fail_if(n == 1, "duplicate was inserted instead of replacement");
}
END_TEST

START_TEST(filtered_select_from_persons)
{
    static char     *initial = "G";
    static uint32_t  idlimit = 200;

    MQI_WHERE_CLAUSE(where,
        MQI_GREATER( MQI_COLUMN(1), MQI_STRING_VAR(initial)   ) MQI_AND
        MQI_GREATER( MQI_COLUMN(3), MQI_UNSIGNED_VAR(idlimit) )
    );

    query_t rows[32];
    int n;

    PREREQUISITE(replace_in_persons);

    n = MQI_SELECT(persons_select_columns, persons, where, rows);

    fail_if(n < 0, "error (%s)", strerror(errno));

    if (verbose)
        print_rows(n, rows);

    fail_if(n != 3, "selcted %d rows but the right number would be 3", n);
}
END_TEST


START_TEST(full_select_from_persons)
{
    query_t *r, rows[32];
    int i, n;

    PREREQUISITE(replace_in_persons);

    n = MQI_SELECT(persons_select_columns, persons, MQI_ALL, rows);

    fail_if(n < 0, "error (%s)", strerror(errno));

    if (verbose) {
        printf("   id first name      family name     \n");
        printf("--------------------------------------\n");

        if (!n)
            printf("no rows\n");
        else {
            for (i = 0; i < n;  i++) {
                r = rows + i;
                printf("%5d %-15s %-15s\n", r->id,
                       r->first_name, r->family_name);
            }
        }

        printf("--------------------------------------\n");
    }

    fail_if(n != 6, "selcted %d rows but the right number would be 3", n);
}
END_TEST



START_TEST(select_from_persons_by_index)
{
    MQI_INDEX_VALUE(index,
        MQI_STRING_VAL(elvis.family_name)
        MQI_STRING_VAL(elvis.first_name)
    );

    query_t row;
    int n;

    PREREQUISITE(replace_in_persons);

    n = MQI_SELECT_BY_INDEX(persons_select_columns, persons, index, &row);

    fail_if(n < 0, "errno (%s)", strerror(errno));

    fail_if(!n, "could not select %s %s", elvis.first_name, elvis.family_name);

    fail_if(strcmp(row.first_name, elvis.first_name), "mismatching first "
            "name ('%s' vs. '%s')", elvis.first_name, row.first_name);

    fail_if(strcmp(row.family_name, elvis.family_name), "mismatching family "
            "name ('%s' vs. '%s')", elvis.family_name, row.family_name);

    fail_if(row.id != elvis.id, "mismatching id (%u vs. %u)",
            elvis.id, row.id);
}
END_TEST



START_TEST(update_in_persons)
{
    MQI_WHERE_CLAUSE(where,
        MQI_EQUAL( MQI_COLUMN(1), MQI_STRING_VAR(elvis.family_name) ) MQI_AND
        MQI_EQUAL( MQI_COLUMN(2), MQI_STRING_VAR(elvis.first_name ) )
    );

    static query_t kalle = {1, "Korhonen", "Kalle"};

    query_t *r, rows[32];
    int i,n;
    int found;

    PREREQUISITE(replace_in_persons);

    n = MQI_UPDATE(persons, persons_select_columns, &kalle, where);

    fail_if(n  < 0, "errno (%s)", strerror(errno));
    fail_if(n != 1, "updated %d row but supposed to just 1", n);

    n = MQI_SELECT(persons_select_columns, persons, MQI_ALL, rows);

    fail_if(n < 0, "select for checking failed (%s)", strerror(errno));

    if (verbose)
        print_rows(n, rows);

    for (found = 0, i = 0;  i < n;  i++) {
        r = rows + i;

        fail_if(r->id == elvis.id, "found the original id %u what supposed "
                "to change to %u", elvis.id, kalle.id);

        fail_if(!strcmp(r->first_name, elvis.first_name), "found the original "
                "first name '%s' what supposed to change to '%s'",
                elvis.first_name, kalle.first_name);

        fail_if(!strcmp(r->family_name, elvis.family_name),"found the original"
                " family name '%s' what supposed to change to '%s'",
                elvis.family_name, kalle.family_name);

        if (r->id == kalle.id &&
            !strcmp(r->first_name, kalle.first_name) &&
            !strcmp(r->family_name, kalle.family_name))
        {
            found = 1;
        }
    }

    fail_unless(found, "could not find the updated row");
}
END_TEST



START_TEST(delete_from_persons)
{
    static uint32_t idlimit = 200;

    MQI_WHERE_CLAUSE(where,
        MQI_LESS( MQI_COLUMN(3), MQI_UNSIGNED_VAR(idlimit) )
    );

    query_t *r, rows[32];
    int i,n;

    PREREQUISITE(update_in_persons);

    n = MQI_DELETE(persons, where);

    fail_if(n  < 0, "errno (%s)", strerror(errno));
    fail_if(n != 2, "deleted %d rows but sopposed to 2", n);

    n = MQI_SELECT(persons_select_columns, persons, MQI_ALL, rows);

    fail_if(n < 0, "verification select failed (%s)", strerror(errno));

    if (verbose)
        print_rows(n, rows);

    for (i = 0;  i < n;  i++) {
        r = rows + i;

        fail_if(r->id < idlimit, "found row with id %u what is smaller than "
                "the limit %u", r->id, idlimit);
    }
}
END_TEST


START_TEST(delete_all_persons)
{
    query_t rows[32];
    int     nrow, n;

    nrow = MQI_SELECT(persons_select_columns, persons, MQI_ALL, rows);
    fail_if(nrow < 0, "select for checking failed (%s)", strerror(errno));

    n = MQI_DELETE(persons, MQI_ALL);
    fail_if(n != nrow, "deleted %d rows instead of the expected %d", n, nrow);

    n = MQI_SELECT(persons_select_columns, persons, MQI_ALL, rows);
    fail_if(n != 0, "verification select failed (%s)", strerror(errno));
}
END_TEST


START_TEST(transaction_rollback)
{
    record_t *a;
    query_t *r, rows[32];
    int i,j,n;
    int sts;
    int found;

    PREREQUISITE(delete_from_persons);

    fail_unless(txdepth > 0, "actually there is no transaction");

    sts = MQI_ROLLBACK(transactions[--txdepth]);

    fail_if(sts < 0, "errno (%s)", strerror(errno));

    n = MQI_SELECT(persons_select_columns, persons, MQI_ALL, rows);

    fail_if(n < 0, "verification select failed (%s)", strerror(errno));

    if (verbose)
        print_rows(n, rows);

    fail_if(n != MQI_DIMENSION(artists)-1, "mismatching row numbers: currently"
            " %d supposed to be %d", n, MQI_DIMENSION(artists)-1);


    for (i = 0;  i < n;  i++) {
        r = rows + i;

        for (found = 0, j = 0;  j < (int)MQI_DIMENSION(artists)-1;  j++) {
            a = artists[j];

            if (a->id == r->id &&
                !strcmp(a->first_name, r->first_name) &&
                !strcmp(a->family_name, r->family_name))
            {
                found = 1;
                break;
            }
        }

        fail_unless(found, "after rolling back can't find %s %s (id %u) "
                    "any more", r->first_name, r->family_name, r->id);
    }
}
END_TEST

START_TEST(table_trigger)
{
    int sts;

    PREREQUISITE(open_db);

    sts = mqi_create_table_trigger(table_event_cb, TABLE_TRIGGER_DATA);

    fail_if(sts < 0, "errno (%s)", strerror(errno));

    PREREQUISITE(create_table_persons);

    if (verbose)
        print_triggers();

    fail_unless(ntrigger == 1, "no callback after table creation");
    fail_unless(triggers->event == mqi_table_created,
                "wrong event type %d", triggers->event);
    fail_unless(triggers->table.handle == persons,
                "wrong table handle (0x%x vs. 0x%x)",
                triggers->table.handle, persons);
    fail_unless(!strcmp(triggers->table.name, "persons"),
                "wrong table name ('%s' vs. 'persons')",
                triggers->table.name);
}
END_TEST

START_TEST(row_trigger)
{
    mqi_handle_t trh;
    record_t *rec;
    trigger_t *trig;
    int sts;
    int i;

    PREREQUISITE(create_table_persons);

    sts = mqi_create_transaction_trigger(transaction_event_cb,
                                         TRANSACT_TRIGGER_DATA);

    fail_if(sts < 0, "create transaction trigger failed: errno (%s)",
            strerror(errno));

    sts = mqi_create_row_trigger(persons, row_event_cb, ROW_TRIGGER_DATA,
                                 persons_select_columns);

    fail_if(sts < 0, "create row trigger failed: errno (%s)", strerror(errno));

    trh = mqi_begin_transaction();

    fail_if(trh == MQI_HANDLE_INVALID, "begin failed: errno(%s)",
            strerror(errno));

    PREREQUISITE(insert_into_persons);

    sts = mqi_commit_transaction(trh);

    fail_if(sts < 0, "commit failed: errno (%s)", strerror(errno));

    if (verbose)
        print_triggers();

    fail_unless(ntrigger == rows_no_in_persons + 2,
                "wrong number of callbacks (%d vs. %d)",
                ntrigger, rows_no_in_persons);

    for (i = 0;  i < ntrigger-2;  i++) {
        trig = triggers + (i + 1);
        rec  = artists[i];

        fail_unless(trig->event == mqi_row_inserted,
                    "wrong event type (%d vs %d) @ callback %d",
                    trig->event, mqi_row_inserted, i);
        fail_unless(trig->table.handle == persons,
                    "wrong table handle (0x%x vs. 0x%x) @ callback %d",
                    trig->table.handle, persons, i);
        fail_unless(!strcmp(trig->table.name, "persons"),
                    "wrong table name ('%s' vs. 'persons') @ callback %d",
                    trig->table.name, persons, i);
        fail_unless(trig->row.id == rec->id,
                    "id column mismatch (%d vs %s) @ callback %d",
                    trig->row.id, rec->id, i);
        fail_unless(!strcmp(trig->row.first_name, rec->first_name),
                    "first name mismatch ('%s' vs. '%s') @ callback %d",
                    trig->row.first_name, rec->first_name);
        fail_unless(!strcmp(trig->row.family_name, rec->family_name),
                    "first name mismatch ('%s' vs. '%s') @ callback %d",
                    trig->row.family_name, rec->family_name);
    }

}
END_TEST



START_TEST(column_trigger)
{
    MQI_WHERE_CLAUSE(where,
        MQI_EQUAL( MQI_COLUMN(1), MQI_STRING_VAR(elvis.family_name) ) MQI_AND
        MQI_EQUAL( MQI_COLUMN(2), MQI_STRING_VAR(elvis.first_name ) )
    );

    static query_t kalle = {1, "Korhonen", "Kalle"};

    mqi_handle_t trh;
    trigger_t *trig;
    int sts;
    int i, n;

    PREREQUISITE(insert_into_persons);

    sts = mqi_create_column_trigger(persons, 1, column_event_cb,
                                    COLUMN_TRIGGER_DATA,
                                    persons_select_columns);

    fail_if(sts < 0, "create column trigger failed: errno (%s)",
            strerror(errno));

    sts = mqi_create_column_trigger(persons, 2, column_event_cb,
                                    COLUMN_TRIGGER_DATA,
                                    persons_select_columns);

    fail_if(sts < 0, "create column trigger failed: errno (%s)",
            strerror(errno));

    trh = mqi_begin_transaction();

    fail_if(trh == MQI_HANDLE_INVALID, "begin failed: errno(%s)",
            strerror(errno));

    n = MQI_UPDATE(persons, persons_select_columns, &kalle, where);

    fail_if(n  < 0, "update failed: errno (%s)", strerror(errno));
    fail_if(n != 1, "updated %d row but supposed to just 1", n);

    sts = mqi_commit_transaction(trh);

    fail_if(sts < 0, "commit failed: errno (%s)", strerror(errno));

    if (verbose)
        print_triggers();

    fail_unless(ntrigger == 2,
                "wrong number of callbacks (%d vs. 2)",
                ntrigger);

    for (i = 0;  i < ntrigger;  i++) {
        trig = triggers + i;

        fail_unless(trig->event == mqi_column_changed,
                    "wrong event type (%d vs %d) @ callback %d",
                    trig->event, mqi_column_changed, i);
        fail_unless(trig->table.handle == persons,
                    "wrong table handle (0x%x vs. 0x%x) @ callback %d",
                    trig->table.handle, persons, i);
        fail_unless(!strcmp(trig->table.name, "persons"),
                    "wrong table name ('%s' vs. 'persons') @ callback %d",
                    trig->table.name, persons, i);
        fail_unless(trig->row.id == kalle.id,
                    "id column mismatch (%d vs %d) @ callback %d",
                    trig->row.id, kalle.id, i);
        fail_unless(!strcmp(trig->row.first_name, kalle.first_name),
                    "first name mismatch ('%s' vs. '%s') @ callback %d",
                    trig->row.first_name, kalle.first_name);
        fail_unless(!strcmp(trig->row.family_name, kalle.family_name),
                    "first name mismatch ('%s' vs. '%s') @ callback %d",
                    trig->row.family_name, kalle.family_name);
    }
}
END_TEST

START_TEST(sequential_transactions)
{
    mqi_handle_t  trh;
    int           sts, i;
    const char   *kind;

    PREREQUISITE(create_table_persons);

    for (i = 0; i < nseq; i++) {
        trh = mqi_begin_transaction();

        fail_if(trh == MQI_HANDLE_INVALID,
                "failed to create %d. transaction : errno (%s)",
                i + 1, strerror(errno));

        if (i & 0x1)
            PREREQUISITE(delete_all_persons);
        else
            PREREQUISITE(insert_into_persons);

        if (!(i & 0x3)) {
            kind = "rollback";
            sts  = mqi_rollback_transaction(trh);
        }
        else {
            kind = "commit";
            sts  = mqi_commit_transaction(trh);
        }

        fail_if(sts < 0, "%s failed: errno (%s)", kind, strerror(errno));
    }
}
END_TEST


START_TEST(nested_transactions)
{
    mqi_handle_t  txids[MQI_TXDEPTH_MAX - 1];
    mqi_handle_t  trh;
    int           sts, tx, i, cnt;
    const char   *kind;

    PREREQUISITE(create_table_persons);

    if (nnest > (int)(sizeof(txids) / sizeof(txids[0])))
        nnest = sizeof(txids) / sizeof(txids[0]);

    for (cnt = 0; cnt < 16; cnt++) {
        for (tx = 0; tx < nnest; tx++) {
            trh = txids[tx] = mqi_begin_transaction();

            fail_if(trh == MQI_HANDLE_INVALID,
                    "couldn't create transaction: errno (%s)", strerror(errno));

            for (i = 0; i < nseq; i++) {
                if (i & 0x1)
                    PREREQUISITE(delete_all_persons);
                else
                    PREREQUISITE(insert_into_persons);
            }
        }

        for (tx = nnest - 1; tx >= 0; tx--) {
            trh = txids[tx];

            if (!(tx & 0x1) && 0) {
                kind = "rollback";
                sts = mqi_rollback_transaction(trh);
            }
            else {
                kind = "commit";
                sts = mqi_commit_transaction(trh);
            }

            fail_if(sts < 0, "%s %u failed: errno (%s)", kind, trh,
                    strerror(errno));
        }
    }
}
END_TEST



static Suite *libmqi_suite(void)
{
    Suite *s = suite_create("Murphy Query Interface - libmqi");
    TCase *tc_basic = basic_tests();

    suite_add_tcase(s, tc_basic);

    return s;
}

static TCase *basic_tests(void)
{
    TCase *tc = tcase_create("basic tests");

    tcase_add_test(tc, open_db);
    tcase_add_test(tc, create_table_persons);
    tcase_add_test(tc, table_handle);
    tcase_add_test(tc, describe_persons);
    tcase_add_test(tc, insert_into_persons);
    tcase_add_test(tc, row_count_in_persons);
    tcase_add_test(tc, insert_duplicate_into_persons);
    tcase_add_test(tc, replace_in_persons);
    tcase_add_test(tc, filtered_select_from_persons);
    tcase_add_test(tc, full_select_from_persons);
    tcase_add_test(tc, select_from_persons_by_index);
    tcase_add_test(tc, update_in_persons);
    tcase_add_test(tc, delete_from_persons);
    tcase_add_test(tc, transaction_rollback);
    tcase_add_test(tc, table_trigger);
    tcase_add_test(tc, row_trigger);
    tcase_add_test(tc, column_trigger);
    tcase_add_test(tc, sequential_transactions);
    tcase_add_test(tc, nested_transactions);

    return tc;
}

static void print_rows(int n, query_t *rows)
{
    query_t *r;
    int i;

    printf("   id first name      family name     \n");
    printf("--------------------------------------\n");

    if (!n)
        printf("no rows\n");
    else {
        for (i = 0; i < n;  i++) {
            r = rows + i;
            printf("%5d %-15s %-15s\n", r->id,
                   r->first_name, r->family_name);
        }
    }

    printf("--------------------------------------\n");
}


static void print_triggers(void)
{
    static char *separator = "+---------------+-------------------+"
                             "-------------------------------------+"
                             "--------------------------------------"
                             "--------+\n";
    trigger_t *trig;
    enum {err, tra, tbl, row, col} t;
    char *ev;
    int i;

    printf(separator);
    printf("| trigger       |      table        |"
           "      selected columns in row        |"
           "    altered column                            |\n");
    printf("| event         |  handle name      |"
           "   id first_name      family_name    |"
           " idx name         value                       |\n");
    printf(separator);

    if (!ntrigger) {
        printf("|-<no events>---|-------------------|"
               "-------------------------------------|"
               "----------------------------------------------|\n");
    }
    else {
        for (i = 0;  i < ntrigger;  i++) {
            trig = triggers + i;

            switch (trig->event) {
            case mqi_column_changed:    t = col; ev = "column_changed";  break;
            case mqi_row_inserted:      t = row; ev = "row_inserted";    break;
            case mqi_row_deleted:       t = row; ev = "row_deleted";     break;
            case mqi_table_created:     t = tbl; ev = "table_created";   break;
            case mqi_table_dropped:     t = tbl; ev = "table_dropped";   break;
            case mqi_transaction_start: t = tra; ev = "transact start";  break;
            case mqi_transaction_end:   t = tra; ev = "transact end";    break;
            default:                    t = err; ev = "<unknown>";       break;
            }


            printf("| %-14s", ev);

            if (t == tbl || t == row || t == col)
                printf("|%8x %-10s", trig->table.handle, trig->table.name);
            else
                printf("|                   ");

            if (t == row || t == col)
                printf("|%5d %-15s %-15s", trig->row.id, trig->row.first_name,
                       trig->row.family_name);
            else
                printf("|                                     ");

            if (t == col)
                printf("| %3d %-12s %-28s", trig->col.index, trig->col.name,
                       trig->col.value);
            else
                printf("|                                              ");

            printf("|\n");
        }
    }

    printf(separator);
}

static void transaction_event_cb(mqi_event_t *evt, void *user_data)
{
    mqi_event_type_t      event = evt->event;
 /* mqi_transact_event_t *te    = &evt->transact; */
    trigger_t            *trig;

    if (ntrigger >= (int)MQI_DIMENSION(triggers)) {
        if (verbose)
            printf("test framework error: trigger log overflow\n");
        return;
    }

    trig = triggers + ntrigger++;

    if (event != mqi_transaction_start && event != mqi_transaction_end) {
        if (verbose)
            printf("invalid event %d for transaction trigger\n", event);
        return;
    }

    if (user_data != TRANSACT_TRIGGER_DATA) {
        if (verbose)
            printf("invalid user_data %p for transaction trigger\n",
                   user_data);
        return;
    }

    trig->event = event;
}

static void table_event_cb(mqi_event_t *evt, void *user_data)
{
    mqi_event_type_t   event = evt->event;
    mqi_table_event_t *te    = &evt->table;
    trigger_t         *trig;

    if (ntrigger >= (int)MQI_DIMENSION(triggers)) {
        if (verbose)
            printf("test framework error: trigger log overflow\n");
        return;
    }

    trig = triggers + ntrigger++;

    if (event != mqi_table_created && event != mqi_table_dropped) {
        if (verbose)
            printf("invalid event %d for table trigger\n", event);
        return;
    }

    if (user_data != TABLE_TRIGGER_DATA) {
        if (verbose)
            printf("invalid user_data %p for table trigger\n", user_data);
        return;
    }


    trig->event = event;
    trig->table.handle = te->table.handle;
    strncpy(trig->table.name, te->table.name,
            MQI_DIMENSION(trig->table.name) - 1);
}

static void row_event_cb(mqi_event_t *evt, void *user_data)
{
    mqi_event_type_t  event = evt->event;
    mqi_row_event_t  *re    = &evt->row;
    trigger_t        *trig;
    query_t          *row;

    if (ntrigger >= (int)MQI_DIMENSION(triggers)) {
        if (verbose)
            printf("test framework error: trigger log overflow\n");
        return;
    }

    trig = triggers + ntrigger++;

    if (event != mqi_row_inserted && event != mqi_row_deleted) {
        if (verbose)
            printf("invalid event %d for row trigger\n", event);
        return;
    }

    if (user_data != ROW_TRIGGER_DATA) {
        if (verbose)
            printf("invalid user_data %p for row trigger\n", user_data);
        return;
    }

    if (!(row = (query_t *)re->select.data)) {
        if (verbose)
            printf("no selected data\n");
        return;
    }


    trig->event = event;
    trig->table.handle = re->table.handle;
    strncpy(trig->table.name, re->table.name,
            MQI_DIMENSION(trig->table.name) - 1);
    trig->row.id = row->id;
    strncpy(trig->row.first_name, row->first_name,
            MQI_DIMENSION(trig->row.first_name) - 1);
    strncpy(trig->row.family_name, row->family_name,
            MQI_DIMENSION(trig->row.family_name) - 1);
}

static void column_event_cb(mqi_event_t *evt, void *user_data)
{
    mqi_event_type_t    event = evt->event;
    mqi_column_event_t *ce    = &evt->column;
    trigger_t          *trig;
    query_t            *row;

    if (ntrigger >= (int)MQI_DIMENSION(triggers)) {
        if (verbose)
            printf("test framework error: trigger log overflow\n");
        return;
    }

    trig = triggers + ntrigger++;

    if (event != mqi_column_changed) {
        if (verbose)
            printf("invalid event %d for column trigger\n", event);
        return;
    }

    if (user_data != COLUMN_TRIGGER_DATA) {
        if (verbose)
            printf("invalid user_data %p for column trigger\n", user_data);
        return;
    }

    if (!(row = (query_t *)ce->select.data)) {
        if (verbose)
            printf("no selected data\n");
        return;
    }


    trig->event = event;
    trig->table.handle = ce->table.handle;
    strncpy(trig->table.name, ce->table.name,
            MQI_DIMENSION(trig->table.name) - 1);
    trig->row.id = row->id;
    strncpy(trig->row.first_name, row->first_name,
            MQI_DIMENSION(trig->row.first_name) - 1);
    strncpy(trig->row.family_name, row->family_name,
            MQI_DIMENSION(trig->row.family_name) - 1);
    trig->col.index = ce->column.index;
    strncpy(trig->col.name, ce->column.name,
            MQI_DIMENSION(trig->col.name) - 1);

#define PRINT_VALUE(fmt,t) \
    snprintf(trig->col.value, MQI_DIMENSION(trig->col.value) - 1, \
             fmt " => " fmt, ce->value.old.t, ce->value.new_.t)
#define PRINT_INVALID \
    snprintf(trig->col.value, MQI_DIMENSION(trig->col.value) - 1, \
             "<invalid> => <invalid>")

    switch(ce->value.type) {
    case mqi_varchar:     PRINT_VALUE("'%s'" , varchar );     break;
    case mqi_integer:     PRINT_VALUE("%d"   , integer );     break;
    case mqi_unsignd:     PRINT_VALUE("%u"   , unsignd );     break;
    case mqi_floating:    PRINT_VALUE("%.2lf", floating);     break;
    case mqi_blob:        PRINT_INVALID;                      break;
    default:              PRINT_INVALID;                      break;
    }

#undef PRINT_INVALID
#undef PRINT_VALUE
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
