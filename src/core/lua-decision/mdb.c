/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
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

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common.h>
#include <murphy/common/debug.h>
#include <murphy-db/mqi.h>
#include <murphy-db/mql.h>

#include <murphy/core/context.h>
#include <murphy/core/scripting.h>
#include <murphy/core/lua-decision/mdb.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-utils/strarray.h>
#include <murphy/core/lua-bindings/murphy.h>


#define TABLE_CLASS         MRP_LUA_CLASS(mdb, table)
#define SELECT_CLASS        MRP_LUA_CLASS(mdb, select)

#define TABLE_ROW_CLASSID   MRP_LUA_CLASSID_ROOT "table_row"
#define SELECT_ROW_CLASSID  MRP_LUA_CLASSID_ROOT "select_row"


typedef enum   field_e        field_t;
typedef struct row_s          row_t;
typedef struct const_def_s    const_def_t;



enum field_e {
    NAME = 1,
    INDEX,
    COLUMNS,
    TABLE,
    CONDITION,
    STATEMENT,
    SINGLEVAL,
    CREATE
};


struct mrp_lua_mdb_table_s {
    bool                builtin;
    mqi_handle_t        handle;
    const char         *name;
    mrp_lua_strarray_t *index;
    size_t              ncolumn;
    mqi_column_def_t   *columns;
    size_t              nrow;
};


struct mrp_lua_mdb_select_s {
    const char *name;
    const char *table_name;
    mrp_lua_strarray_t *columns;
    const char *condition;
    struct {
        const char *string;
        mql_statement_t *precomp;
    } statement;
    mql_result_t *result;
    size_t nrow;
};

struct row_s {
    int index;
    void *data;
};

struct const_def_s {
    const char *name;
    mqi_data_type_t value;
};

static int  table_create_from_lua(lua_State *);
static int  table_getfield(lua_State *);
static int  table_setfield(lua_State *);
static int  table_tostring(lua_State *);
static void table_destroy_from_lua(void *);

static void table_row_class_create(lua_State *);
/* static int  table_row_create(lua_State *, int, void *, int); */
static int  table_row_getfield(lua_State *);
static int  table_row_setfield(lua_State *);
static int  table_row_getlength(lua_State *);
static mrp_lua_mdb_table_t *table_row_check(lua_State *, int, int *);

static int  select_create_from_lua(lua_State *);
static int  select_getfield(lua_State *);
static int  select_setfield(lua_State *);
static void select_destroy_from_lua(void *);
static int  select_update(lua_State *, int, mrp_lua_mdb_select_t *);
static int  select_update_from_lua(lua_State *);
static int  select_update_from_resolver(mrp_scriptlet_t *,mrp_context_tbl_t *);
static void select_install(lua_State *, mrp_lua_mdb_select_t *);

static void select_row_class_create(lua_State *);
/* static int  select_row_create(lua_State *, int, void *, int); */
static int  select_row_getfield(lua_State *);
static int  select_row_setfield(lua_State *);
static int  select_row_getlength(lua_State *);
static mrp_lua_mdb_select_t *select_row_check(lua_State *, int, int *);

static bool define_constants(lua_State *);

static field_t field_check(lua_State *, int, const char **);
static field_t field_name_to_type(const char *, size_t);

static mqi_column_def_t *check_coldefs(lua_State *, int, size_t *);
static int push_coldefs(lua_State *, mqi_column_def_t *, size_t);
static void free_coldefs(mqi_column_def_t *);

static int row_create(lua_State *, int, void *, int, const char *);
static row_t *row_check(lua_State *, int, const char *);

static void adjust_lua_table_size(lua_State *, int, void *, size_t, size_t,
                                  const char *);
static bool create_mdb_table(mrp_lua_mdb_table_t *);


MRP_LUA_METHOD_LIST_TABLE (
    table_methods,           /* methodlist name */
    MRP_LUA_METHOD_CONSTRUCTOR  (table_create_from_lua)
);

#if 0
MRP_LUA_METHOD_LIST_TABLE (
    table_row_methods,       /* methodlist name */
);
#endif

MRP_LUA_METHOD_LIST_TABLE (
    select_methods,          /* methodlist name */
    MRP_LUA_METHOD_CONSTRUCTOR  (select_create_from_lua)
);

