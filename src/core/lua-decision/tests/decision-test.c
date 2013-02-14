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

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common.h>

#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-utils/strarray.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-decision/mdb.h>
#include <murphy/core/lua-decision/element.h>


#define VOLUME_CLASS   MRP_LUA_CLASS(volume, limit)


typedef enum {
    DEVICE = 1,
    STREAM
} volume_type_t;

typedef enum {
    TYPE = 1,
    NAME,
    DEVICES,
    STREAMS,
    LIMIT,
    UPDATE,
} volume_field_t;

typedef struct volume_s {
    volume_type_t       type;
    const char         *name;
    mrp_lua_strarray_t *nodes;
    double              limit;
    mrp_funcbridge_t   *update;
    void               *user_data;
} volume_t;

static int  volume_create(lua_State *);
static int  volume_getfield(lua_State *);
static int  volume_setfield(lua_State *);
static void volume_destroy(void *);

MRP_LUA_METHOD_LIST_TABLE (
   volume_methods,
   MRP_LUA_METHOD_CONSTRUCTOR  (volume_create)
);

MRP_LUA_METHOD_LIST_TABLE (
   volume_overrides,
   MRP_LUA_OVERRIDE_CALL       (volume_create)
   MRP_LUA_OVERRIDE_GETFIELD   (volume_getfield)
   MRP_LUA_OVERRIDE_SETFIELD   (volume_setfield)
);


MRP_LUA_CLASS_DEF (
   volume,                      /* class name */
   limit,                       /* constructor name */
   volume_t,                    /* userdata type */
   volume_destroy,              /* userdata destructor */
   volume_methods,
   volume_overrides
);


static size_t    nvol;
static volume_t *vols[5];

static int volume_create(lua_State *L)
{
    int         table;
    size_t      fldnamlen;
    const char *fldnam;
    volume_t   *vol;

    vol = (volume_t *)mrp_lua_create_object(L, VOLUME_CLASS, NULL,0);

    table = lua_gettop(L);

    MRP_LUA_FOREACH_FIELD(L, 2, fldnam, fldnamlen) {

        switch (fldnamlen) {
        case 7:
            if (!strcmp(fldnam, "devices")) {
                if (vol->nodes) {
                    return luaL_error(L, "streams and devices are "
                                      "mutually exclusive");
                }
                vol->type  = DEVICE;
                vol->nodes = mrp_lua_check_strarray(L, -1);
                break;
            }
            if (!strcmp(fldnam, "streams")) {
                if (vol->nodes) {
                    return luaL_error(L, "streams and devices are "
                                      "mutually exclusive");
                }
                vol->type  = STREAM;
                vol->nodes = mrp_lua_check_strarray(L, -1);
                break;
            }
            goto not_userdata;

        case 6:
            if (!strcmp(fldnam, "update")) {
                vol->update = mrp_funcbridge_create_luafunc(L, -1);
                break;
            }
            goto not_userdata;

        case 5:
            if (!strcmp(fldnam, "limit")) {
                vol->limit = luaL_checknumber(L, -1);
                break;
            }
            goto not_userdata;

        case 4:
            if (!strcmp(fldnam, "type")) {
                return luaL_error(L, "type field is readonly");
            }
            if (!strcmp(fldnam, "name")) {
                vol->name = luaL_checklstring(L, -1, NULL);
                break;
            }
            goto not_userdata;

        default:
        not_userdata:
            lua_pushvalue(L, -2);
            lua_pushvalue(L, -2);
            lua_rawset(L, table);
            break;
        }
    } /* MRP_LUA_FOREACH_FIELD */

    if (!vol->type || !vol->nodes)
        return luaL_error(L, "Either streams or devices must be present");
    if (!vol->name)
        return luaL_error(L, "name is not present");

    mrp_lua_set_object_name(L, VOLUME_CLASS, vol->name);

    vols[nvol++] = vol;

    printf("volume %p\n", vol);

    return 1;
}

static volume_t *checkvolume(lua_State *L)
{
    return (volume_t *)mrp_lua_check_object(L, VOLUME_CLASS, 1);
}

static volume_field_t checkfield(lua_State *L)
{
    size_t      len;
    const char *name;

    name = luaL_checklstring(L, 2, &len);

    switch (len) {
    case 4:
        if (!strcmp(name, "type"))
            return TYPE;
        if (!strcmp(name, "name"))
            return NAME;
        break;
    case 5:
        if (!strcmp(name, "limit"))
            return LIMIT;
        break;
    case 6:
        if (!strcmp(name, "update"))
            return UPDATE;
        break;
    case 7:
        if (!strcmp(name, "streams"))
            return STREAMS;
        if (!strcmp(name, "devices"))
            return DEVICES;
        break;
    default:
        break;
    }

    return 0;
}

static void volume_destroy(void *data)
{
    volume_t *vol = (volume_t *)data;
    size_t i;

    printf("*** volume destroyed\n");

    mrp_lua_free_strarray(vol->nodes);

    for (i = 0; i < nvol; i++) {
        if (vols[i] == vol) {
            while ((i+1) < sizeof(vols)/sizeof(vols[0]) && vols[i+1]) {
                vols[i] = vols[i+1];
                i++;
            }
            vols[i] = NULL;
            break;
        }
    }
}

static const char *volumetype2str(volume_type_t type)
{
    switch (type) {
    case DEVICE:     return "device";
    case STREAM:     return "stream";
    default:         return "<unknown>";
    }
}


