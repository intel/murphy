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
#include <alloca.h>

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common.h>
#include <murphy/common/debug.h>

#include <murphy/core/context.h>
#include <murphy/core/scripting.h>
#include <murphy/core/lua-decision/element.h>
#include <murphy/core/lua-decision/mdb.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-utils/strarray.h>
#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-bindings/murphy.h>


#define ELEMENT_CLASS           MRP_LUA_CLASS(element, lua)

#define ELEMENT_INPUT_CLASSID   MRP_LUA_CLASSID_ROOT "element_input"
#define ELEMENT_OUTPUT_CLASSID  MRP_LUA_CLASSID_ROOT "element_output"

#define ELEMENT_IDX 1
#define INPUT_IDX   1
#define OUTPUT_IDX  2

#define INPUT_MAX      (sizeof(mrp_lua_element_mask_t) * 8)
#define INPUT_BIT(_i)  (((mrp_lua_element_mask_t)1) << (_i))
#define INPUT_MASK(_n) (INPUT_BIT(_n) - 1)

typedef enum   field_e        field_t;
typedef enum   input_type_e   input_type_t;


enum field_e {
    NAME = 1,
    INPUTS,
    OUTPUTS,
    UPDATE,
};

enum input_type_e {
    NUMBER = MRP_FUNCBRIDGE_FLOATING,
    STRING = MRP_FUNCBRIDGE_STRING,
    SELECT = MRP_FUNCBRIDGE_OBJECT,
};

struct mrp_lua_element_input_s {
    const char *name;
    input_type_t  type;
    union {
        mrp_funcbridge_value_t constant;
        mrp_lua_mdb_select_t *select;
    };
};


struct mrp_lua_element_s {
    MRP_LUA_ELEMENT_FIELDS;
};


static int  element_create_from_lua(lua_State *);
static int  element_getfield(lua_State *);
static int  element_setfield(lua_State *);
static int  element_tostring(lua_State *);
static void element_destroy_from_lua(void *);
static mrp_lua_element_t *element_check(lua_State *, int);
static void element_install(lua_State *, mrp_lua_element_t *);

static void element_input_class_create(lua_State *);
static int  element_input_create_luatbl(lua_State *, int);
static int  element_input_getfield(lua_State *);
static int  element_input_setfield(lua_State *);
static mrp_lua_element_input_t *element_input_create_userdata(lua_State *,
                                                     int, size_t *,
                                                     mrp_lua_element_mask_t *);

static mrp_lua_mdb_table_t **element_output_check(lua_State *, int, size_t *);


static field_t field_check(lua_State *, int, const char **);
static field_t field_name_to_type(const char *, size_t);

MRP_LUA_METHOD_LIST_TABLE (
    element_methods,         /* methodlist name */
    MRP_LUA_METHOD_CONSTRUCTOR  (element_create_from_lua)
);

