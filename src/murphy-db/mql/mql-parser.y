%{

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>

#include <murphy-db/assert.h>
#include <murphy-db/mqi.h>
#include <murphy-db/mql.h>

#define MQL_SUCCESS                                                     \
    do {                                                                \
        if (mode == mql_mode_exec)                                      \
            result = mql_result_success_create();                       \
    } while (0)

#define MQL_ERROR(code, fmt...)                                         \
    do {                                                                \
        switch (mode) {                                                 \
        case mql_mode_exec:                                             \
            result = mql_result_error_create(code, fmt);                \
            break;                                                      \
        case mql_mode_precompile:                                       \
            errno = code;                                               \
            free(statement);                                            \
            statement = NULL;                                           \
            break;                                                      \
        case mql_mode_parser:                                           \
            fprintf(mqlout, "%s:%d: error: ", file, yy_mql_lineno);     \
            fprintf(mqlout, fmt);                                       \
            fprintf(mqlout, "\n");                                      \
            break;                                                      \
        }                                                               \
        YYERROR;                                                        \
    } while (0)


#define SET_INPUT(t,v)                                                  \
    input_t *input;                                                     \
    if (ninput >= MQI_COLUMN_MAX)                                       \
        MQL_ERROR(EOVERFLOW, "Too many input values\n");                \
    input = inputs + ninput++;                                          \
    input->type = mqi_##t;                                              \
    input->flags = 0;                                                   \
    input->value.t = (v)

typedef enum mql_mode_e        mql_mode_t;
typedef struct input_s         input_t;

enum mql_mode_e {
    mql_mode_parser,
    mql_mode_exec,
    mql_mode_precompile,
};

struct input_s {
    mqi_data_type_t      type;
    uint32_t             flags;
    union {
        char *varchar;
        int32_t integer;
        uint32_t unsignd;
        double floating;
    }                    value;
};

extern int yy_mql_lineno;
extern int yy_mql_lex(void);


void yy_mql_error(const char *);

static int set_select_variables(int *, mqi_data_type_t *, int *, char *,int);
static void print_query_result(mqi_column_desc_t *, mqi_data_type_t *,
                               int *, int, int, void *);

static mqi_handle_t table;
static uint32_t     table_flags;

static char                  *trigger_name;
static struct mql_callback_s *callback;

static mqi_column_def_t   coldefs[MQI_COLUMN_MAX + 1];
static mqi_column_def_t  *coldef = coldefs;

static char              *colnams[MQI_COLUMN_MAX + 1];
static int                ncolnam;

static mqi_cond_entry_t   conds[MQI_COND_MAX + 1];
static mqi_cond_entry_t  *cond = conds;
static int                binds;

static input_t            inputs[MQI_COLUMN_MAX];
static int                ninput;

static mqi_column_desc_t  coldescs[MQI_COLUMN_MAX + 1];
static int                ncoldesc;

static char    *strs[256];
static int      nstr;

static int32_t  ints[256];
static int      nint;

static uint32_t uints[32];
static int      nuint;

static double   floats[256];
static int      nfloat;

static mql_mode_t mode;

static mql_statement_t   *statement;

static mql_result_type_t  rtype;
static mql_result_t      *result;

static char        *file;
static const char  *mqlbuf;
static int          mqlin;
static FILE        *mqlout;

%}

%union {
    mqi_data_type_t  type;
    char            *string;
    long long int    number;
    double           floating;
    int              integer;
    bool             boolean;
};


%defines

%token <string>   TKN_SHOW
%token <string>   TKN_BEGIN
%token <string>   TKN_COMMIT
%token <string>   TKN_ROLLBACK
%token <string>   TKN_TRANSACTION
%token <string>   TKN_TRANSACTIONS
%token <string>   TKN_CREATE
%token <string>   TKN_UPDATE
%token <string>   TKN_REPLACE
%token <string>   TKN_DELETE
%token <string>   TKN_DROP
%token <string>   TKN_DESCRIBE
%token <string>   TKN_TABLE
%token <string>   TKN_TABLES
%token <string>   TKN_INDEX
%token <string>   TKN_ROWS
%token <string>   TKN_COLUMN
%token <string>   TKN_TRIGGER
%token <string>   TKN_INSERT
%token <string>   TKN_SELECT
%token <string>   TKN_INTO
%token <string>   TKN_FROM
%token <string>   TKN_WHERE
%token <string>   TKN_VALUES
%token <string>   TKN_SET
%token <string>   TKN_ON
%token <string>   TKN_IN
%token <string>   TKN_OR
%token <string>   TKN_PERSISTENT
%token <string>   TKN_TEMPORARY
%token <string>   TKN_CALLBACK
%token <string>   TKN_VARCHAR
%token <string>   TKN_INTEGER
%token <string>   TKN_UNSIGNED
%token <string>   TKN_REAL
%token <string>   TKN_BLOB
%token <integer>  TKN_PARAMETER
%token <string>   TKN_LOGICAL_AND
%token <string>   TKN_LOGICAL_OR
%token <string>   TKN_LESS
%token <string>   TKN_LESS_OR_EQUAL
%token <string>   TKN_EQUAL
%token <string>   TKN_GREATER_OR_EQUAL
%token <string>   TKN_GREATER
%token <string>   TKN_NOT
%token <string>   TKN_LEFT_PAREN
%token <string>   TKN_RIGHT_PAREN
%token <string>   TKN_COMMA
%token <string>   TKN_SEMICOLON
%token <string>   TKN_PLUS
%token <string>   TKN_MINUS
%token <string>   TKN_STAR
%token <string>   TKN_SLASH
%token <number>   TKN_NUMBER
%token <floating> TKN_FLOATING
%token <string>   TKN_IDENTIFIER
%token <string>   TKN_QUOTED_STRING