MRP_LUA_METHOD_LIST_TABLE (
    table_overrides,         /* methodlist name */
    MRP_LUA_OVERRIDE_CALL       (table_create_from_lua)
    MRP_LUA_OVERRIDE_GETFIELD   (table_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (table_setfield)
    MRP_LUA_OVERRIDE_STRINGIFY  (table_tostring)
);

MRP_LUA_METHOD_LIST_TABLE (
    table_row_overrides,     /* methodlist name */
    MRP_LUA_OVERRIDE_GETFIELD   (table_row_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (table_row_setfield)
    MRP_LUA_OVERRIDE_GETLENGTH  (table_row_getlength)
);

MRP_LUA_METHOD_LIST_TABLE (
    select_overrides,        /* methodlist name */
    MRP_LUA_OVERRIDE_CALL       (select_create_from_lua)
    MRP_LUA_OVERRIDE_GETFIELD   (select_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (select_setfield)
    MRP_LUA_METHOD     (update,  select_update_from_lua)
);

MRP_LUA_METHOD_LIST_TABLE (
    select_row_overrides,    /* methodlist name */
    MRP_LUA_OVERRIDE_GETFIELD   (select_row_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (select_row_setfield)
    MRP_LUA_OVERRIDE_GETLENGTH  (select_row_getlength)
);

MRP_LUA_CLASS_DEF (
   mdb,                      /* class name */
   table,                    /* constructor name */
   mrp_lua_mdb_table_t,      /* userdata type */
   table_destroy_from_lua,   /* userdata destructor */
   table_methods,            /* class methods */
   table_overrides           /* override methods */
);

MRP_LUA_CLASS_DEF (
   mdb,                      /* class name */
   select,                   /* constructor name */
   mrp_lua_mdb_select_t,     /* userdata type */
   select_destroy_from_lua,  /* userdata destructor */
   select_methods,           /* class methods */
   select_overrides          /* override methods */
);


void mrp_lua_create_mdb_class(lua_State *L)
{
    mrp_lua_create_object_class(L, TABLE_CLASS);
    mrp_lua_create_object_class(L, SELECT_CLASS);

    table_row_class_create(L);
    select_row_class_create(L);

    define_constants(L);

    luaL_findtable(L, LUA_GLOBALSINDEX, "builtin.table", 20);
}

mrp_lua_mdb_table_t *mrp_lua_create_builtin_table(lua_State    *L,
                                                  mqi_handle_t  handle)
{
    mrp_lua_mdb_table_t *tbl = NULL;

    MRP_UNUSED(L);
    MRP_UNUSED(handle);

    return tbl;
}

mrp_lua_mdb_table_t *mrp_lua_table_check(lua_State *L, int idx)
{
    return (mrp_lua_mdb_table_t *)mrp_lua_check_object(L, TABLE_CLASS, idx);
}

mrp_lua_mdb_table_t *mrp_lua_to_table(lua_State *L, int idx)
{
    return (mrp_lua_mdb_table_t *)mrp_lua_to_object(L, TABLE_CLASS, idx);
}

int mrp_lua_push_table(lua_State *L, mrp_lua_mdb_table_t *tbl)
{
    return mrp_lua_push_object(L, tbl);
}

const char *mrp_lua_table_name(mrp_lua_mdb_table_t *tbl)
{
    return (tbl && tbl->name) ? tbl->name : "<unknown>";
}

mrp_lua_mdb_select_t *mrp_lua_select_check(lua_State *L, int idx)
{
    return (mrp_lua_mdb_select_t *)mrp_lua_check_object(L, SELECT_CLASS, idx);
}

mrp_lua_mdb_select_t *mrp_lua_to_select(lua_State *L, int idx)
{
    return (mrp_lua_mdb_select_t *)mrp_lua_to_object(L, SELECT_CLASS, idx);
}

int mrp_lua_push_select(lua_State *L,mrp_lua_mdb_select_t *sel,bool singleval)
{
    mql_result_t *rslt;
    const char *str;
    lua_Number num;
    char buf[1024];

    if (!singleval)
        mrp_lua_push_object(L, sel);
    else {
        if (!(rslt = sel->result) || sel->nrow < 1)
            lua_pushnil(L);
        else {
            switch (mql_result_rows_get_row_column_type(rslt, 0)) {
            case mqi_string:
                str = mql_result_rows_get_string(rslt, 0, 0, buf,sizeof(buf));
                lua_pushstring(L, str);
                break;
            case mqi_integer:
            case mqi_unsignd:
            case mqi_floating:
                num = mql_result_rows_get_floating(rslt, 0, 0);
                lua_pushnumber(L, num);
                break;
            default:
                lua_pushnil(L);
                break;
            }
        }
    }

    return 1;
}

const char * mrp_lua_select_name(mrp_lua_mdb_select_t *sel)
{
    return (sel && sel->name) ? sel->name : "<unknown>";
}

static int table_create_from_lua(lua_State *L)
{
    mrp_lua_mdb_table_t *tbl;
    size_t fldnamlen;
    const char *fldnam;

    MRP_LUA_ENTER;

    tbl = (mrp_lua_mdb_table_t *)mrp_lua_create_object(L, TABLE_CLASS, NULL);

    tbl->builtin = true;
    tbl->handle = MQI_HANDLE_INVALID;

    MRP_LUA_FOREACH_FIELD(L, 2, fldnam, fldnamlen) {

        switch (field_name_to_type(fldnam, fldnamlen)) {

        case NAME:
            tbl->name = mrp_strdup(luaL_checkstring(L, -1));
            tbl->handle = mqi_get_table_handle((char *)tbl->name);
            break;

        case INDEX:
            tbl->index = mrp_lua_check_strarray(L, -1);
            break;

        case COLUMNS:
            tbl->columns = check_coldefs(L, -1, &tbl->ncolumn);
            break;

        case CREATE:
            if (!lua_isboolean(L, -1)) {
                luaL_error(L, "attempt to assign non-boolean "
                           "value to 'create' field");
            }
            tbl->builtin = !lua_toboolean(L, -1);
            break;

        default:
            luaL_error(L, "unexpected field '%s'", fldnam);
            break;
        }

    } /* MRP_LUA_FOREACH_FIELD */

    if (!tbl->name)
        luaL_error(L, "mandatory 'name' field is unspecified");

    if (tbl->builtin) {
        if (tbl->handle == MQI_HANDLE_INVALID)
            luaL_error(L, "table '%s' do not exist", tbl->name);
        if (tbl->columns && tbl->ncolumn > 0)
            luaL_error(L, "can't specify columns for an existing table");
    }
    else {
        if (tbl->handle != MQI_HANDLE_INVALID) {
            luaL_error(L, "attempt to create an already existing table '%s'",
                       tbl->name);
        }
        if (tbl->columns && tbl->ncolumn > 0) {
            if (!create_mdb_table(tbl))
                luaL_error(L, "failed to create MDB table '%s'", tbl->name);
        }
        else {
            luaL_error(L,"mandatory 'column' field is unspecified or invalid");
        }
    }

    mrp_lua_set_object_name(L, TABLE_CLASS, tbl->name);

    MRP_LUA_LEAVE(1);
}

static int table_getfield(lua_State *L)
{
    mrp_lua_mdb_table_t *tbl = mrp_lua_table_check(L, 1);
    const char *fldnam;
    field_t fld;

    MRP_LUA_ENTER;

    if (lua_type(L, 2) == LUA_TNUMBER) {
        mrp_debug("reading row %d in '%s'", lua_tointeger(L,-1), tbl->name);
        lua_rawget(L, 1);
    }
    else {
        fld = field_check(L, 2, &fldnam);
        lua_pop(L, 1);

        mrp_debug("reading '%s' property of '%s'", fldnam, tbl->name);

        if (!tbl)
            lua_pushnil(L);
        else {
            switch (fld) {
            case NAME:     lua_pushstring(L, tbl->name);                 break;
            case INDEX:    mrp_lua_push_strarray(L, tbl->index);         break;
            case COLUMNS:  push_coldefs(L, tbl->columns, tbl->ncolumn);  break;
            default:       lua_pushnil(L);                               break;
            }
        }
    }

    MRP_LUA_LEAVE(1);
}

static int table_setfield(lua_State *L)
{
    mrp_lua_mdb_table_t *tbl;
    size_t rowidx;

    MRP_LUA_ENTER;

    tbl = mrp_lua_table_check(L, 1);

    if (lua_type(L, 2) != LUA_TNUMBER)
        luaL_error(L, "'%s' is read-only", tbl->name);
    else {
        rowidx = lua_tointeger(L, 2);

        if (rowidx-- < 1)
            luaL_error(L, "invalid row index %u", rowidx);
        if (rowidx > tbl->nrow)
            luaL_error(L, "row index '%u' is out of sequence", rowidx);

        if (rowidx == tbl->nrow) {
            adjust_lua_table_size(L, 1, tbl, tbl->nrow, tbl->nrow+1,
                                  TABLE_ROW_CLASSID);
            tbl->nrow++;
        }
        else {
            lua_pushvalue(L, 2);
            lua_rawget(L, 1);
            luaL_checktype(L, -1, LUA_TTABLE);
        }

        mrp_debug("setting row %u in table '%s'\n", rowidx+1, tbl->name);
    }

    MRP_LUA_LEAVE(1);
}

static int table_tostring(lua_State *L)
{
    mrp_lua_mdb_table_t *tbl;

    MRP_LUA_ENTER;

    tbl = mrp_lua_table_check(L, 1);

    if (tbl && tbl->name)
        lua_pushstring(L, tbl->name);
    else
        lua_pushstring(L, "<error>");

    MRP_LUA_LEAVE(1);
}

static void table_destroy_from_lua(void *data)
{
    mrp_lua_mdb_table_t *tbl = (mrp_lua_mdb_table_t *)data;

    MRP_LUA_ENTER;

    if (tbl) {
        mrp_free((void *)tbl->name);
        mrp_lua_free_strarray(tbl->index);
        free_coldefs(tbl->columns);
    }

    MRP_LUA_LEAVE_NOARG;
}

static void table_row_class_create(lua_State *L)
{
    /* create a metatable for row's */
    luaL_newmetatable(L, TABLE_ROW_CLASSID);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    luaL_openlib(L, NULL, table_row_overrides, 0);
}

#if 0
static int table_row_create(lua_State *L, int tbl, void *data, int rowidx)
{
    int n;

    MRP_LUA_ENTER;

    n = row_create(L, tbl, data, rowidx, TABLE_ROW_CLASSID);

    MRP_LUA_LEAVE(n);
}
#endif

static int table_row_getfield(lua_State *L)
{
    mrp_lua_mdb_table_t *tbl;
    int rowidx;

    MRP_LUA_ENTER;

    tbl = table_row_check(L, 1, &rowidx);

    mrp_debug("reading field in row %d of '%s' table\n", rowidx+1, tbl->name);

    lua_pushnil(L);

    MRP_LUA_LEAVE(1);
}

static int table_row_setfield(lua_State *L)
{
    mrp_lua_mdb_table_t *tbl;
    int rowidx;

    MRP_LUA_ENTER;

    tbl = table_row_check(L, 1, &rowidx);

    mrp_debug("writing field in row %d of '%s' table\n", rowidx+1, tbl->name);

    MRP_LUA_LEAVE(0);
}

static int  table_row_getlength(lua_State *L)
{
    mrp_lua_mdb_table_t *tbl;

    MRP_LUA_ENTER;

    tbl = table_row_check(L, 1, NULL);
    lua_pushinteger(L, tbl->ncolumn);

    MRP_LUA_LEAVE(1);
}


static mrp_lua_mdb_table_t *table_row_check(lua_State *L,
                                            int  idx,
                                            int *ret_rowidx)
{
    row_t *row = row_check(L, idx, TABLE_ROW_CLASSID);

    if (ret_rowidx)
        *ret_rowidx = row->index;

    return (mrp_lua_mdb_table_t *)row->data;
}


static int select_create_from_lua(lua_State *L)
{
    mrp_lua_mdb_select_t *sel;
    size_t fldnamlen;
    const char *fldnam;
    const char *condition;
    char  cols[1024];
    char  qry[2048];

    MRP_LUA_ENTER;

    sel = (mrp_lua_mdb_select_t *)mrp_lua_create_object(L, SELECT_CLASS, NULL);

    MRP_LUA_FOREACH_FIELD(L, 2, fldnam, fldnamlen) {

        switch (field_name_to_type(fldnam, fldnamlen)) {

        case NAME:
            sel->name = mrp_strdup(luaL_checkstring(L, -1));
            break;

        case TABLE:
            sel->table_name = mrp_strdup(luaL_checkstring(L, -1));
            break;

        case COLUMNS:
            sel->columns = mrp_lua_check_strarray(L, -1);
            break;

        case CONDITION:
            condition = luaL_checkstring(L, -1);

            if (strchr(condition, '%'))
                luaL_error(L, "non-static condition '%s'", condition);
            else
                sel->condition = mrp_strdup(condition);
            break;

        default:
            luaL_error(L, "unexpected field '%s'", fldnam);
            break;
        }
    } /* MRP_LUA_FOREACH_FIELD */

    if (!sel->name)
        luaL_error(L, "mandatory 'name' field is missing");
    if (!sel->table_name)
        luaL_error(L, "mandatory 'table' field is missing");
    if (!sel->columns || !sel->columns->nstring)
        luaL_error(L, "mandatory 'column' field is missing or invalid");

    mrp_lua_print_strarray(sel->columns, cols, sizeof(cols));

    if (!sel->condition) {
        snprintf(qry, sizeof(qry), "SELECT %s FROM %s",
                 cols, sel->table_name);
    }
    else {
        snprintf(qry, sizeof(qry), "SELECT %s FROM %s WHERE %s",
                 cols, sel->table_name, sel->condition);
    }

    sel->statement.string = mrp_strdup(qry);

    mrp_lua_set_object_name(L, SELECT_CLASS, sel->name);

    mrp_debug("select '%s' created", sel->name);

    select_install(L, sel);

    select_update(L, -1, sel);

    MRP_LUA_LEAVE(1);
}

static int select_getfield(lua_State *L)
{
    mrp_lua_mdb_select_t *sel = mrp_lua_select_check(L, 1);
    field_t fld;
    const char *fldnam;

    MRP_LUA_ENTER;

    if (!sel)
        lua_pushnil(L);
    else {
        if (lua_type(L, 2) == LUA_TNUMBER) {
            mrp_debug("reading row %d in '%s'", lua_tointeger(L,-1),sel->name);
            lua_rawget(L, 1);
        }
        else {
            fld = field_check(L, 2, &fldnam);
            lua_pop(L, 1);

            mrp_debug("reading property %s in '%s'", fldnam, sel->name);

            if (fld) {
                switch (fld) {
                case NAME:      lua_pushstring(L, sel->name);            break;
                case TABLE:     lua_pushstring(L, sel->table_name);      break;
                case COLUMNS:   mrp_lua_push_strarray(L, sel->columns);  break;
                case CONDITION: lua_pushstring(L, sel->condition);       break;
                case STATEMENT: lua_pushstring(L,sel->statement.string); break;
                case SINGLEVAL: mrp_lua_push_select(L, sel, true);       break;
                default:        lua_pushnil(L);                          break;
                }
            }
            else {
                if (!fldnam || !luaL_getmetafield(L, 1, fldnam))
                    lua_pushnil(L);
            }
        }
    }

    MRP_LUA_LEAVE(1);
}

static int select_setfield(lua_State *L)
{
    mrp_lua_mdb_select_t *sel;

    MRP_LUA_ENTER;

    sel = mrp_lua_select_check(L, 1);

    luaL_error(L, "'%s' is read-only", sel->name);

    MRP_LUA_LEAVE(0);
}


static void select_destroy_from_lua(void *data)
{
    mrp_lua_mdb_select_t *sel = (mrp_lua_mdb_select_t *)data;

    MRP_LUA_ENTER;

    if (sel) {
        mrp_lua_free_strarray(sel->columns);
        mrp_free((void *)sel->name);
        mrp_free((void *)sel->table_name);
        mrp_free((void *)sel->condition);
        mrp_free((void *)sel->statement.string);
    }

    MRP_LUA_LEAVE_NOARG;
}

static int select_update(lua_State *L, int tbl, mrp_lua_mdb_select_t *sel)
{
    mql_statement_t *statement;
    mql_result_t *result;
    int nrow;

    MRP_LUA_ENTER;

    if (!sel->statement.precomp)
        sel->statement.precomp = mql_precompile(sel->statement.string);

    if (!(statement = sel->statement.precomp))
        nrow = 0;
    else {
        mql_result_free(sel->result);
        sel->result = NULL;

        result = mql_exec_statement(mql_result_rows, statement);
        if (!mql_result_is_success(result)) {
            nrow = -mql_result_error_get_code(result);
        }
        else {
            sel->result = result;
            nrow = mql_result_rows_get_row_column_count(result);
        }
    }

    mrp_debug("\"%s\" resulted %d rows", sel->statement.string, nrow);

    if (nrow >= 0) {
        adjust_lua_table_size(L, tbl,sel, sel->nrow, nrow, SELECT_ROW_CLASSID);
        sel->nrow = nrow;
    }

    MRP_LUA_LEAVE(nrow);
}

static int select_update_from_lua(lua_State *L)
{
    mrp_lua_mdb_select_t *sel;
    int nrow;

    MRP_LUA_ENTER;

    sel = mrp_lua_select_check(L, 1);

    mrp_debug("update request for select '%s'\n", sel->name);

    nrow = select_update(L, 1, sel);

    lua_pushinteger(L, nrow < 0 ? 0 : nrow);

    MRP_LUA_LEAVE(1);
}

static int select_update_from_resolver(mrp_scriptlet_t *script,
                                       mrp_context_tbl_t *ctbl)
{
    mrp_lua_mdb_select_t *sel = (mrp_lua_mdb_select_t *)script->data;
    lua_State *L = mrp_lua_get_lua_state();
    int nrow;

    MRP_LUA_ENTER;

    MRP_UNUSED(ctbl);

    if (!sel || !L)
        return -EINVAL;

    mrp_debug("update request for select '%s'", sel->name);

    mrp_lua_push_object(L, sel);

    nrow = select_update(L, -1, sel);

    lua_pop(L, 1);

    MRP_LUA_LEAVE(nrow);
}


static void select_install(lua_State *L, mrp_lua_mdb_select_t *sel)
{
    static mrp_interpreter_t select_updater = {
        { NULL, NULL },
        "select_updater",
        NULL,
        NULL,
        NULL,
        select_update_from_resolver,
        NULL
    };

    mrp_context_t *ctx;
    char target[1024], table[1024];
    const char *depends;

    MRP_LUA_ENTER;

    MRP_UNUSED(L);

    ctx = mrp_lua_get_murphy_context();

    if (ctx == NULL || ctx->r == NULL) {
        printf("Invalid or incomplete murphy context.\n");
        return;
    }

    printf("\nselect_%s: table_%s\n\tupdate(%s)\n",
           sel->name, sel->table_name, sel->name);

    snprintf(target, sizeof(target), "select_%s", sel->name);
    snprintf(table , sizeof(table) , "$%s", sel->table_name);

    depends = table;

    if (!mrp_resolver_add_prepared_target(ctx->r, target, &depends, 1,
                                          &select_updater, NULL, sel)) {
        printf("Failed to install resolver target for element '%s'.\n",
               sel->name);
    }

    MRP_LUA_LEAVE_NOARG;
}

static void select_row_class_create(lua_State *L)
{
    /* create a metatable for row's */
    luaL_newmetatable(L, SELECT_ROW_CLASSID);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    luaL_openlib(L, NULL, select_row_overrides, 0);
}

#if 0
static int select_row_create(lua_State *L, int tbl, void *data, int rowidx)
{
    int n;

    MRP_LUA_ENTER;

    n = row_create(L, tbl, data, rowidx, SELECT_ROW_CLASSID);

    MRP_LUA_LEAVE(n);
}
#endif

static int select_row_getfield(lua_State *L)
{
    mrp_lua_mdb_select_t *sel;
    mrp_lua_strarray_t *cols;
    mql_result_t *rslt;
    const char *fldnam;
    int rowidx;
    int colidx;
    const char *string;
    lua_Number number;
    char buf[1024];

    MRP_LUA_ENTER;

    sel  = select_row_check(L, 1, &rowidx);
    cols = sel->columns;
    rslt = sel->result;

    mrp_debug("reading field in row %d of '%s' selection\n",
              rowidx+1, sel ? sel->name : "<unknwon>");

    if (!sel || !rslt || (size_t)rowidx >= sel->nrow)
        lua_pushnil(L); /* we should never get here actually */


    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        fldnam = lua_tostring(L, 2);
        for (colidx = 0;   colidx < (int)cols->nstring;   colidx++) {
            if (!strcmp(fldnam, cols->strings[colidx]))
                break;
        }
        goto get_data;

    case LUA_TNUMBER:
        colidx = lua_tointeger(L, 2) - 1;
        /* intentional fall through */

    get_data:
        if (colidx < 0 || colidx >= (int)cols->nstring)
            goto no_data;
 
        switch (mql_result_rows_get_row_column_type(rslt, colidx)) {
        case mqi_string:
            string = mql_result_rows_get_string(rslt, colidx, rowidx,
                                                buf, sizeof(buf));
            lua_pushstring(L, string);
            break;
        case mqi_integer:
        case mqi_unsignd:
        case mqi_floating:
            number = mql_result_rows_get_floating(rslt, colidx, rowidx);
            lua_pushnumber(L, number);
            break;
        default:
            goto no_data;
        }
        break;

    default:
    no_data:
        lua_pushnil(L);
        break;
    }

    MRP_LUA_LEAVE(1);
}

static int select_row_setfield(lua_State *L)
{
    mrp_lua_mdb_select_t *sel;
    int rowidx;

    MRP_LUA_ENTER;

    sel = select_row_check(L, 1, &rowidx);

    luaL_error(L, "attempt to write row %u of read-only selection '%s'",
               rowidx+1, sel->name);

    MRP_LUA_LEAVE(0);
}

static int  select_row_getlength(lua_State *L)
{
    mrp_lua_mdb_select_t *sel;

    MRP_LUA_ENTER;

    sel = select_row_check(L, 1, NULL);
    lua_pushinteger(L, sel->columns->nstring);

    MRP_LUA_LEAVE(1);
}


static mrp_lua_mdb_select_t *select_row_check(lua_State *L,
                                              int idx,
                                              int *ret_rowidx)
{
    row_t *row = row_check(L, idx, SELECT_ROW_CLASSID);

    if (ret_rowidx)
        *ret_rowidx = row->index;

    return (mrp_lua_mdb_select_t *)row->data;
}

static bool define_constants(lua_State *L)
{
    static const_def_t const_defs[] = {
        { "string"   , mqi_string   },
        { "integer"  , mqi_integer  },
        { "floating" , mqi_floating },
        {    NULL    , mqi_unknown  }
    };

    const_def_t *cd;
    bool success = false;

    lua_getglobal(L, "mdb");

    if (lua_istable(L, -1)) {
        for (cd = const_defs;   cd->name;   cd++) {
            lua_pushinteger(L, cd->value);
            lua_setfield(L, -2, cd->name);
        }

        lua_pop(L, 1);

        success = true;
    }

    return success;
}

static field_t field_check(lua_State *L, int idx, const char **ret_fldnam)
{
    const char *fldnam;
    size_t fldnamlen;
    field_t fldtyp;

    if (!(fldnam = lua_tolstring(L, idx, &fldnamlen)))
        fldtyp = 0;
    else
        fldtyp = field_name_to_type(fldnam, fldnamlen);

    if (ret_fldnam)
        *ret_fldnam = fldnam;

    return fldtyp;
}

static field_t field_name_to_type(const char *name, size_t len)
{
    switch (len) {

    case 4:
        if (!strcmp(name, "name"))
            return NAME;
        break;

    case 5:
        if (!strcmp(name, "index"))
            return INDEX;
        if (!strcmp(name, "table"))
            return TABLE;
        break;

    case 6:
        if (!strcmp(name, "create"))
            return CREATE;
        break;

    case 7:
        if (!strcmp(name, "columns"))
            return COLUMNS;
        break;

    case 9:
        if (!strcmp(name, "statement"))
            return STATEMENT;
        if (!strcmp(name, "condition"))
            return CONDITION;
        break;

    case 12:
        if (!strcmp(name, "single_value"))
            return SINGLEVAL;
        break;

    default:
        break;
    }

    return 0;
}


static mqi_column_def_t *check_coldefs(lua_State *L, int t, size_t *ret_len)
{
    size_t tlen, dlen;
    size_t size;
    mqi_column_def_t *coldefs, *cd;
    size_t i,j;

    t = (t < 0) ? lua_gettop(L) + t + 1 : t;

    luaL_checktype(L, t, LUA_TTABLE);
    tlen  = lua_objlen(L, t);
    size = sizeof(mqi_column_def_t) * (tlen + 1);

    if (!(coldefs = mrp_alloc(size)))
        luaL_error(L, "can't allocate %d byte long memory", size);
    else {
        memset(coldefs, 0, size);

        for (i = 0;  i < tlen;  i++) {
            cd = coldefs + i;

            lua_pushinteger(L, (int)(i+1));
            lua_gettable(L, t);

            if (!lua_istable(L, -1))
                goto error;

            luaL_checktype(L, -1, LUA_TTABLE);

            dlen = lua_objlen(L, -1);

            for (j = 0;  j < dlen;  j++) {
                lua_pushnumber(L, (int)(j+1));
                lua_gettable(L, -2);

                switch (j) {
                case 0:    cd->name   = mrp_strdup(lua_tostring(L, -1)); break;
                case 1:    cd->type   = lua_tointeger(L, -1);            break;
                case 2:    cd->length = lua_tointeger(L, -1);            break;
                default:   cd->type   = mqi_error;                       break;
                }

                lua_pop(L, 1);
            }

            lua_pop(L, 1);

            if ( cd->name == NULL        ||
                (cd->type != mqi_string  &&
                 cd->type != mqi_integer &&
                 cd->type != mqi_floating ))
                 goto error;
        }

        if (ret_len)
            *ret_len = tlen;
    }

    return coldefs;

 error:
    free_coldefs(coldefs);
    luaL_argerror(L, i+1, "malformed column definition");
    if (ret_len)
        *ret_len = 0;
    return NULL;
}

static int push_coldefs(lua_State *L, mqi_column_def_t *coldefs, size_t hint)
{
    mqi_column_def_t *cd;
    int i;

    if (!coldefs)
        lua_pushnil(L);
    else {
        lua_createtable(L, hint, 0);

        for (cd = coldefs, i = 1;  cd->name;   cd++, i++) {
            lua_pushinteger(L, i);

            lua_createtable(L, cd->length ? 3 : 2, 0);

            lua_pushinteger(L, 1);
            lua_pushstring(L, cd->name);
            lua_settable(L, -3);

            lua_pushinteger(L, 2);
            lua_pushinteger(L, cd->type);
            lua_settable(L, -3);

            if (cd->length) {
                lua_pushinteger(L, 3);
                lua_pushinteger(L, cd->length);
                lua_settable(L, -3);
            }

            lua_settable(L, -3);
        }
    }

    return 1;
}

static void free_coldefs(mqi_column_def_t *coldefs)
{
    mqi_column_def_t *cd;

    if (coldefs) {
        for (cd = coldefs;  cd->name;  cd++)
            mrp_free((void *)cd->name);
        mrp_free(coldefs);
    }
}

static int row_create(lua_State *L, int tbl, void *data, int rowidx,
                      const char *class_id)
{
    row_t *row;

    tbl = (tbl < 0) ? lua_gettop(L) + tbl + 1 : tbl;

    luaL_checktype(L, tbl, LUA_TTABLE);

    row = lua_newuserdata(L, sizeof(row_t));

    row->data = data;
    row->index = rowidx;

    luaL_getmetatable(L, class_id);
    lua_setmetatable(L, -2);

    return 1;
}

static row_t *row_check(lua_State *L, int  idx, const char *class_id)
{
    row_t *row;

    idx = (idx < 0) ? lua_gettop(L) + idx + 1 : idx;

    row = (row_t *)luaL_checkudata(L, idx, class_id);

    return row;
}


static void adjust_lua_table_size(lua_State *L,
                                  int tbl,
                                  void *data,
                                  size_t old_size,
                                  size_t new_size,
                                  const char *class_id)
{
    size_t rowidx;

    tbl = (tbl < 0) ? lua_gettop(L) + tbl + 1 : tbl;

    luaL_checktype(L, tbl, LUA_TTABLE);

    if (old_size < new_size) {
        for (rowidx = old_size;   rowidx < new_size;   rowidx++) {
            lua_pushinteger(L, (int)(rowidx+1));
            row_create(L, tbl, data, rowidx, class_id);
            lua_rawset(L, tbl);
        }
        return;
    }

    if (old_size > new_size) {
        for (rowidx = old_size - 1;  rowidx >= new_size;   rowidx--) {
            lua_pushinteger(L, (int)(rowidx+1));
            lua_pushnil(L);
            lua_rawset(L, tbl);
        }
        return;
    }
}

static bool create_mdb_table(mrp_lua_mdb_table_t *tbl)
{
    char **index;

    if (!tbl->columns || !tbl->ncolumn)
        tbl->handle = MQI_HANDLE_INVALID;
    else {
        if (!tbl->index || !tbl->index->nstring)
            index = NULL;
        else
            index = (char **)tbl->index->strings;

        tbl->handle = mqi_create_table((char *)tbl->name, MQI_TEMPORARY,
                                       index, tbl->columns);

        if (tbl->handle == MQI_HANDLE_INVALID)
            mrp_debug("failed to create table '%s'", tbl->name);
        else
            mrp_debug("table '%s' has been sucessfully created", tbl->name);
    }

    return (tbl->handle != MQI_HANDLE_INVALID);
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
