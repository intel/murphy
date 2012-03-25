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
static mqi_handle_t transactions[10];
static int          txdepth;
static mqi_handle_t persons = MQI_HANDLE_INVALID;
static int          columns_no_in_persons = -1;
static int          rows_no_in_persons = -1;


static Suite *libmqi_suite(void);
static TCase *basic_tests(void);
static void   print_rows(int, query_t *);


int main(int argc, char **argv)
{
    Suite   *s  = libmqi_suite();
    SRunner *sr = srunner_create(s);
    int      nf;
    int      i;
    
    for (i = 1;  i < argc;  i++) {
        if (!strcmp("-v", argv[i]))
            verbose = 1;
        else if (!strcmp("-f", argv[1]))
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
    PREREQUISITE(open_db);

    persons = MQI_CREATE_TABLE("persons", MQI_TEMPORARY,
                               persons_coldefs, persons_indexdef);

    fail_if(persons == MQI_HANDLE_INVALID, "errno (%s)", strerror(errno));

    columns_no_in_persons = MQI_DIMENSION(persons_coldefs) - 1;
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

    fail_if(txdepth >= MQI_DIMENSION(transactions), "too many nested "
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

    query_t *r, rows[32];
    int i, n;

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

        for (found = 0, j = 0;  j < MQI_DIMENSION(artists)-1;  j++) {
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


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