%type <boolean>   optional_trigger_select

%type <integer>   insert
%type <integer>   insert_or_replace
%type <integer>   insert_option

%type <integer>   varchar
%type <integer>   blob
%type <integer>   sign

%type <floating>  floating_value

%start statement_list

%code requires {
    #include <murphy-db/mqi.h>
    #include <murphy-db/mql.h>

    typedef struct mql_callback_s  mql_callback_t;

    int yy_mql_input(void *, unsigned);

    mql_statement_t *mql_make_show_tables_statement(uint32_t);
    mql_statement_t *mql_make_describe_statement(mqi_handle_t);
    mql_statement_t *mql_make_transaction_statement(mql_statement_type_t,
                                                    char *);
    mql_statement_t *mql_make_insert_statement(mqi_handle_t, int, int,
                                               mqi_data_type_t*,
                                               mqi_column_desc_t*, void*);
    mql_statement_t *mql_make_update_statement(mqi_handle_t, int,
                                               mqi_cond_entry_t *, int,
                                               mqi_data_type_t *,
                                               mqi_column_desc_t *, void *);
    mql_statement_t *mql_make_delete_statement(mqi_handle_t, int,
                                               mqi_cond_entry_t *);
    mql_statement_t *mql_make_select_statement(mqi_handle_t, int, int,
                                               mqi_cond_entry_t *, int,
                                               char **, mqi_data_type_t *,
                                               int *, mqi_column_desc_t *);

    mql_result_t *mql_result_success_create(void);
    mql_result_t *mql_result_error_create(int, const char *, ...);
    mql_result_t *mql_result_event_column_change_create(mqi_handle_t, int,
                                                        mqi_change_value_t *,
                                                        mql_result_t *);
    mql_result_t *mql_result_event_row_change_create(mqi_event_type_t,
                                                     mqi_handle_t,
                                                     mql_result_t *);
    mql_result_t *mql_result_event_table_create(mqi_event_type_t,mqi_handle_t);
    mql_result_t *mql_result_event_transaction_create(mqi_event_type_t);
    mql_result_t *mql_result_columns_create(int, mqi_column_def_t *);
    mql_result_t *mql_result_rows_create(int, mqi_column_desc_t*,
                                         mqi_data_type_t*,int*,int,int,void*);
    mql_result_t *mql_result_string_create_table_list(int, char **);
    mql_result_t *mql_result_string_create_column_change(const char *,
                                                         const char *,
                                                         mqi_change_value_t *,
                                                         mql_result_t *);
    mql_result_t *mql_result_string_create_row_change(mqi_event_type_t,
                                                      const char *,
                                                      mql_result_t *);
    mql_result_t *mql_result_string_create_table_change(mqi_event_type_t,
                                                        const char *);
    mql_result_t *mql_result_string_create_transaction_change(
                                                            mqi_event_type_t);
    mql_result_t *mql_result_string_create_column_list(int, mqi_column_def_t*);
    mql_result_t *mql_result_string_create_row_list(int, char **,
                                                    mqi_column_desc_t *,
                                                    mqi_data_type_t *, int *,
                                                    int, int, void *);
    mql_result_t *mql_result_list_create(mqi_data_type_t, int, void *);

    mql_callback_t *mql_find_callback(char *);
    int mql_create_column_trigger(char *, mqi_handle_t, int,mqi_data_type_t,
                                  mql_callback_t *,
                                  int, char **, mqi_column_desc_t *,
                                  mqi_data_type_t *, int *,
                                  int);
    int mql_create_row_trigger(char *, mqi_handle_t, mql_callback_t *,
                               int, char **, mqi_column_desc_t *,
                               mqi_data_type_t *, int *, int);
    int mql_create_table_trigger(char *, mql_callback_t *);
    int mql_create_transaction_trigger(char *, mql_callback_t *);

    int mql_begin_transaction(char *);
    int mql_rollback_transaction(char *);
    int mql_commit_transaction(char *);
}


%%

/*#toplevel#*/
statement_list:
  statement
| statement_list semicolon statement
;

semicolon: TKN_SEMICOLON {
    if (mode != mql_mode_parser) {
        result = mql_result_error_create(EINVAL, "multiple MQL statements");
        YYERROR;
    }
};

/*#toplevel#*/
statement:
  show_statement
| create_statement
| drop_statement
| begin_statement
| commit_statement
| rollback_statement
| describe_statement
| insert_statement
| update_statement
| delete_statement
| select_statement
| error
;

/***************************
 *
 * Show statement
 *
 */
/*#toplevel#*/
show_statement:
  show_table_statement
;

show_table_statement: TKN_SHOW show_tables
;

show_tables: table_flags TKN_TABLES {
    char  *names[4096];
    int    n;
    
    if (mode == mql_mode_precompile)
        statement = mql_make_show_tables_statement(table_flags);
    else {
        if ((n = mqi_show_tables(table_flags, names,MQI_DIMENSION(names))) < 0)
            MQL_ERROR(errno, "can't show tables: %s", strerror(errno));
        else {
            if (mode == mql_mode_exec) {
                switch (rtype) {
                case mql_result_string:
                    result = mql_result_string_create_table_list(n, names);
                    break;
                case mql_result_list:
                    result = mql_result_list_create(mqi_string,n,(void*)names);
                    break;
                default:
                    result = mql_result_error_create(EINVAL,
                                                     "can't show tables: %s",
                                                     strerror(EINVAL));
                    break;
                }
            }
            else {
                mql_result_t *r = mql_result_string_create_table_list(n,names);

                fprintf(mqlout, "%s", mql_result_string_get(r));

                mql_result_free(r);
            }
        }
    }
};