static int volume_getfield(lua_State *L)
{
    volume_t         *vol = checkvolume(L);
    volume_field_t fld = checkfield(L);
    char buf[256];

    printf("index %d for %s volume (node %s) \n",
           fld, volumetype2str(vol->type),
           mrp_lua_print_strarray(vol->nodes, buf, sizeof(buf)));

    switch (fld) {
    case TYPE:
        lua_pushstring(L, volumetype2str(vol->type));
        break;
    case STREAMS:
        if (vol->type == STREAM)
            mrp_lua_push_strarray(L, vol->nodes);
        else
            lua_pushnil(L);
        break;
    case DEVICES:
        if (vol->type == DEVICE)
            mrp_lua_push_strarray(L, vol->nodes);
        else
            lua_pushnil(L);
        break;
    case LIMIT:
        lua_pushnumber(L, vol->limit);
        break;
    case UPDATE:
        mrp_funcbridge_push(L, vol->update);
        break;
    default:
        lua_pushvalue(L, 2);
        lua_rawget(L, 1);
        break;
    }

    return 1;
}

static int volume_setfield(lua_State *L)
{
    volume_t       *vol = checkvolume(L);
    volume_field_t  fld = checkfield(L);
    char            buf[256];

    printf("new index %d for %s volume (node %s) \n",
           fld, volumetype2str(vol->type),
           mrp_lua_print_strarray(vol->nodes, buf, sizeof(buf)));

    switch (fld) {
    case STREAMS:
        if (vol->type != STREAM) {
            return luaL_error(L, "attempt to set sterams for device "
                              "volume limit");
        }
        goto set_nodes;
    case DEVICES:
        if (vol->type != STREAM) {
            return luaL_error(L, "attempt to set sterams for device "
                              "volume limit");
        }
        goto set_nodes;
    set_nodes:
        mrp_lua_free_strarray(vol->nodes);
        vol->nodes = mrp_lua_check_strarray(L, 3);
        break;
    case LIMIT:
        vol->limit = luaL_checknumber(L, 3);
        break;
    case UPDATE:
        vol->update = mrp_funcbridge_create_luafunc(L, 3);
    default:
        lua_rawset(L, 1);
        break;
    }

    return 0;
}


static void volume_openlib(lua_State *L)
{

    mrp_lua_create_object_class(L, VOLUME_CLASS);
}


bool my_update_func(lua_State *L, void *data,
                    const char *signature, mrp_funcbridge_value_t *args,
                    char  *ret_type, mrp_funcbridge_value_t *ret_val)
{
    MRP_UNUSED(L);

    printf("**** %s(%p) signature='%s' arg1=%p arg2='%s'\n",
           __FUNCTION__, data, signature,
           signature[0] == 'o' ? args[0].pointer : NULL,
           signature[1] == 's' ? args[1].string  : "<undefined>");

    *ret_type = MRP_FUNCBRIDGE_FLOATING;
    ret_val->floating = 3.1415;

    return true;
}

int main(int argc, char **argv)
{
    mrp_funcbridge_value_t args[] = {
        {.pointer = NULL },
        {.string  = "Hello world, here I am"}
    };

    const char *pnam = basename(argv[0]);
    lua_State *L;
    char buf[512];
    int error;
    volume_t *v;
    mrp_funcbridge_t *fb;
    mrp_funcbridge_value_t ret;
    char t;

    if (argc > 2) {
        printf("Usage: %s [file]\n", pnam);
        exit(1);
    }

    if (!(L = luaL_newstate())) {
        printf("failed to initialize Lua\n");
        exit(1);
    }

    printf("Lua initialized\n");

    luaL_openlibs(L);
    mrp_create_funcbridge_class(L);
    mrp_lua_create_mdb_class(L);
    mrp_lua_create_element_class(L);
    volume_openlib(L);

    mrp_funcbridge_create_cfunc(L, "my_update_func", "os",
                                my_update_func, (void *)0x1234);

    if (argc == 2) {
        error = luaL_loadfile(L, argv[1]) ||
                lua_pcall(L, 0, 0, 0);
        if (error) {
            printf("%s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }

        if ((v = args[0].pointer = vols[0]) && (fb = v->update)) {
            char value[32];
            if (!mrp_funcbridge_call_from_c(L, fb, "os", args, &t, &ret))
                printf("*** call failed: %s\n", ret.string);
            else {
                switch (t) {
                case MRP_FUNCBRIDGE_NO_DATA:
                    snprintf(value, sizeof(value), "<no data>");
                    break;
                case MRP_FUNCBRIDGE_STRING:
                    snprintf(value, sizeof(value), "%s", ret.string);
                    break;
                case MRP_FUNCBRIDGE_INTEGER:
                    snprintf(value, sizeof(value), "%d", ret.integer);
                    break;
                case MRP_FUNCBRIDGE_FLOATING:
                    snprintf(value, sizeof(value), "%lf", ret.floating);
                    break;
                default:
                    snprintf(value, sizeof(value), "<unsupported>");
                    break;
                }
                printf("*** return value %s\n", value);
            }
        }
    }
    else {
        printf("%s> ", pnam);
        fflush(stdout);

        while (fgets(buf, sizeof(buf), stdin)) {
            error = luaL_loadbuffer(L, buf, strlen(buf), "line") ||
                    lua_pcall(L, 0, 0, 0);
            if (error) {
                printf("%s\n", lua_tostring(L, -1));
                lua_pop(L, 1);
            }

            printf("%s> ", pnam);
            fflush(stdout);
        }
    }

    lua_close(L);

    return 0;
}