MRP_LUA_METHOD_LIST_TABLE (
    element_overrides,       /* methodlist name */
    MRP_LUA_OVERRIDE_CALL       (element_create_from_lua)
    MRP_LUA_OVERRIDE_GETFIELD   (element_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (element_setfield)
    MRP_LUA_OVERRIDE_STRINGIFY  (element_tostring)
);

MRP_LUA_METHOD_LIST_TABLE (
    element_input_overrides, /* methodlist name */
    MRP_LUA_OVERRIDE_GETFIELD   (element_input_getfield)
    MRP_LUA_OVERRIDE_SETFIELD   (element_input_setfield)
);


MRP_LUA_CLASS_DEF (
    element,                     /* class name */
    lua,                         /* constructor name */
    mrp_lua_element_t,           /* userdata type */
    element_destroy_from_lua,    /* userdata destructor */
    element_methods,             /* class methods */
    element_overrides            /* override methods */
);


void mrp_lua_create_element_class(lua_State *L)
{
    mrp_lua_create_object_class(L, ELEMENT_CLASS);

    element_input_class_create(L);
}


static int element_create_from_lua(lua_State *L)
{
    mrp_lua_element_t *el;
    int table;
    size_t fldnamlen;
    const char *fldnam;

    MRP_LUA_ENTER;

    el = (mrp_lua_element_t *)mrp_lua_create_object(L, ELEMENT_CLASS, NULL);
    table = lua_gettop(L);

    lua_pushinteger(L, INPUT_IDX);
    element_input_create_luatbl(L, table);
    lua_rawset(L, table);

    MRP_LUA_FOREACH_FIELD(L, 2, fldnam, fldnamlen) {

        switch (field_name_to_type(fldnam, fldnamlen)) {

        case NAME:
            el->name = mrp_strdup(luaL_checkstring(L, -1));
            break;

        case INPUTS:
            el->inputs = element_input_create_userdata(L, -1, &el->ninput,
                                                       &el->inpmask);
            break;

        case OUTPUTS:
            el->outputs = element_output_check(L, -1, &el->noutput);
            break;

        case UPDATE:
            el->update = mrp_funcbridge_create_luafunc(L, -1);
            break;

        default:
            lua_pushvalue(L, -2);
            lua_pushvalue(L, -2);
            lua_rawset(L, table);
            break;
        }

    } /* MRP_LUA_FOREACH_FIELD */

    if (!el->name)
        luaL_error(L, "missing mandatory 'name' field");
    if (!el->inputs || !el->ninput)
        luaL_error(L, "missing or empty manadatory 'input' field");
    if (!el->outputs || !el->noutput)
        luaL_error(L, "missing or empty manadatory 'output' field");
    if (!el->update)
        luaL_error(L, "missing or invalid mandatory 'update' field");

    mrp_lua_set_object_name(L, ELEMENT_CLASS, el->name);

    mrp_debug("element '%s' created", el->name);

    if (el->inpmask == INPUT_MASK(el->ninput))
        element_install(L, el);

    MRP_LUA_LEAVE(1);
}

static int element_getfield(lua_State *L)
{
    mrp_lua_element_t *el;
    field_t fld;

    MRP_LUA_ENTER;

    el  = element_check(L, 1);
    fld = field_check(L, 2, NULL);
    lua_pop(L, 1);

    switch (fld) {
    case NAME:      lua_pushstring(L, el->name);          break;
    case INPUTS:    lua_rawgeti(L, 1, INPUT_IDX);         break;
    case OUTPUTS:   lua_pushnil(L);                       break;
    case UPDATE:    mrp_funcbridge_push(L, el->update);   break;
    default:        lua_pushnil(L);                       break;
    }

    MRP_LUA_LEAVE(1);
}

static int element_setfield(lua_State *L)
{
    mrp_lua_element_t *el;

    MRP_LUA_ENTER;

    el = element_check(L, 1);
    luaL_error(L, "'%s' is read-only", el->name);

    MRP_LUA_LEAVE(0);
}

static int element_tostring(lua_State *L)
{
    mrp_lua_element_t *el;

    MRP_LUA_ENTER;

    if ((el = element_check(L, 1)) && el->name)
        lua_pushstring(L, el->name);
    else
        lua_pushstring(L, "<error>");

    MRP_LUA_LEAVE(1);
}

static void element_destroy_from_lua(void *data)
{
    mrp_lua_element_t *el = (mrp_lua_element_t *)data;

    MRP_LUA_ENTER;

    if (el) {
        mrp_free((void *)el->name);
    }

    MRP_LUA_LEAVE_NOARG;
}

static mrp_lua_element_t *element_check(lua_State *L, int idx)
{
    return (mrp_lua_element_t *)mrp_lua_check_object(L, ELEMENT_CLASS, idx);
}

static int element_update_cb(mrp_scriptlet_t *script, mrp_context_tbl_t *ctbl)
{
    mrp_lua_element_t *el = (mrp_lua_element_t *)script->data;

    MRP_UNUSED(ctbl);

    mrp_debug("'%s'", el->name);

    return TRUE;
}


static void element_install(lua_State *L, mrp_lua_element_t *el)
{
    static mrp_interpreter_t element_updater = {
        { NULL, NULL },
        "element_updater",
        NULL,
        NULL,
        NULL,
        element_update_cb,
        NULL
    };

    mrp_lua_element_input_t *inp;
    mrp_context_t *ctx;
    size_t i;
    char buf[1024], target[1024];
    const char **depends, *d;
    char *dep;
    int ndepend;
    char *p, *e;
    size_t len;

    MRP_UNUSED(L);

    MRP_LUA_ENTER;

    ctx = mrp_lua_get_murphy_context();

    if (ctx == NULL || ctx->r == NULL) {
        mrp_log_error("Invalid or incomplete murphy context");
        return;
    }

    depends = alloca(el->ninput * sizeof(depends[0]));
    ndepend = 0;

    for (i = 0, e = (p = buf) + sizeof(buf);  i < el->ninput && p < e;  i++) {
        inp = el->inputs + i;

        if (inp->type == SELECT) {
            d  = mrp_lua_select_name(inp->select);
            p += snprintf(p, e-p, " _select_%s", d);

            len = strlen(d) + 7 + 1;
            depends[ndepend++] = dep = alloca(len);
            sprintf(dep, "_select_%s", d);
        }
    }

    for (i = 0;   i < el->noutput;  i++) {
        snprintf(target, sizeof(target), "_table_%s",
                 mrp_lua_table_name(el->outputs[i]));

        printf("\%s:%s\n\tupdate(%s)\n\n", target, buf, el->name);


        if (!mrp_resolver_add_prepared_target(ctx->r, target, depends, ndepend,
                                              &element_updater, NULL, el)) {
            printf("Failed to install resolver target for element '%s'.\n",
                   el->name);
        }
    }

    MRP_LUA_LEAVE_NOARG;
}

static void element_input_class_create(lua_State *L)
{
    /* create a metatable for input's */
    luaL_newmetatable(L, ELEMENT_INPUT_CLASSID);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    luaL_openlib(L, NULL, element_input_overrides, 0);
}

static int element_input_create_luatbl(lua_State *L, int el)
{
    MRP_LUA_ENTER;

    el = (el < 0) ? lua_gettop(L) + el + 1 : el;

    luaL_checktype(L, el, LUA_TTABLE);

    lua_createtable(L, 2, 0);

    luaL_getmetatable(L, ELEMENT_INPUT_CLASSID);
    lua_setmetatable(L, -2);

    lua_pushinteger(L, ELEMENT_IDX);
    lua_pushvalue(L, el);
    lua_rawset(L, -3);

    MRP_LUA_LEAVE(0);
}

static int element_input_getfield(lua_State *L)
{
    mrp_lua_element_t *el;
    const char *inpnam;
    mrp_lua_element_input_t *inp;
    size_t i;

    MRP_LUA_ENTER;

    lua_rawgeti(L, 1, INPUT_IDX);
    el = element_check(L, -1);
    lua_pop(L, 1);

    inpnam = luaL_checklstring(L, 2, NULL);

    mrp_debug("reading %s.inputs.%s", el->name, inpnam);

    for (i = 0;  i < el->ninput;  i++) {
        inp = el->inputs + i;
        if (!strcmp(inpnam, inp->name)) {
            switch (inp->type) {
            case NUMBER:    lua_pushnumber(L, inp->constant.floating);   break;
            case STRING:    lua_pushstring(L, inp->constant.string);     break;
            case SELECT:    mrp_lua_push_select(L, inp->select, false);  break;
            default:        lua_pushnil(L);                              break;
            }
            return 1;
        }
    }

    lua_pushnil(L);

    MRP_LUA_LEAVE(1);
}

static int element_input_setfield(lua_State *L)
{
    mrp_lua_element_t *el;
    const char *inpnam;
    mrp_lua_element_input_t *inp;
    size_t i;

    MRP_LUA_ENTER;

    lua_rawgeti(L, 1, INPUT_IDX);
    el = element_check(L, -1);
    lua_pop(L, 1);

    inpnam = luaL_checklstring(L, 2, NULL);

    mrp_debug("writing %s.inputs.%s", el->name, inpnam);

    for (i = 0; i < el->ninput;  i++) {
        inp = el->inputs + i;

        if (!strcmp(inpnam, inp->name)) {
            luaL_argcheck(L, !inp->type, 1, "input already assigned");

            switch (lua_type(L, 3)) {
            case LUA_TNUMBER:
                inp->type = NUMBER;
                inp->constant.floating = lua_tonumber(L, 3);
                break;
            case LUA_TSTRING:
                inp->type = STRING;
                inp->constant.string = lua_tolstring(L, 3, NULL);
                break;
            case LUA_TTABLE:
                if ((inp->select = mrp_lua_to_select(L, 3))) {
                    inp->type = SELECT;
                    break;
                }
                /* intentional fall through */
            default:
                luaL_error(L, "invalid input type '%s' for %s",
                           lua_typename(L, lua_type(L, 3)), inpnam);
                break;
            } /* switch type */

            if ((el->inpmask |= INPUT_BIT(i)) == INPUT_MASK(el->ninput))
                element_install(L, el);

            break;
        }
    } /* for inp */

    MRP_LUA_LEAVE(0);
}

static mrp_lua_element_input_t *element_input_create_userdata(lua_State *L,
                                                              int idx,
                                                              size_t *ret_len,
                                           mrp_lua_element_mask_t *ret_inpmask)
{
    mrp_lua_element_input_t arr[INPUT_MAX + 1], *i, *inp;
    mrp_lua_element_mask_t inpmask;
    const char *name;
    size_t namlgh;
    size_t len;

    idx = (idx < 0) ? lua_gettop(L) + idx + 1 : idx;
    len = 0;
    inpmask = 0;

    luaL_checktype(L, idx, LUA_TTABLE);

    memset(arr, 0, sizeof(arr));

    MRP_LUA_FOREACH_FIELD(L, idx, name, namlgh) {
        if (len >= INPUT_MAX)
            luaL_error(L, "too many inputs (max %d allowes)", INPUT_MAX);

        i = arr + len++;

        if (namlgh < 1) {
            if (lua_type(L, -1) == LUA_TSTRING)
                i->name = mrp_strdup(luaL_checkstring(L, -1));
            else {
                luaL_error(L, "invalid type '%s' for input name",
                           lua_typename(L, lua_type(L, -1)));
            }
        }
        else {
            switch (lua_type(L, -1)) {

            case LUA_TNUMBER:
                i->name = mrp_strdup(name);
                i->type = NUMBER;
                i->constant.floating = luaL_checknumber(L, -1);
                break;

            case LUA_TSTRING:
                i->name = mrp_strdup(name);
                i->type = STRING;
                i->constant.string = mrp_strdup(luaL_checkstring(L, -1));
                break;

            case LUA_TTABLE:
                i->name = mrp_strdup(name);
                i->type = SELECT;
                i->select = mrp_lua_select_check(L, -1);
                break;

            default:
                luaL_error(L, "invalid input type %s",
                           lua_typename(L, lua_type(L, -1)));
                break;
            }

            inpmask |= INPUT_BIT(len - 1);
        }
    } /* MRP_LUA_FOREACH_FIELD */

    if (!(inp = mrp_alloc(sizeof(mrp_lua_element_input_t) * (len + 1))))
        luaL_error(L, "can't allocate memory");

    memcpy(inp, arr, sizeof(mrp_lua_element_input_t) * len);
    memset(inp + len, 0, sizeof(mrp_lua_element_input_t));

    if (ret_len)
        *ret_len = len;

    if (ret_inpmask)
        *ret_inpmask = inpmask;

    return inp;
}

static mrp_lua_mdb_table_t **element_output_check(lua_State *L,
                                                  int idx,
                                                  size_t *ret_len)
{
    mrp_lua_mdb_table_t **arr;
    size_t len, i;
    size_t size;

    luaL_checktype(L, idx, LUA_TTABLE);
    len  = luaL_getn(L, idx);
    size = sizeof(mrp_lua_mdb_table_t *) * (len + 1);

    if (!(arr = mrp_alloc(size)))
        luaL_error(L, "can't allocate %d byte long memory", size);

    lua_pushvalue(L, idx);

    for (i = 0;  i < len;  i++) {
        lua_pushnumber(L, (int)(i+1));
        lua_gettable(L, -2);

        arr[i] = mrp_lua_table_check(L, -1);

        lua_pop(L, 1);
    }

    arr[i] = NULL;

    lua_pop(L, 1);

    if (ret_len)
        *ret_len = len;

    return arr;
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

    case 6:
        if (!strcmp(name, "inputs"))
            return INPUTS;
        if (!strcmp(name, "update"))
            return UPDATE;
        break;

    case 7:
        if (!strcmp(name, "outputs"))
            return OUTPUTS;
        break;

    default:
        break;
    }

    return 0;
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