/***********************************
 *
 * Create statement
 *
 */
/*#toplevel#*/
create_statement:
  create_table_statement
| create_index_statement
| create_trigger_statement
; 

/*#toplevel#*/
create_table_statement: TKN_CREATE create_table table_definition
;

/*#toplevel#*/
create_index_statement: TKN_CREATE create_index index_definition
;

/*#toplevel#*/
create_trigger_statement:
  create_transaction_trigger
| create_table_trigger
| create_row_trigger
| create_column_trigger
;


/* create table */

create_table: table_flags TKN_TABLE {
    coldef = coldefs;
    
    if (table_flags == MQI_ANY)
        table_flags = MQI_TEMPORARY;
};



table_definition: TKN_IDENTIFIER TKN_LEFT_PAREN column_defs TKN_RIGHT_PAREN {
    if (mqi_create_table($1, table_flags, NULL, coldefs) == MQI_HANDLE_INVALID)
        MQL_ERROR(errno, "Can't create table: %s\n", strerror(errno));
    else
        MQL_SUCCESS;
};

/*#toplevel#*/
column_defs:
  column_def
| column_defs TKN_COMMA column_def
;

/*#toplevel#*/
column_def: column_name column_type {
    memset(++coldef, 0, sizeof(mqi_column_def_t));
};

column_name: TKN_IDENTIFIER {
    if ((coldef - coldefs) >= MQI_COLUMN_MAX) {
        MQL_ERROR(EOVERFLOW, "Too many columns. Max %d columns allowed\n",
                  MQI_COLUMN_MAX);
    }

    coldef->name = $1;
};

/*#toplevel#*/
column_type:
  varchar       { coldef->type = mqi_varchar;   coldef->length = $1; }
| TKN_INTEGER   { coldef->type = mqi_integer;   coldef->length = 0;  }
| TKN_UNSIGNED  { coldef->type = mqi_unsignd;   coldef->length = 0;  }
| TKN_REAL      { coldef->type = mqi_floating;  coldef->length = 0;  }
| blob          { coldef->type = mqi_blob;      coldef->length = $1; }
;

varchar: TKN_VARCHAR TKN_LEFT_PAREN TKN_NUMBER TKN_RIGHT_PAREN {
    $$ = (int)$3;
};

blob: TKN_BLOB TKN_LEFT_PAREN TKN_NUMBER TKN_RIGHT_PAREN {
    $$ = (int)$3;
};

/* create index */

create_index: TKN_INDEX {
    ncolnam = 0;
};

index_definition: TKN_ON table_name TKN_LEFT_PAREN column_list TKN_RIGHT_PAREN
{
    colnams[ncolnam] = NULL;

    if (mqi_create_index(table, colnams) < 0)
        MQL_ERROR(errno, "failed to create index: %s", strerror(errno));
    else
        MQL_SUCCESS;
};


/* create trigger */

/*#toplevel#*/
create_transaction_trigger: TKN_CREATE create_trigger transaction_trigger
;

/*#toplevel#*/
create_table_trigger: TKN_CREATE create_trigger table_trigger
;

/*#toplevel#*/
create_row_trigger: TKN_CREATE create_trigger row_trigger
;

/*#toplevel#*/
create_column_trigger: TKN_CREATE create_trigger column_trigger
;

create_trigger: TKN_TRIGGER TKN_IDENTIFIER TKN_ON {
    if (mode != mql_mode_exec)
        MQL_ERROR(EPERM, "only mql_exec_string() can create triggers");
    else {
        table = MQI_HANDLE_INVALID;
        ncolnam = 0;
        trigger_name = $2;
        callback = NULL;
    }
};



transaction_trigger: TKN_TRANSACTIONS callback {

    if (mql_create_transaction_trigger(trigger_name, callback) < 0) {
        MQL_ERROR(errno, "failed to create transaction trigger: %s",
                  strerror(errno));
    }
    else {
        MQL_SUCCESS;
    }
};

table_trigger: TKN_TABLES callback {

    if (mql_create_table_trigger(trigger_name, callback) < 0)
        MQL_ERROR(errno, "failed to create table trigger: %s",strerror(errno));
    else
        MQL_SUCCESS;

};

row_trigger: TKN_ROWS TKN_IN table_name callback trigger_select {

    int rowsize;
    int colsizes[MQI_COLUMN_MAX + 1];
    mqi_data_type_t coltypes[MQI_COLUMN_MAX + 1];
    char errbuf[256];
    int sts;

    sts = set_select_variables(&rowsize, coltypes,colsizes,
                               errbuf, sizeof(errbuf));
    if (sts < 0)
        MQL_ERROR(errno, "%s", errbuf);

    sts = mql_create_row_trigger(trigger_name, table, callback,
                                 ncolnam,colnams,
                                 coldescs, coltypes, colsizes,
                                 rowsize);
    if (sts < 0)
        MQL_ERROR(errno, "failed to create row triger: %s",strerror(errno));
    else
        MQL_SUCCESS;
};

