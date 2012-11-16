
/*
 * DB commands
 */

#include <stdarg.h>

#include <murphy-db/mql.h>
#include <murphy-db/mqi.h>

static void db_cmd(char *fmt, ...)
{
    mql_result_t *r;
    char          buf[1024];
    va_list       ap;
    int           n, error;
    const char   *msg;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n < (int)sizeof(buf) && n > 0) {
        r = mql_exec_string(mql_result_string, buf);

        if (mql_result_is_success(r))
            printf("%s\n", mql_result_string_get(r));
        else {
            error = mql_result_error_get_code(r);
            msg   = mql_result_error_get_message(r);

            printf("DB error %d: %s\n", error, msg ? msg : "unknown error");
        }

        mql_result_free(r);
    }
}


static void db_exec(mrp_console_t *c, void *user_data, const char *grp,
                    const char *cmd, char *args)
{
    mqi_handle_t tx;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);
    MRP_UNUSED(grp);
    MRP_UNUSED(cmd);

    tx = mqi_begin_transaction();
    db_cmd(args);
    mqi_commit_transaction(tx);
}


void db_source(mrp_console_t *c, void *user_data, int argc, char **argv)
{
    mqi_handle_t tx;
    int          i, success;

    MRP_UNUSED(c);
    MRP_UNUSED(user_data);

    success = TRUE;
    tx      = mqi_begin_transaction();

    for (i = 2; i < argc && success; i++) {
        if (mql_exec_file(argv[i]) == 0)
            printf("DB script '%s' OK\n", argv[i]);
        else {
            printf("DB script error %d: %s\n", errno, strerror(errno));
            success = FALSE;
        }
    }

    if (success)
        mqi_commit_transaction(tx);
    else {
        mqi_rollback_transaction(tx);
        printf("DB rolled back.\n");
    }
}


#define DB_GROUP_DESCRIPTION                                                \
    "Database commands provide means to manipulate the Murphy database\n"   \
    "from the console. Commands are provided for listing, describing,\n"    \
    "and removing tables as well as for issuing arbitrary high-level\n"     \
    "MQL commands. Note that these commands are intended for debugging\n" \
    "and debugging purposes. Extra care should to be taken when directly\n" \
    "manipulating the database."

#define DBEXEC_SYNTAX      "<DB command>"
#define DBEXEC_SUMMARY     "execute the given database MQL command"
#define DBEXEC_DESCRIPTION "Executes the given MQL command and prints the\n" \
    "result.\n"

#define DBSRC_SYNTAX      "source <file>"
#define DBSRC_SUMMARY     "evaluate the MQL script in the given <file>"
#define DBSRC_DESCRIPTION "Read and evaluate the contents of <file>.\n"


MRP_CORE_CONSOLE_GROUP(db_group, "db", DB_GROUP_DESCRIPTION, NULL, {
        MRP_TOKENIZED_CMD("source", db_source, FALSE,
                          DBSRC_SYNTAX, DBSRC_SUMMARY, DBSRC_DESCRIPTION),
        MRP_RAWINPUT_CMD("eval", db_exec,
                         MRP_CONSOLE_CATCHALL | MRP_CONSOLE_SELECTABLE,
                         DBEXEC_SYNTAX, DBEXEC_SUMMARY, DBEXEC_DESCRIPTION),
});