column_trigger: TKN_COLUMN TKN_IDENTIFIER TKN_IN table_name callback
                optional_trigger_select
{
    int colidx;
    mqi_data_type_t coltype;
    int rowsize;
    int colsizes[MQI_COLUMN_MAX + 1];
    mqi_data_type_t coltypes[MQI_COLUMN_MAX + 1];
    char errbuf[256];
    int sts;

    if ((colidx  = mqi_get_column_index(table, $2))    < 0 ||
        (coltype = mqi_get_column_type(table, colidx)) < 0  )
    {
        MQL_ERROR(errno, "do not know trigger column '%s'", $2);
    }

    if ($6) {
        sts = set_select_variables(&rowsize, coltypes,colsizes,
                                   errbuf, sizeof(errbuf));
        if (sts < 0)
            MQL_ERROR(errno, "%s", errbuf);

        sts = mql_create_column_trigger(trigger_name,
                                        table, colidx,coltype, callback,
                                        ncolnam,colnams,
                                        coldescs,coltypes,colsizes,
                                        rowsize);
    }
    else {
        sts = mql_create_column_trigger(trigger_name,
                                        table, colidx,coltype, callback,
                                        0,NULL,NULL,NULL,NULL, 0);
    }

    if (sts < 0)
        MQL_ERROR(errno,"failed to create column trigger: %s",strerror(errno));
    else
        MQL_SUCCESS;
};


callback: TKN_CALLBACK TKN_IDENTIFIER {
    if (!(callback = mql_find_callback($2))) {
        MQL_ERROR(ENOENT, "can't find callback '%s'", $2);
    }
};

trigger_select: TKN_SELECT columns
;

optional_trigger_select:
  /* no select */   { $$ = false; }
| trigger_select    { $$ = true;  }
;

/***********************************
 *
 * Drop statement
 *
 */
/*#toplevel#*/
drop_statement:
  drop_table_statement
| drop_index_statement
; 

/* drop table */

/*#toplevel#*/
drop_table_statement: TKN_DROP TKN_TABLE  table_name {
    if (mqi_drop_table(table) < 0)
        MQL_ERROR(errno, "failed to drop table: %s", strerror(errno));
    else
        MQL_SUCCESS;
}
;


/* drop index */

/*#toplevel#*/
drop_index_statement: TKN_DROP TKN_INDEX table_name {
};


/***********************************
 *
 * Begin/Commit/Rollback statement
 *
 */
/*#toplevel#*/
begin_statement: TKN_BEGIN transaction TKN_IDENTIFIER {
    if (mode == mql_mode_precompile)
        statement = mql_make_transaction_statement(mql_statement_begin, $3);
    else {
        if (mql_begin_transaction($3) < 0)
            MQL_ERROR(errno, "can't start transaction: %s", strerror(errno));
        else
            MQL_SUCCESS;
    }
};

/*#toplevel#*/
commit_statement: TKN_COMMIT transaction TKN_IDENTIFIER {
    if (mode == mql_mode_precompile)
        statement = mql_make_transaction_statement(mql_statement_commit, $3);
    else {
        if (mql_commit_transaction($3) < 0)
            MQL_ERROR(errno, "can't commit transaction: %s", strerror(errno));
        else
            MQL_SUCCESS;
    }
};

/*#toplevel#*/
rollback_statement: TKN_ROLLBACK transaction TKN_IDENTIFIER {
    if (mode == mql_mode_precompile)
        statement = mql_make_transaction_statement(mql_statement_rollback, $3);
    else {
        if (mql_rollback_transaction($3) < 0)
            MQL_ERROR(errno, "can't rollback transaction: %s",strerror(errno));
        else
            MQL_SUCCESS;
    }
};



/***********************************
 *
 * Describe statement
 *
 */
/*#toplevel#*/
describe_statement: TKN_DESCRIBE table_name {
    mqi_column_def_t defs[MQI_COLUMN_MAX];
    int              n;

    if (mode == mql_mode_precompile)
        statement = mql_make_describe_statement(table);
    else {
        if ((n = mqi_describe(table, defs, MQI_COLUMN_MAX)) < 0)
            MQL_ERROR(errno, "can't describe table: %s", strerror(errno));
        else {
            if (mode == mql_mode_exec) {
                switch (rtype) {
                case mql_result_columns:
                    result = mql_result_columns_create(n, defs);
                    break;
                case mql_result_string:
                    result = mql_result_string_create_column_list(n, defs);
                    break;
                default:
                    result = mql_result_error_create(EINVAL, "describe failed:"
                                                     " invalid result type %d",
                                                     rtype);
                    break;
                }
            }
            else {
                mql_result_t *r = mql_result_string_create_column_list(n,defs);

                fprintf(mqlout, "%s", mql_result_string_get(r));

                mql_result_free(r);
            }
        }
    }
};

/***********************************
 *
 * Insert statement
 *
 */
/*#toplevel#*/
insert_statement: insert table_name insert_columns TKN_VALUES insert_values {
    void              *row[2];
    char              *col;
    mqi_column_desc_t *cd;
    mqi_data_type_t    coltypes[MQI_COLUMN_MAX + 1];
    input_t           *inp;
    mqi_data_type_t    type;
    int                cindex;
    int                err;
    int                i;

    if (!ncolnam) {
        while ((colnams[ncolnam] = mqi_get_column_name(table, ncolnam)))
            ncolnam++;
    }

    if (ncolnam != ninput)
        MQL_ERROR(EINVAL, "unbalanced set of columns and values");

    for (i = 0, err = 0; i < ncolnam; i++) {
        col = colnams[i];
        cd  = coldescs + i;
        inp = inputs + i;

        if ((cindex = mqi_get_column_index(table, col)) < 0) {
            MQL_ERROR(ENOENT, "know nothing about '%s'", col);
            err = 1;
            continue;
        }

        type = coltypes[i] = mqi_get_column_type(table, cindex);

        if (type != inp->type) {
            if (type != mqi_integer ||
                inp->type != mqi_unsignd ||
                inp->value.unsignd > INT32_MAX)
            {
                MQL_ERROR(EINVAL, "mismatching column and value type for '%s'",
                          col);
                err = 1;
                continue;
            }
        }

        cd->cindex = cindex;
        cd->offset = (void *)&inp->value - (void *)inputs;
    }

    cd = coldescs + i;
    cd->cindex = -1;
    cd->offset = -1;


    if (mode == mql_mode_precompile) {
        statement = mql_make_insert_statement(table, $1, ncolnam, coltypes,
                                              coldescs, inputs);
    }
    else {
        row[0] = (void *)inputs;
        row[1] = NULL;

        if (err || mqi_insert_into(table, $1, coldescs, row) < 0)
            MQL_ERROR(errno, "insert failed: %s\n", strerror(errno));
        else
            MQL_SUCCESS;
    }
};


insert: insert_or_replace {
      table = MQI_HANDLE_INVALID;
      ncolnam = 0;
      ninput = 0;
      ncoldesc = 0;
      $$ = $1;
};

insert_or_replace:
  TKN_INSERT insert_option TKN_INTO  { $$ = $2; }
| TKN_REPLACE TKN_INTO               { $$ = 1;  } 
;

insert_option:
   /* no option */     { $$ = 0; }
| TKN_OR TKN_REPLACE   { $$ = 1; }
/*
| TKN_IGNORE           { $$ = 1; }
*/
;


insert_columns: 
  /* all columns: leaves ncolnam as zero */
| TKN_LEFT_PAREN column_list TKN_RIGHT_PAREN
;

insert_values: TKN_LEFT_PAREN input_value_list TKN_RIGHT_PAREN;

/*#toplevel#*/
input_value_list:
  input_value
| input_value_list TKN_COMMA input_value
; 



/***********************************
 *
 * Update statement
 *
 */
/*#toplevel#*/
update_statement: update table_name TKN_SET assignment_list where_clause {
    mqi_column_desc_t *cd    = coldescs + ninput;
    mqi_cond_entry_t  *where = (cond == conds) ? NULL : conds;
    mqi_data_type_t    coltypes[MQI_COLUMN_MAX + 1];
    int                i;

    if (!ninput)
        MQL_ERROR(ENOMEDIUM, "No column to update");

    cd->cindex = -1;
    cd->offset = -1;

    if (mode == mql_mode_precompile) {
        for (i = 0;  i < ninput; i++)
            coltypes[i] = inputs[i].type;

        statement = mql_make_update_statement(table, cond - conds, conds,
                                              ninput, coltypes, coldescs,
                                              inputs);
    }
    else {
        if (mqi_update(table, where, coldescs, inputs) < 0)
            MQL_ERROR(errno, "update failed: %s", strerror(errno));
        else
            MQL_SUCCESS;
    }
};

update: TKN_UPDATE {
      table = MQI_HANDLE_INVALID;
      ninput = 0;
      ncoldesc = 0;
      nstr = 0;
      nint = 0;
      nuint = 0;
      nfloat = 0;
      cond = conds;
      binds = 0;
};


/*#toplevel#*/
assignment_list:
  assignment
| assignment_list TKN_COMMA assignment
;

/*#toplevel#*/
assignment: TKN_IDENTIFIER TKN_EQUAL input_value {
    int                i   = ninput - 1;
    input_t           *inp = inputs + i;
    mqi_column_desc_t *cd  = coldescs + i;
    int                cindex;
    int                offset;
    mqi_data_type_t    type;

    if ((cindex = mqi_get_column_index(table, $1)) < 0)
        MQL_ERROR(ENOENT, "know nothing about '%s'", $1);
 
    if ((inp->flags & MQL_BINDABLE))
        offset = -(MQL_BIND_INDEX(inp->flags) + 1);
    else {
        if ((type = mqi_get_column_type(table, cindex)) != inp->type) {
            if (type != mqi_integer ||
                inp->type != mqi_unsignd ||
                inp->value.unsignd > INT32_MAX)
            {
                MQL_ERROR(EINVAL, "mismatching column and value type "
                          "for '%s'",$1);
            }
        }
        offset = (void *)&inp->value - (void *)inputs;
    }

    cd->cindex = cindex;
    cd->offset = offset;
};



/***********************************
 *
 * Delete statement
 *
 */
/*#toplevel#*/
delete_statement: delete table_name where_clause {
    mqi_cond_entry_t *where = (cond == conds) ? NULL : conds;

    if (mode == mql_mode_precompile)
        statement = mql_make_delete_statement(table, cond - conds, where);
    else {
        if (mqi_delete_from(table, where) < 0)
            MQL_ERROR(errno, "delete failed: %s", strerror(errno));
        else
            MQL_SUCCESS;
    }
};

delete: TKN_DELETE TKN_FROM {
    table = MQI_HANDLE_INVALID;
    nstr = 0;
    nint = 0;
    nuint = 0;
    nfloat = 0;
    cond = conds;
    binds = 0;
};

/***********************************
 *
 * Select statement
 *
 */
/*#toplevel#*/
select_statement: select columns TKN_FROM table_name where_clause {
    int colsizes[MQI_COLUMN_MAX + 1];
    mqi_data_type_t coltypes[MQI_COLUMN_MAX + 1];
    mqi_cond_entry_t *where;
    int rowsize;
    int tsiz;
    size_t rsiz;
    void *rows;
    char errbuf[256];
    int sts;
    int n;


    if ((tsiz = mqi_get_table_size(table)) < 0)
        MQL_ERROR(errno, "can't get table size: %s", strerror(errno));


    sts = set_select_variables(&rowsize, coltypes,colsizes,
                               errbuf, sizeof(errbuf));
    if (sts < 0)
        MQL_ERROR(errno, "%s", errbuf);


    if (mode != mql_mode_precompile && mode != mql_mode_exec && !tsiz) {
        if (mode == mql_mode_parser)
            fprintf(mqlout, "no rows\n");
    }
    else {
        rsiz  = tsiz * rowsize;
        rows  = alloca(rsiz);
        where = (cond == conds) ? NULL : conds;

        if (mode != mql_mode_precompile) {
            if (tsiz != 0) {
                if ((n = mqi_select(table, where,
                                    coldescs, rows, rowsize, tsiz)) < 0)
                    MQL_ERROR(errno, "select failed: %s", strerror(errno));
            }
            else
                n = 0;
        }

        switch (mode) {
        case mql_mode_parser:
            fprintf(mqlout, "Selected %d rows:\n", n);
            print_query_result(coldescs, coltypes, colsizes, n, rowsize, rows);
            break;
        case mql_mode_exec:
            if (rtype == mql_result_rows) {
                result = mql_result_rows_create(ncolnam, coldescs, coltypes,
                                                colsizes, n, rowsize, rows);
            }
            else {
                result = mql_result_string_create_row_list(ncolnam, colnams,
                                                           coldescs, coltypes,
                                                           colsizes,
                                                           n, rowsize, rows);
            }
            break;
        case mql_mode_precompile:
            statement = mql_make_select_statement(table, rowsize,
                                                  cond - conds, where,
                                                  ncolnam, colnams, coltypes,
                                                  colsizes, coldescs); 
            break;
        }
    }
};


select: TKN_SELECT {
    table = MQI_HANDLE_INVALID;
    ncolnam = 0;
    nstr = 0;
    nint = 0;
    nuint = 0;
    nfloat = 0;
    cond = conds;
    binds = 0;
};

columns:
  TKN_STAR
| column_list
;


/***********************************
 *
 * Transaction
 *
 */
transaction:
  /* no token */
| TKN_TRANSACTION
;

/***********************************
 *
 * Table name
 *
 */
table_name: TKN_IDENTIFIER {
    if ((table = mqi_get_table_handle($1)) == MQI_HANDLE_INVALID)
        MQL_ERROR(errno, "Do not know anything about '%s'", $1);
};

/***********************************
 *
 * Table flags
 *
 */
table_flags:
  /* no option */ { table_flags = MQI_ANY;        }
| TKN_PERSISTENT  { table_flags = MQI_PERSISTENT; }
| TKN_TEMPORARY   { table_flags = MQI_TEMPORARY;  }
;

/***********************************
 *
 * Column list
 *
 */
/*#toplevel#*/
column_list:
  column
| column_list TKN_COMMA column
;

column: TKN_IDENTIFIER {
    if (ncolnam < MQI_COLUMN_MAX)
        colnams[ncolnam++] = $1;
    else
        MQL_ERROR(EOVERFLOW, "Too many columns");
};

/***********************************
 *
 * Input value
 *
 */
/*#toplevel#*/
input_value:
  string_input
| integer_input
| unsigned_input
| floating_input
| parameter_input
;

string_input:   TKN_QUOTED_STRING { SET_INPUT(varchar,  $1);              };
integer_input:  sign TKN_NUMBER   { SET_INPUT(integer,  $1 * $2);         };
unsigned_input: TKN_NUMBER        { SET_INPUT(unsignd,  $1);              };
floating_input: TKN_FLOATING      { SET_INPUT(floating, $1);              }
|               sign TKN_FLOATING { SET_INPUT(floating, (double)$1 * $2); };

parameter_input: TKN_PARAMETER {
    input_t *input;

    if (mode != mql_mode_precompile) {
        MQL_ERROR(EINVAL, "parameters are allowed only in "
                  "precompilation mode");
    }
    if (binds >= MQL_PARAMETER_MAX) {
        MQL_ERROR(EOVERFLOW, "number of parameters exceeds %d",
                  MQL_PARAMETER_MAX);
    }

    input = inputs + ninput++;
    input->type = $1;
    input->flags = MQL_BINDABLE | MQL_BIND_INDEX(binds++);

    memset(&input->value, 0, sizeof(input->value));
};


/***********************************
 *
 * Where clause
 *
 */
where_clause:
  /* no where clause  */ {
  }
| TKN_WHERE conditional_expression {
    cond->type = mqi_operator;
    cond->u.operator_ = mqi_end;
    cond++;
  };


/*#toplevel#*/
conditional_expression:
  relational_expression
| relational_expression logical_operator relational_expression
;

/*#toplevel#*/
relational_expression: value relational_operator value;

/*#toplevel#*/
value: 
  column_value
| string_variable
| integer_variable
| unsigned_variable
| floating_variable
| parameter_value
| expression_value
| unary_operator value
;

column_value: TKN_IDENTIFIER {
    int cx;

    if (cond - conds >= MQI_COND_MAX)
        MQL_ERROR(EOVERFLOW, "too complex condition");

    if ((cx = mqi_get_column_index(table,$1)) < 0)
        MQL_ERROR(ENOENT, "no column with name '%s'", $1);

    cond->type = mqi_column;
    cond->u.column = cx;
    cond++;
};

string_variable: TKN_QUOTED_STRING {
    if (cond - conds >= MQI_COND_MAX)
        MQL_ERROR(EOVERFLOW, "too complex condition");
    strs[nstr] = $1;
    cond->type = mqi_variable;
    cond->u.variable.flags = 0;
    cond->u.variable.type = mqi_varchar;
    cond->u.variable.v.varchar = strs + nstr++;
    cond++;
};

integer_variable: sign TKN_NUMBER {
    if (cond - conds >= MQI_COND_MAX)
        MQL_ERROR(EOVERFLOW, "too complex condition");
    ints[nint] = $1 * $2;
    cond->type = mqi_variable;
    cond->u.variable.type = mqi_integer;
    cond->u.variable.v.integer = ints + nint++;
    cond++;
};

unsigned_variable: TKN_NUMBER {
    if (cond - conds >= MQI_COND_MAX)
        MQL_ERROR(EOVERFLOW, "too complex condition");
    uints[nuint] = $1;
    cond->type = mqi_variable;
    cond->u.variable.flags = 0;
    cond->u.variable.type = mqi_unsignd;
    cond->u.variable.v.unsignd = uints + nuint++;
    cond++;
};

floating_variable: floating_value {
    if (cond - conds >= MQI_COND_MAX)
        MQL_ERROR(EOVERFLOW, "too complex condition");
    floats[nfloat] = $1;
    cond->type = mqi_variable;
    cond->u.variable.flags = 0;
    cond->u.variable.type = mqi_floating;
    cond->u.variable.v.floating = floats + nfloat++;
    cond++;
};

floating_value:
  TKN_FLOATING        { return $1;              }
| sign TKN_FLOATING   { return (double)$1 * $2; }


parameter_value: TKN_PARAMETER {
    if (mode != mql_mode_precompile) {
        MQL_ERROR(EINVAL, "parameters are allowed only in "
                  "precompilation mode");
    }
    if (binds >= MQL_PARAMETER_MAX) {
        MQL_ERROR(EOVERFLOW, "number of parameters exceeds %d",
                  MQL_PARAMETER_MAX);
    }
    if (cond - conds >= MQI_COND_MAX) {
        MQL_ERROR(EOVERFLOW, "too complex condition");
    }
    cond->type = mqi_variable;
    cond->u.variable.flags = MQL_BINDABLE | MQL_BIND_INDEX(binds++);
    cond->u.variable.type = $1;
    cond->u.variable.v.generic = NULL;
    cond++;
};

expression_value:
  TKN_LEFT_PAREN  {
    if (cond - conds >= MQI_COND_MAX)
        MQL_ERROR(EOVERFLOW, "too complex condition");
    cond->type = mqi_operator;
    cond->u.operator_ = mqi_begin;
    cond++;
  }
  conditional_expression
  TKN_RIGHT_PAREN {
    if (cond - conds >= MQI_COND_MAX)
        MQL_ERROR(EOVERFLOW, "too complex condition");
    cond->type = mqi_operator;
    cond->u.operator_ = mqi_end;
    cond++;
  }
;


sign:
  TKN_PLUS  { $$ = +1; }
| TKN_MINUS { $$ = -1; }
;


unary_operator:
  TKN_NOT {
      cond->type = mqi_operator;
      cond->u.operator_ = mqi_not;
      cond++;
  }
;

relational_operator:
  TKN_LESS {
      cond->type = mqi_operator;
      cond->u.operator_ = mqi_less;
      cond++;
  }
| TKN_LESS_OR_EQUAL {
    cond->type = mqi_operator;
    cond->u.operator_ = mqi_leq;
    cond++;
  }
| TKN_EQUAL {
    cond->type = mqi_operator;
    cond->u.operator_ = mqi_eq;
    cond++;
  }
| TKN_GREATER_OR_EQUAL {
    cond->type = mqi_operator;
    cond->u.operator_ = mqi_geq;
    cond++;
  }
| TKN_GREATER {
    cond->type = mqi_operator;
    cond->u.operator_ = mqi_gt;
    cond++;
  }
;

logical_operator:
  TKN_LOGICAL_AND {
      cond->type = mqi_operator;
      cond->u.operator_ = mqi_and;
      cond++;
  }
| TKN_LOGICAL_OR {
    cond->type = mqi_operator;
    cond->u.operator_ = mqi_or;
    cond++;
  }
;


%%


int mql_exec_file(const char *path)
{
    char buf[1024];
    int sts;

    mode   = mql_mode_parser;
    rtype  = mql_result_unknown;
    mqlbuf = NULL;
    mqlout = stderr;
    
    if (!path) {
        mqlin = fileno(stdin);
        sts = yy_mql_parse() ? -1 : 0;
    }
    else {
        strncpy(buf, path, sizeof(buf));
        buf[sizeof(buf)-1] = '\0';
        
        file = basename(buf);

        if ((mqlin = open(path, O_RDONLY)) < 0) {
            sts = -1;
            fprintf(mqlout, "could not open file '%s': %s\n",
                    path, strerror(errno));
        }
        else {
            sts = yy_mql_parse() ? -1 : 0;
            close(mqlin);
        }
    }

    mqlin = -1;
    
    return sts;
}


mql_result_t *mql_exec_string(mql_result_type_t result_type, const char *str)
{
    if (result_type == mql_result_dontcare)
        result_type = mql_result_string;

    MDB_CHECKARG((result_type == mql_result_event ||
                  result_type == mql_result_columns  ||
                  result_type == mql_result_rows ||
                  result_type == mql_result_string  ) && 
                 str, NULL);

    mode = mql_mode_exec;
    result = NULL;
    rtype  = result_type;
    mqlbuf = str;

    if (yy_mql_parse() && !result) {
        result = mql_result_error_create(EIO, "Syntax error in '%s'", str);
    }


    return result;
}

mql_statement_t *mql_precompile(const char *str)
{
    MDB_CHECKARG(str, NULL);

    mode = mql_mode_precompile;
    rtype = mql_result_unknown;
    statement = NULL;
    mqlbuf = str;
    
    yy_mql_parse();

    return statement;
} 

int yy_mql_input(void *dst, unsigned dstlen)
{
    int len = 0;

    if (dst && dstlen > 0) {

        if (mqlbuf) {
            if ((len = strlen(mqlbuf)) < 1)
                len = 0;
            else if ((unsigned)len + 1 <= dstlen) {
                memcpy(dst, mqlbuf, len + 1);
                mqlbuf += len;
            }
            else {
                memcpy(dst, mqlbuf, dstlen);
                mqlbuf += dstlen;
            }
        }
        else if (mqlin >= 0) {
            while ((len = read(mqlin, dst, dstlen)) < 0) {
                if (errno != EINTR) {
                    break;
                }
            }
        }
    }

    return len;
}


void yy_mql_error(const char *msg)
{
    if (mode == mql_mode_parser)
        fprintf(mqlout, "Error: '%s'\n", msg);
}


static int set_select_variables(int *rowsize,
                                mqi_data_type_t *coltypes,
                                int *colsizes,
                                char *errbuf, int elgh)
{
    mqi_column_desc_t *cd;
    int i;
    int rlgh;
    int colsize;
    int colidx;
    mqi_data_type_t coltype;

    if (!ncolnam) {
        while ((colnams[ncolnam] = mqi_get_column_name(table, ncolnam)))
            ncolnam++;
    }

    for (i = 0, rlgh = 0;  i < ncolnam;   i++) {
        cd = coldescs + i;
        
        if ((colidx  = mqi_get_column_index(table, colnams[i])) < 0 ||
            (colsize = mqi_get_column_size(table, colidx))      < 0 ||
            (coltype = mqi_get_column_type(table, colidx)) == mqi_error)
        {
            snprintf(errbuf, elgh, "invalid column '%s'", colnams[i]);
            return -1;
        }
        cd->cindex = colidx;
        cd->offset = rlgh;
        
        coltypes[i] = coltype;
        colsizes[i] = colsize;
        
        switch (coltype) {
        case mqi_varchar:   rlgh += sizeof(char *);    break;
        case mqi_integer:   rlgh += sizeof(int32_t);   break;
        case mqi_unsignd:   rlgh += sizeof(uint32_t);  break;
        case mqi_floating:  rlgh += sizeof(double);    break;
        case mqi_blob:      rlgh += sizeof(void *);    break;
        default:                                       break;
        }
    } /* for */
    
    cd = coldescs + i;
    cd->cindex = -1;
    cd->offset = -1;

    *rowsize = rlgh;

    return 0;
}


static void print_query_result(mqi_column_desc_t *coldescs,
                               mqi_data_type_t   *coltypes,
                               int               *colsizes,
                               int                nresult,
                               int                recsize,
                               void              *results)
{
    int i, j, recoffs;
    void *data;
    char  name[4096];
    int   clgh;
    int   clghs[MQI_COLUMN_MAX + 1];
    int   n;

    for (j = 0, n = 0;  j < ncolnam;  j++) {
        snprintf(name, sizeof(name),  "%s", colnams[j]);

        switch (coltypes[j]) {
        case mqi_varchar:   clgh = colsizes[j] - 1;  break;
        case mqi_integer:   clgh = 11;               break;
        case mqi_unsignd:   clgh = 10;               break;
        case mqi_floating:  clgh = 10;               break;
        default:            clgh = 0;                break;
        }

        clghs[j] = clgh;

        if (clgh < (int)sizeof(name))
            name[clgh] = '\0';

        n += fprintf(mqlout, "%s%*s", j?" ":"", clgh,name);

    }

    if (n > (int)sizeof(name)-1)
        n = sizeof(name)-1;
    memset(name, '-', n);
    name[n] = '\0';

    fprintf(mqlout, "\n%s\n", name);



    for (i = 0, recoffs = 0;  i < nresult;  i++, recoffs += recsize) {
        for (j = 0;  j < ncolnam;  j++) {
            if (j) fprintf(mqlout, " ");

            data = results + (recoffs + coldescs[j].offset);
            clgh = clghs[j];

#define PRINT(t,f) fprintf(mqlout, f, clgh, *(t *)data)

            switch (coltypes[j]) {
            case mqi_varchar:     PRINT(char *  , "%*s"   );    break;
            case mqi_integer:     PRINT(int32_t , "%*d"   );    break;
            case mqi_unsignd:     PRINT(uint32_t, "%*u"   );    break;
            case mqi_floating:    PRINT(double  , "%*.2lf");    break;
            case mqi_blob:                                      break;
            default:                                            break;
            }

#undef PRINT

        }
        fprintf(mqlout, "\n");
    }
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */
