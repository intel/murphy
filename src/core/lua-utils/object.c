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
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/env.h>
#include <murphy/common/mm.h>
#include <murphy/common/refcnt.h>

#include <murphy/core/lua-bindings/murphy.h>
#include <murphy/core/lua-utils/object.h>
#include <murphy/core/lua-utils/error.h>


#undef  __MURPHY_MANGLE_CLASS_SELF__     /* extra self-mangling if defined */
#define CHECK    true                    /* do type/self-checking */
#define NOCHECK (!CHECK)                 /* omit type/self-checking */

/**
 * Metadata we use to administer objects allocated via us.
 */
typedef struct {
    void *selfish;                       /* verification pointer(ish) to us */
    mrp_lua_classdef_t *def;             /* class definition for this object */
    struct {
        int self;                        /* self reference for static objects */
        int ext;                         /* object extensions */
        int priv;                        /* private references */
    } refs;
    mrp_refcnt_t refcnt;                 /* object reference count */
    int  dead : 1;                       /* being cleaned up */
    int  initializing : 1;               /* being initialized */
    mrp_list_hook_t hook[0];             /* to object list if we're tracking */
} userdata_t;


static bool valid_id(const char *);
static int  userdata_destructor(lua_State *);

static void object_create_reftbl(userdata_t *u, lua_State *L);
static void object_delete_reftbl(userdata_t *u, lua_State *L);
static void object_create_exttbl(userdata_t *u, lua_State *L);
static void object_delete_exttbl(userdata_t *u, lua_State *L);
static int  override_setfield(lua_State *L);
static int  override_getfield(lua_State *L);
static int override_tostring(lua_State *L);
static int  object_setup_bridges(userdata_t *u, lua_State *L);

static void invalid_destructor(void *data);
static inline int is_native(userdata_t *u, const char *name);

/**
 * A static non-NULL class definition we return for failed lookups.
 */
static mrp_lua_classdef_t invalid_classdef = {
    .class_name    = "<invalid class>",
    .class_id      = "<invalid class-id>",
    .constructor   = "<invalid constructor>",
    .destructor    = invalid_destructor,
    .type_name     = "<invalid class type>",
    .type_id       = MRP_LUA_NONE,
    .userdata_id   = "<invalid userdata>",
    .userdata_size = 0,
    .methods       = NULL,
    .overrides     = NULL,
    .members       = NULL,
    .nmember       = 0,
    .natives       = NULL,
    .nnative       = 0,
    .notify        = NULL,
    .flags         = 0,
}, *invalid_class = &invalid_classdef;


/**
 * object infra configurable settings
 */
static struct {
    bool track;                          /* track objects per classdef */
    bool set;                            /* whether already set */
    bool busy;                           /* whether taken into use */
} cfg;


/**
 * indirect table to look up classdefs by type_ids
 */
static mrp_lua_classdef_t **classdefs;
static int                  nclassdef;

/**
 * Macros to convert between userdata and user-visible data addresses.
 */

#define USERDATA_SIZE                                                   \
    (cfg.track?MRP_OFFSET(userdata_t, hook[1]):MRP_OFFSET(userdata_t, hook[0]))

#define USER_TO_DATA(u) user_to_data(u)
#define DATA_TO_USER(d) data_to_user(d)

static inline void *user_to_data(userdata_t *u)
{
    if (u != NULL) {
        if (cfg.track)
            return &((userdata_t *)(u))->hook[1];
        else
            return &((userdata_t *)(u))->hook[0];
    }
    else
        return NULL;
}

static inline userdata_t *data_to_user(void *d)
{
    if (d != NULL)
        return ((userdata_t *)(((void *)d) - USERDATA_SIZE));
    else
        return NULL;
}


/** Check our configuration from the environment. */
static void check_config(void)
{
    char *config = getenv(MRP_LUA_CONFIG_ENVVAR);

    if (mrp_env_config_bool(config, "track", false))
        mrp_lua_track_objects(true);
}


/** Encode our self(ish) pointer. */
static inline void userdata_setself(userdata_t *u)
{
#ifdef __MURPHY_MANGLE_CLASS_SELF__
    void *data = USER_TO_DATA(u);

    u->selfish = (void *)(((ptrdiff_t)u) ^ ((ptrdiff_t)data));
#else
    u->selfish = u;
#endif
}

/** Decode our self(ish) pointer, return NULL if the most basic check fails. */
static inline void *userdata_getself(userdata_t *u)
{
#ifdef __MURPHY_MANGLE_CLASS_SELF__
    void *data = USER_TO_DATA(u);
    void *self = u ? (void *)(((ptrdiff_t)u->selfish) ^ ((ptrdiff_t)data)):NULL;
#else
    void *self = u ? u->selfish : NULL;
#endif

    if (u == self)
        return self;
    else
        return NULL;
}

/** Check if the give pointer appears to point to a valid userdata. */
static inline bool valid_userdata(userdata_t *u)
{
    return (userdata_getself(u) == u);
}

/** Obtain userdata for a data pointer, optionally checking basic validity. */
static inline userdata_t *userdata_get(void *data, bool check)
{
    userdata_t *u;

    if (data != NULL) {
        u = DATA_TO_USER(data);

        if (!check || userdata_getself(u) == u)
            return u;
    }

    return NULL;
}

/** Obtain data for a userdata pointer, optionally checking for validity. */
static inline void *object_get(userdata_t *u, bool check)
{
    if (u != NULL) {
        if (!check || (userdata_getself(u) == u))
            return USER_TO_DATA(u);
    }

    return NULL;
}


/** Create and register a new object class definition. */
int mrp_lua_create_object_class(lua_State *L, mrp_lua_classdef_t *def)
{
    static bool chkconfig = true;

    mrp_debug("registering Lua object class '%s'", def->class_name);

    if (def->constructor == NULL) {
        mrp_log_error("Classes with NULL constructor not allowed.");
        mrp_log_error("Please define a constructor for class %s (type %s).",
                      def->class_name, def->type_name);
        errno = EINVAL;
        return -1;
    }

    if (chkconfig) {
        check_config();
        chkconfig = false;
    }

    /* make a metatatable for userdata, ie for 'c' part of object instances*/
    luaL_newmetatable(L, def->userdata_id);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */
    lua_pushcfunction(L, userdata_destructor);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, override_tostring);
    lua_setfield(L, -2, "__tostring");

    lua_pop(L, 1);

    /* define pre-declared members */
    {
        mrp_lua_class_member_t  *members  = def->members;
        int                      nmember  = def->nmember;
        char                   **natives  = def->natives;
        int                      nnative  = def->nnative;
        mrp_lua_class_notify_t   notify   = def->notify;
        int                      flags    = def->flags;

        def->members = NULL;
        def->nmember = 0;
        def->natives = NULL;
        def->nnative = 0;
        def->notify  = NULL;
        def->flags   = 0;
        def->brmeta  = LUA_NOREF;

        if (mrp_lua_declare_members(def, flags, members, nmember,
                                    natives, nnative, notify) != 0) {
            luaL_error(L, "failed to create object class '%s'",
                       def->class_name);
        }
    }

    mrp_list_init(&def->objects);

    /* make the class table */
    luaL_openlib(L, def->constructor, def->methods, 0);

    /* make a metatable for class, ie. for LUA part of object instances */
    luaL_newmetatable(L, def->class_id);

    if (mrp_reallocz(classdefs, nclassdef, nclassdef + 1) != NULL) {
        def->type_id = MRP_LUA_OBJECT + nclassdef;
        classdefs[nclassdef++] = def;
    }
    else {
        mrp_log_error("Failed to store class %s in lookup table.",
                      def->class_name);
        def->type_id = MRP_LUA_NONE;
    }

    /* XXX TODO we could/should do better identification */
    def->type_meta = lua_topointer(L, -1);

    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);        /* metatable.__index = metatable */

    luaL_openlib(L, NULL, def->overrides, 0);
    lua_setmetatable(L, -2);

    lua_pop(L, 1);

    return 0;
}


/** Traverse a dott global name and push the table it resolves to, or nil. */
void mrp_lua_get_class_table(lua_State *L, mrp_lua_classdef_t *def)
{
#if 0
    const char *p;
    char *q;
    char tag[256];

    lua_pushvalue(L, LUA_GLOBALSINDEX);

    for (p = def->constructor, q = tag; *p;  p++) {
        if ((*q++ = *p) == '.') {
            q[-1] = '\0';
            lua_getfield(L, -1, tag);
            if (lua_type(L, -1) != LUA_TTABLE) {
                lua_pop(L, 2);
                lua_pushnil(L);
                return;
            }
            lua_remove(L, -2);
            q = tag;
        }
    } /* for */

    *q = '\0';

    lua_getfield(L, -1, tag);
    lua_remove(L, -2);
#else
    const char *p;
    char *q;
    char tag[256];
    int  idx;

    for (p = def->constructor, q = tag, idx = 0; *p;  p++) {
        if ((*q++ = *p) == '.') {
            q[-1] = '\0';
            if (idx++ == 0) {
                lua_pushnil(L);
                mrp_lua_getglobal(L, tag);
            }
            else
                lua_getfield(L, -1, tag);
            if (lua_type(L, -1) != LUA_TTABLE) {
                lua_pop(L, 2);
                lua_pushnil(L);
                return;
            }
            lua_remove(L, -2);
            q = tag;
        }
    } /* for */

    *q = '\0';

    if (idx++ == 0) {
        lua_pushnil(L);
        mrp_lua_getglobal(L, tag);
    }
    else
        lua_getfield(L, -1, tag);

    lua_remove(L, -2);
#endif
}


static void invalid_destructor(void *data)
{
    MRP_UNUSED(data);
    mrp_log_error("<invalid-destructor> called");
}


static mrp_lua_classdef_t *class_by_type(int type_id)
{
    int idx = type_id - MRP_LUA_OBJECT;

    if (0 <= idx && idx < nclassdef)
        return classdefs[idx];
    else
        return invalid_class;
}


static mrp_lua_classdef_t *class_by_type_name(const char *type_name)
{
    mrp_lua_classdef_t *def;
    int                 i;

    for (i = 0; i < nclassdef; i++) {
        def = classdefs[i];

        if (def->type_name[0] != type_name[0])
            continue;

        if (!strcmp(def->type_name + 1, type_name + 1))
            return def;
    }

    return invalid_class;
}


static mrp_lua_classdef_t *class_by_class_name(const char *class_name)
{
    mrp_lua_classdef_t *def;
    int                 i;

    for (i = 0; i < nclassdef; i++) {
        def = classdefs[i];

        if (def->class_name[0] != class_name[0])
            continue;

        if (!strcmp(def->class_name + 1, class_name + 1))
            return def;
    }

    return invalid_class;
}


static mrp_lua_classdef_t *class_by_class_id(const char *class_id)
{
    mrp_lua_classdef_t *def;
    int                 i;

    for (i = 0; i < nclassdef; i++) {
        def = classdefs[i];

        if (def->class_id[0] != class_id[0])
            continue;

        if (!strcmp(def->class_id + 1, class_id + 1))
            return def;
    }

    return invalid_class;
}


static mrp_lua_classdef_t *class_by_userdata_id(const char *userdata_id)
{
    mrp_lua_classdef_t *def;
    int                 i;

    for (i = 0; i < nclassdef; i++) {
        def = classdefs[i];

        if (def->userdata_id[0] != userdata_id[0])
            continue;

        if (!strcmp(def->userdata_id + 1, userdata_id + 1))
            return def;
    }

    return invalid_class;
}

/** Get the type_id for the given class name. */
mrp_lua_type_t mrp_lua_class_name_type(const char *class_name)
{
    return class_by_class_name(class_name)->type_id;
}

/** Get the type_id for the given class id. */
mrp_lua_type_t mrp_lua_class_id_type(const char *class_id)
{
    return class_by_class_id(class_id)->type_id;
}

/** Get the type_id for the given class type name. */
mrp_lua_type_t mrp_lua_class_type(const char *type_name)
{
    return class_by_type_name(type_name)->type_id;
}

/** Dump the given oject instance for debugging. */
static const char *__instance(userdata_t **uptr, const char *fmt)
{
    static char buf[16][256];
    static int  idx = 0;

    userdata_t *u = uptr ? *uptr : NULL;
    char       *p = buf[idx++];
    char       *r = p;
    const char *s;
    int         l, n;

    /*
     * Notes: The currently implemeted format specifiers are:
     *     'D': dynamic flag, avaluates to 'S' or 'D'
     *     't': object type name
     *     'i': object instancem indirect userdata pointer
     *     'u': object userdata_t
     *     'd': object user-visible data
     *     'S': USERDATA_SIZE
     *     'R': reference count
     */

    if (!fmt || !*fmt || (fmt[0] == '*' && fmt[1] == '\0'))
        fmt = "<%D:%t>%i:(%u+%S)>";

    l = (int)sizeof(buf[0]);
    s = fmt;
    while (*s && l > 0) {
        if (*s != '%') {
            *p++ = *s++;
            l--;
            continue;
        }

        if (!u) {
            *p++ = '?';
            s++;
            l--;
            continue;
        }

        s++;

#define P(fmt, arg) n = snprintf(p, l, fmt, arg);
        switch (*s) {
        case 'D': P("%s", u->def->flags & MRP_LUA_CLASS_DYNAMIC?"D":"S"); break;
        case 't': P("%s", u->def->type_name);                             break;
        case 'i': P("%p", uptr);                                          break;
        case 'u': P("%p", u);                                             break;
        case 'd': P("%p", USER_TO_DATA(u));                               break;
        case 'S': P("%d", (int)USERDATA_SIZE);                            break;
        case 'R': P("%d", (int)u->refcnt);                                break;
        default : P("%s", "?");                                           break;
        }
#undef P

        p += n;
        l -= n;

        if (*s)
            s++;
    }

    if (l <= 0)
        buf[idx-1][sizeof(buf[0])-1] = '\0';

    if (idx >= (int)MRP_ARRAY_SIZE(buf))
        idx = 0;

    return r;
}

/** Dump the given object for debugging. */
static const char *__object(userdata_t *u, const char *fmt)
{
    static char buf[16][256];
    static int  idx = 0;

    char       *p = buf[idx++];
    char       *r = p;
    const char *s;
    int         l, n;

    /*
     * Notes: The currently implemeted format specifiers are:
     *     'D': dynamic flag, avaluates to 'S' or 'D'
     *     't': object type name
     *     'u': object userdata_t
     *     'd': object user-visible data
     *     'S': USERDATA_SIZE
     *     'R': reference count
     */

    if (!fmt || !*fmt || (fmt[0] == '*' && fmt[1] == '\0'))
        fmt = "<%D:%t>%i:(%u+%S)>";

    l = (int)sizeof(buf[0]);
    s = fmt;
    while (*s && l > 0) {
        if (*s != '%') {
            *p++ = *s++;
            l--;
            continue;
        }

        if (!u) {
            *p++ = '?';
            s++;
            l--;
            continue;
        }

        s++;

#define P(fmt, arg) n = snprintf(p, l, fmt, arg);
        switch (*s) {
        case 'D': P("%s", u->def->flags & MRP_LUA_CLASS_DYNAMIC?"D":"S"); break;
        case 't': P("%s", u->def->type_name);                             break;
        case 'i': P("%s", "?");                                           break;
        case 'u': P("%p", u);                                             break;
        case 'd': P("%p", USER_TO_DATA(u));                               break;
        case 'S': P("%d", (int)USERDATA_SIZE);                            break;
        case 'R': P("%d", (int)u->refcnt);                                break;
        default : P("%s", "?");                                           break;
        }
#undef P

        p += n;
        l -= n;

        if (*s)
            s++;
    }

    if (l <= 0)
        buf[idx-1][sizeof(buf[0])-1] = '\0';

    if (idx >= (int)MRP_ARRAY_SIZE(buf))
        idx = 0;

    return r;
}


/** Create a new object, optionally assign it to a class table name or index. */
void *mrp_lua_create_object(lua_State *L, mrp_lua_classdef_t *def,
                            const char *name, int idx)
{
    int class = 0;
    size_t size;
    userdata_t **userdatap, *userdata;
    int dynamic;

    MRP_UNUSED(class_by_userdata_id);

    mrp_lua_checkstack(L, -1);

    if (name || idx) {
        if (name && !valid_id(name))
            return NULL;

        mrp_lua_get_class_table(L, def);
        luaL_checktype(L, -1, LUA_TTABLE);
        class = lua_gettop(L);
    }

    lua_createtable(L, 1, 1);

    luaL_openlib(L, NULL, def->methods, 0);

    luaL_getmetatable(L, def->class_id);
    lua_setmetatable(L, -2);

    lua_pushliteral(L, "userdata");

    size = USERDATA_SIZE + def->userdata_size;
    userdata = (userdata_t *)mrp_allocz(size);

    if (userdata == NULL) {
        mrp_log_error("Failed to allocate object of type %s <%s>.",
                      def->class_name, def->type_name);
        return NULL;
    }

    userdatap = (userdata_t **)lua_newuserdata(L, sizeof(userdata));
    *userdatap = userdata;
    mrp_refcnt_init(&userdata->refcnt);

    if (cfg.track)
        mrp_list_init(&userdata->hook[0]);

    userdata->refs.priv = LUA_NOREF;
    userdata->refs.ext  = LUA_NOREF;

    luaL_getmetatable(L, def->userdata_id);
    lua_setmetatable(L, -2);

    lua_rawset(L, -3);              /* userdata["userdata"]=lib<def->methods> */

    userdata_setself(userdata);
    userdata->def    = def;

    if (!(dynamic = def->flags & MRP_LUA_CLASS_DYNAMIC)) {
        lua_pushvalue(L, -1);       /* userdata->refs.self = lib<def->methods> */
        userdata->refs.self = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
        userdata->refs.self = LUA_NOREF;

    if (name) {
        lua_pushstring(L, name);
        lua_pushvalue(L, -2);
        lua_rawset(L, class);
    }

    if (idx) {
        lua_pushvalue(L, -1);
        lua_rawseti(L, class, idx);
    }

    if (class)
        lua_remove(L, class);

    object_create_reftbl(userdata, L);
    if (def->flags & MRP_LUA_CLASS_EXTENSIBLE)
        object_create_exttbl(userdata, L);

    if (object_setup_bridges(userdata, L) < 0) {
        luaL_error(L, "Failed to set up bridged methods.");
        return NULL;                     /* not reached */
    }

    if (cfg.track)
        mrp_list_append(&def->objects, &userdata->hook[0]);

    def->nactive++;
    def->ncreated++;

    mrp_debug("created %s", __instance(userdatap, "*"));

    return USER_TO_DATA(userdata);
}


/** Set the name of the object @-1 to the given name in the class table. */
void mrp_lua_set_object_name(lua_State *L, mrp_lua_classdef_t *def,
                             const char *name)
{
    if (valid_id(name)) {
        mrp_lua_get_class_table(L, def);
        luaL_checktype(L, -1, LUA_TTABLE);

        lua_pushstring(L, name);
        lua_pushvalue(L, -3);

        lua_rawset(L, -3);
        lua_pop(L, 1);
    }
}

/** Assign the object @-1 to the given index in the class table. */
void mrp_lua_set_object_index(lua_State *L, mrp_lua_classdef_t *def, int idx)
{
    mrp_lua_get_class_table(L, def);
    luaL_checktype(L, -1, LUA_TTABLE);

    lua_pushvalue(L, -2);

    lua_rawseti(L, -2, idx);

    lua_pop(L, 1);
}

/** Trigger (potential) destruction of the given object. */
void mrp_lua_destroy_object(lua_State *L, const char *name, int idx, void *data)
{
    userdata_t *userdata = userdata_get(data, CHECK);
    mrp_lua_classdef_t *def;

    if (userdata) {
        if (userdata->dead)
            return;

        userdata->dead = true;
        def = userdata->def;

        if (!(def->flags & MRP_LUA_CLASS_DYNAMIC)) {
            mrp_debug("destroying %s (name: '%s', idx: %d)",
                      __object(userdata, "*"), name ? name : "", idx);

            def->nactive--;
            def->ndead++;

            object_delete_reftbl(userdata, L);
            object_delete_exttbl(userdata, L);

            if (userdata->refs.self != LUA_NOREF) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->refs.self);
                lua_pushstring(L, "userdata");
                lua_pushnil(L);
                lua_rawset(L, -3);
                lua_pop(L, -1);

                luaL_unref(L, LUA_REGISTRYINDEX, userdata->refs.self);
                userdata->refs.self = LUA_NOREF;
            }
        }
        else {
            mrp_log_error("ERROR: %s should be called for static object",
                          __FUNCTION__);
            mrp_log_error("ERROR: but was called for %s",
                          __object(userdata, "*"));
        }

        if (name || idx) {
            mrp_lua_get_class_table(L, def);
            luaL_checktype(L, -1, LUA_TTABLE);

            if (name) {
                lua_pushstring(L, name);
                lua_pushnil(L);
                lua_rawset(L, -3);
            }

            if (idx) {
                lua_pushnil(L);
                lua_rawseti(L, -2, idx);
            }

            lua_pop(L, 1);
        }
    }
}


/** Find the object corresponding to the given name in the class table. */
int mrp_lua_find_object(lua_State *L, mrp_lua_classdef_t *def, const char *name)
{
    if (!name)
        lua_pushnil(L);
    else {
        mrp_lua_get_class_table(L, def);
        luaL_checktype(L, -1, LUA_TTABLE);

        lua_pushstring(L, name);
        lua_rawget(L, -2);

        lua_remove(L, -2);
    }

    return 1;
}

/** Check if the object @idx is ours and optionally of the given type. */
void *mrp_lua_check_object(lua_State *L, mrp_lua_classdef_t *def, int idx)
{
    userdata_t *userdata, **userdatap;
    char errmsg[256];

    luaL_checktype(L, idx, LUA_TTABLE);

    lua_pushvalue(L, idx);
    lua_pushliteral(L, "userdata");
    lua_rawget(L, -2);

    if (!def) {
        userdatap = (userdata_t **)lua_touserdata(L, -1);

        if (!userdatap) {
            luaL_argerror(L, idx, "couldn't find expected userdata");
            userdata = NULL;
        }
        else
            userdata = *userdatap;
    }
    else {
        userdatap = (userdata_t **)luaL_checkudata(L, -1, def->userdata_id);

        if (!userdatap || def != (userdata = *userdatap)->def) {
            snprintf(errmsg, sizeof(errmsg), "'%s' expected", def->class_name);
            luaL_argerror(L, idx, errmsg);
            userdata = NULL;
        }
        else
            userdata = *userdatap;
    }

    if (userdata_getself(userdata) != userdata) {
        luaL_error(L, "invalid userdata");
        userdata = NULL;
    }

    lua_pop(L, 2);

    return userdata ? USER_TO_DATA(userdata) : NULL;
}

/** Check if the object @idx is of the given virtual type. */
int mrp_lua_object_of_type(lua_State *L, int idx, mrp_lua_type_t type)
{
    mrp_lua_type_t      ltype = (mrp_lua_type_t)lua_type(L, idx);
    mrp_lua_classdef_t *def;
    int                 match;

    switch (type) {
    case MRP_LUA_NULL:
    case MRP_LUA_BOOLEAN:
    case MRP_LUA_STRING:
    case MRP_LUA_DOUBLE:
    case MRP_LUA_FUNC:
        return (type == ltype);

    case MRP_LUA_INTEGER:
        return ((int)lua_tointeger(L, idx) == (double)lua_tonumber(L, idx));

    case MRP_LUA_LFUNC:
        return (ltype == LUA_TFUNCTION && !lua_iscfunction(L, idx));
    case MRP_LUA_CFUNC:
        return (ltype == LUA_TFUNCTION &&  lua_iscfunction(L, idx));
    case MRP_LUA_BFUNC:
        /* XXX TODO */ mrp_log_error("Can't handle funcbridge yet.");
        return false;

    case MRP_LUA_BOOLEAN_ARRAY:
    case MRP_LUA_STRING_ARRAY:
    case MRP_LUA_INTEGER_ARRAY:
    case MRP_LUA_DOUBLE_ARRAY:
        return (ltype == LUA_TTABLE); /* XXX could do be better */

    case MRP_LUA_NONE:
        return false;
    case MRP_LUA_ANY:
        return true;

    case MRP_LUA_OBJECT:
        return (ltype == LUA_TTABLE); /* XXX could do much be better */

    default:
        if (type > MRP_LUA_MAX)
            return false;

        if ((def = class_by_type(type)) == invalid_class)
            return false;

        if (lua_getmetatable(L, idx)) {
             /* XXX TODO we could/should do better identification */
            match = (lua_topointer(L, idx) == def->type_meta);
            lua_pop(L, 1);
        }
        else
            match = false;

        return match;
    }

    return false;
}

/** Check if the given data is of the given virtual type. */
int mrp_lua_pointer_of_type(void *data, mrp_lua_type_t type)
{
    userdata_t *u;

    if (type < MRP_LUA_OBJECT) {
        mrp_log_error("Can't do pointer-based type-equality for "
                      "non-object types.");
        return 0;
    }

    /*
     * We consider NULL to be a valid instance. Might need to be changed.
     */

    if ((u = userdata_get(data, CHECK)) != NULL)
        return type == u->def->type_id;
    else
        return true;
}

/** Obtain the user-visible data for the object @idx. */
void *mrp_lua_to_object(lua_State *L, mrp_lua_classdef_t *def, int idx)
{
    userdata_t *userdata, **userdatap;
    int top = lua_gettop(L);

    idx = (idx < 0) ? lua_gettop(L) + idx + 1 : idx;

    if (!lua_istable(L, idx))
        return NULL;

    lua_pushliteral(L, "userdata");
    lua_rawget(L, idx);

    userdatap = (userdata_t **)lua_touserdata(L, -1);

    if (!userdatap || !lua_getmetatable(L, -1)) {
        lua_settop(L, top);
        return NULL;
    }

    userdata = *userdatap;

    lua_getfield(L, LUA_REGISTRYINDEX, def->userdata_id);

    if (!lua_rawequal(L, -1, -2) || userdata != userdata_getself(userdata))
        userdata = NULL;

    lua_settop(L, top);

    return userdata ? USER_TO_DATA(userdata) : NULL;
}


/** Push the given data on the stack. */
int mrp_lua_push_object(lua_State *L, void *data)
{
    userdata_t *userdata = userdata_get(data, CHECK);
    userdata_t **userdatap;
    mrp_lua_classdef_t *def = userdata ? userdata->def : NULL;

    /*
     * Notes:
     *
     *    This is essentially mrp_lua_create_object with a few differences:
     *      1) No need for name or idx handling.
     *      2) No need for adding global reference, already done during
     *         initial object creation if necessary.
     *      3) Instead of creating a Lua userdata pointer and a new userdata,
     *         we only create a Lua userdata pointer, make it point to the
     *         existing userdata and increate the userdata reference count.
     *
     *   userdata_destructor has been similarly modified to decrese the
     *   reference count of userdata and destroy the object only when the
     *   last reference is dropped.
     */

    mrp_lua_checkstack(L, -1);

    if (!userdata || !def || userdata->dead) {
        lua_pushnil(L);
        return 1;
    }

    if (!(def->flags & MRP_LUA_CLASS_DYNAMIC)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->refs.self);

        mrp_debug("pushed %s", __object(userdata, "*"));
    }
    else {
        lua_createtable(L, 1, 1);

        luaL_openlib(L, NULL, def->methods, 0);

        luaL_getmetatable(L, def->class_id);
        lua_setmetatable(L, -2);

        lua_pushliteral(L, "userdata");

        userdatap = (userdata_t **)lua_newuserdata(L, sizeof(userdata));
        *userdatap = userdata;
        mrp_ref_obj(userdata, refcnt);

        luaL_getmetatable(L, def->userdata_id);
        lua_setmetatable(L, -2);

        lua_rawset(L, -3);

        mrp_debug("pushed %s", __instance(userdatap, "*"));
    }

    return 1;
}

/** Obtain the class definition for the given object. */
mrp_lua_classdef_t *mrp_lua_get_object_classdef(void *data)
{
    userdata_t *userdata = userdata_get(data, CHECK);
    mrp_lua_classdef_t *def;

    if (!userdata || userdata->dead)
        def = NULL;
    else
        def = userdata->def;

    return def;
}


static bool valid_id(const char *id)
{
    const char *p;
    char c;

    if (!(p = id) || !isalpha(*p))
        return false;

    while ((c = *p++)) {
        if (!isalnum(c) && (c != '_'))
            return false;
    }

    return true;
}

static int userdata_destructor(lua_State *L)
{
    userdata_t *userdata, **userdatap;
    mrp_lua_classdef_t *def;

    if (!(userdatap = lua_touserdata(L, -1)) || !lua_getmetatable(L, -1))
        luaL_error(L, "attempt to destroy unknown type of userdata");
    else {
        userdata = *userdatap;
        def = userdata->def;

        if (mrp_unref_obj(userdata, refcnt)) {
            mrp_debug("freeing %s", __instance(userdatap, "*"));

            if (cfg.track)
                mrp_list_delete(&userdata->hook[0]);

            lua_getfield(L, LUA_REGISTRYINDEX, def->userdata_id);
            if (!lua_rawequal(L, -1, -2))
                luaL_typerror(L, -2, def->userdata_id);
            else
                def->destructor(USER_TO_DATA(userdata));

            if (def->flags & MRP_LUA_CLASS_DYNAMIC) {
                def->nactive--;

                object_delete_reftbl(userdata, L);
                object_delete_exttbl(userdata, L);
            }
            else {
                def->ndead--;
            }

            def->ndestroyed++;

            *userdatap = NULL;
            mrp_free(userdata);
        }
        else {
            mrp_debug("unreffed %s", __instance(userdatap, "*"));

            if (!(def->flags & MRP_LUA_CLASS_DYNAMIC))
                mrp_log_error("Hmm, more refs for a static object ?");

            *userdatap = NULL;
        }
    }

    return 0;
}


static int default_setter(void *data, lua_State *L, int member,
                          mrp_lua_value_t *v)
{
    userdata_t             *u = DATA_TO_USER(data);
    mrp_lua_class_member_t *m;
    mrp_lua_value_t        *vptr;
    void                  **itemsp;
    union {
        void     *ptr;
        size_t   *size;
        uint32_t *u32;
    } nitemp;
    size_t                   nitem;

    mrp_lua_checkstack(L, -1);

    m    = u->def->members + member;
    vptr = data + m->offs;

    if (L == NULL) {
        switch (m->type) {
        case MRP_LUA_STRING:
            vptr->str = NULL;
            goto ok;
        case MRP_LUA_FUNC:
        case MRP_LUA_LFUNC:
        case MRP_LUA_CFUNC:
            vptr->lfn = LUA_NOREF;
            goto ok;
        case MRP_LUA_BFUNC:
            vptr->bfn = NULL;
            goto ok;
        case MRP_LUA_ANY:
            vptr->any = LUA_NOREF;
            goto ok;
        case MRP_LUA_STRING_ARRAY:
        case MRP_LUA_BOOLEAN_ARRAY:
        case MRP_LUA_INTEGER_ARRAY:
        case MRP_LUA_DOUBLE_ARRAY:
            itemsp     = data + m->offs;
            nitemp.ptr = data + m->size;
            *itemsp    = NULL;
            if (m->sizew == 8)
                *nitemp.size = 0;
            else
                *nitemp.u32  = 0;
            goto ok;
        case MRP_LUA_OBJECT:
            if (m->type_id == MRP_LUA_NONE)
                m->type_id = class_by_type_name(m->type_name)->type_id;
            *((void **)(data + m->offs)) = NULL;
            *((int   *)(data + m->size)) = LUA_NOREF;
        default:
            goto error;
        }
    }

    switch (m->type) {
    case MRP_LUA_STRING:
        mrp_free((void *)vptr->str);
        vptr->str = v->str ? mrp_strdup(v->str) : NULL;

        if (vptr->str == NULL && v->str != NULL)
            goto error;
        else
            goto ok;

    case MRP_LUA_BOOLEAN:
        vptr->bln = v->bln;
        goto ok;

    case MRP_LUA_INTEGER:
        vptr->s32 = v->s32;
        goto ok;

    case MRP_LUA_DOUBLE:
        vptr->dbl = v->dbl;
        goto ok;

    case MRP_LUA_FUNC:
        mrp_lua_object_unref_value(data, L, vptr->lfn);
        vptr->lfn = v->lfn;
        goto ok;

    case MRP_LUA_LFUNC:
        mrp_lua_object_unref_value(data, L, vptr->lfn);
        vptr->lfn = v->lfn;
        goto ok;

    case MRP_LUA_CFUNC:
        mrp_lua_object_unref_value(data, L, vptr->lfn);
        vptr->lfn = v->lfn;
        goto ok;

    case MRP_LUA_BFUNC:
        goto error;

    case MRP_LUA_ANY:
        mrp_lua_object_unref_value(data, L, vptr->any);
        vptr->any = v->any;
        goto ok;

    case MRP_LUA_STRING_ARRAY:
    case MRP_LUA_BOOLEAN_ARRAY:
    case MRP_LUA_INTEGER_ARRAY:
    case MRP_LUA_DOUBLE_ARRAY:
        itemsp     = data + m->offs;
        nitemp.ptr = data + m->size;
        if (m->sizew == 8)
            nitem = *nitemp.size;
        else
            nitem = *nitemp.u32;
        mrp_lua_object_free_array(itemsp, &nitem, m->type);
        *itemsp = *v->array.items;
        if (m->sizew == 8)
            *nitemp.size = *v->array.nitem64;
        else
            *nitemp.u32 = (uint32_t)*v->array.nitem64;
        goto ok;

    case MRP_LUA_OBJECT:
        mrp_lua_object_unref_value(data, L, *((int *)(data + m->size)));
        *((void **)(data + m->offs)) = v->obj.ptr;
        *((int   *)(data + m->size)) = v->obj.ref;
        goto ok;

    default:
        goto error;
    }

 ok:
    return 1;

 error:
    return -1;
}


static int default_getter(void *data, lua_State *L, int member,
                          mrp_lua_value_t *v)
{
    userdata_t             *u = DATA_TO_USER(data);
    mrp_lua_class_member_t *m;
    mrp_lua_value_t        *vptr;

    MRP_UNUSED(L);

    mrp_lua_checkstack(L, -1);

    m    = u->def->members + member;
    vptr = data + m->offs;

    switch (m->type) {
    case MRP_LUA_STRING:
        v->str = vptr->str;
        goto ok;

    case MRP_LUA_BOOLEAN:
        v->bln = vptr->bln;
        goto ok;

    case MRP_LUA_INTEGER:
        v->s32 = vptr->s32;
        goto ok;

    case MRP_LUA_DOUBLE:
        v->dbl = vptr->dbl;
        goto ok;

    case MRP_LUA_FUNC:
        v->lfn = vptr->lfn;
        goto ok;

    case MRP_LUA_LFUNC:
        v->lfn = vptr->lfn;
        goto ok;

    case MRP_LUA_CFUNC:
        v->lfn = vptr->lfn;
        goto ok;

    case MRP_LUA_BFUNC:
        goto error;

    case MRP_LUA_ANY:
        v->any = vptr->any;
        goto ok;

    case MRP_LUA_STRING_ARRAY:
    case MRP_LUA_BOOLEAN_ARRAY:
    case MRP_LUA_INTEGER_ARRAY:
    case MRP_LUA_DOUBLE_ARRAY:
        v->array = vptr->array;
        goto ok;

    case MRP_LUA_OBJECT:
        v->obj.ptr = *((void **)(data + m->offs));
        v->obj.ref = *((int   *)(data + m->size));
        goto ok;

    default:
        goto error;
    }

 ok:
    return 1;

 error:
    return -1;
}


static int patch_overrides(mrp_lua_classdef_t *def)
{
    luaL_reg set = { NULL, NULL }, get = { NULL, NULL }, *r, *overrides;
    int      i, n, extra, tostring;

    tostring = 0;
    for (n = 0, r = def->overrides; r->name != NULL; r++, n++) {
        if (!strcmp(r->name, "__newindex")) {
            if (set.name != NULL) {
                mrp_log_error("Class with multiple SETFIELD overrides.");
                exit(1);
            }

            if (set.func == override_setfield) {
                mrp_log_error("SETFIELD already overridden to setfield!");
                exit(1);
            }

            set = *r;
            r->func = override_setfield;
            continue;
        }

        if (!strcmp(r->name, "__index")) {
            if (get.name != NULL) {
                mrp_log_info("Class with multiple GETFIELD overrides.");
                exit(1);
            }

            if (get.func == override_getfield) {
                mrp_log_error("GETFIELD already overridden to getfield!");
                exit(1);
            }

            get = *r;
            r->func = override_getfield;
            continue;
        }

        if (!strcmp(r->name, "__tostring")) {
            tostring = 1;
            continue;
        }
    }

    if (set.func && get.func && tostring) {
        def->setfield = set.func;
        def->getfield = get.func;

        return 0;
    }

    extra = (set.func ? 0 : 1) + (get.func ? 0 : 1) + (tostring ? 0 : 1);

    /* XXX TODO: currently this is leaked if/when a classdef is destroyed */
    if ((overrides = mrp_allocz_array(typeof(*overrides), n+1 + extra)) == NULL)
        return -1;

    for (i = 0, r = def->overrides; r->name != NULL; i++, r++) {
        overrides[i].name = r->name;
        overrides[i].func = r->func;
    }

    if (set.func == NULL) {
        mrp_debug("overriding __newindex for class %s", def->class_name);
        overrides[i].name = "__newindex";
        overrides[i].func = override_setfield;
        i++;
    }
    else
        def->setfield = set.func;

    if (get.func == NULL) {
        mrp_debug("overriding __index for class %s", def->class_name);
        overrides[i].name = "__index";
        overrides[i].func = override_getfield;
        i++;
    }
    else
        def->getfield = get.func;

    if (!tostring) {
        mrp_debug("overriding __tostring for class %s", def->class_name);
        overrides[i].name = "__tostring";
        overrides[i].func = override_tostring;
        i++;
    }

    def->overrides = overrides;

    return 0;
}


/** Declare automatically handled class members for the given class. */
int mrp_lua_declare_members(mrp_lua_classdef_t *def, mrp_lua_class_flag_t flags,
                            mrp_lua_class_member_t *members, int nmember,
                            char **natives, int nnative,
                            mrp_lua_class_notify_t notify)
{
#define F(flag)          MRP_LUA_CLASS_##flag
#define INHERITED_FLAGS (F(READONLY)|F(RAWGETTER)|F(RAWSETTER))

    mrp_lua_class_member_t *m;
    int                     i;

    def->flags = flags;

    if (members == NULL || nmember <= 0) {
        if (def->flags & MRP_LUA_CLASS_EXTENSIBLE)
            goto update_overrides;
        else
            return 0;
    }

    def->members = mrp_allocz_array(typeof(*def->members), nmember);

    if (def->members == NULL)
        return -1;

    for (i = 0, m = def->members; i < nmember; i++, m++) {
        if (members[i].flags & MRP_LUA_CLASS_NOTIFY) {
            if (notify == NULL) {
                mrp_log_error("member '%s' needs a non-NULL notifier",
                              members[i].name);
                goto fail;
            }
        }

        if (MRP_LUA_BOOLEAN_ARRAY <= members[i].type &&
            members[i].type <= MRP_LUA_DOUBLE_ARRAY) {
            if (members[i].sizew != 8 && members[i].sizew != 4) {
                mrp_log_error("array member '%s': size must be 32- or 64-bit",
                              members[i].name);
                goto fail;
            }
        }

        if ((m->name = mrp_strdup(members[i].name)) == NULL)
            goto fail;

        *m = members[i];

        if (m->setter == NULL)
            m->setter = default_setter;
        if (m->getter == NULL)
            m->getter = default_getter;

        m->flags |= (flags & INHERITED_FLAGS); /* inherit flags we can */

        /* clear flags the default setter and getter don't do */
        if (m->setter == default_setter)
            m->flags &= ~MRP_LUA_CLASS_RAWSETTER;

        if (m->getter == default_getter)
            m->flags &= ~MRP_LUA_CLASS_RAWGETTER;

        def->nmember++;
    }

    def->flags  = flags;
    def->notify = notify;

    if (natives == NULL || nnative == 0)
        goto update_overrides;

    def->natives = mrp_allocz_array(typeof(*def->natives), nnative);

    if (def->natives == NULL)
        goto fail;

    for (i = 0; i < nnative; i++) {
        if ((def->natives[i] = mrp_strdup(natives[i])) == NULL)
            goto fail;

        def->nnative++;
    }

 update_overrides:
    if (!(def->flags & MRP_LUA_CLASS_NOOVERRIDE))
        patch_overrides(def);

    return 0;

 fail:
    for (i = 0, m = def->members; i < def->nmember; i++, m++)
        mrp_free(m->name);
    mrp_free(def->members);

    def->members = NULL;
    def->nmember = 0;

    for (i = 0; i < def->nnative; i++)
        mrp_free(def->natives[i]);
    mrp_free(def->natives);

    def->natives = NULL;
    def->nnative = 0;

    return -1;
}


static int object_setup_bridges(userdata_t *u, lua_State *L)
{
    mrp_lua_classdef_t     *def = u->def;
    mrp_lua_class_bridge_t *b;
    int                     i, class_usestack;

    class_usestack = (def->flags & MRP_LUA_CLASS_USESTACK) ? true : false;
    for (i = 0, b = def->bridges; i < def->nbridge; i++, b++) {
        b->fb = mrp_funcbridge_create_cfunc(L, b->name, b->signature, b->fc,
                                            USER_TO_DATA(u));

        if (b->fb == NULL)
            return -1;

        b->fb->autobridge = true;
        b->fb->usestack   = (b->flags & MRP_LUA_CLASS_USESTACK) ? true : false;
        b->fb->usestack  |= class_usestack;
    }

    return 0;
}


static int class_member(userdata_t *u, lua_State *L, int index)
{
    mrp_lua_class_member_t *members = u->def->members;
    int                     nmember = u->def->nmember;
    mrp_lua_class_member_t *m;
    int                     i;
    const char              *name;

    if (lua_type(L, index) != LUA_TSTRING)
        return -1;

    name = lua_tostring(L, index);

    /*
     * XXX TODO, check how to speed this up. For instance if Lua
     * strings or references to string happened to be always
     * represented by the same value as long as they are interned
     * (ie. not collected) we could simply check for numeric
     * equality of the stack item or a reference to thereof to one
     * store in the object classdef...
     *
     * Alternatively if all else fails, at least pass in the length
     * here and store it alongside all native names, to speed up
     * negtive tests.
     */

    for (i = 0, m = members; i < nmember; i++, m++)
        if (!strcmp(m->name, name))
            return i;

    return -1;
}


static int class_bridge(userdata_t *u, lua_State *L, int index)
{
    mrp_lua_class_bridge_t *bridges, *b;
    int                     nbridge;
    const char             *name;
    int                     bidx;

    if ((bridges = u->def->bridges) == NULL || (nbridge = u->def->nbridge) == 0)
        return -1;

    if (lua_type(L, index) != LUA_TSTRING)
        return -1;

    name = lua_tostring(L, index);

    /*
     * XXX TODO, ditto as for class_member()
     */

    for (bidx = 0, b = bridges; bidx < nbridge; bidx++, b++)
        if (!strcmp(b->name, name))
            return bidx;

    return -1;
}


static int seterr(lua_State *L, char *e, size_t size, const char *format, ...)
{
    va_list ap;
    char    msg[256];

    va_start(ap, format);
    vsnprintf(e ? e : msg, e ? size : sizeof(msg), format, ap);
    va_end(ap);

    if (!e && L) {
        lua_pushstring(L, msg);
        lua_error(L);
    }

    return -1;
}


static void object_create_reftbl(userdata_t *u, lua_State *L)
{
    lua_newtable(L);
    u->refs.priv = luaL_ref(L, LUA_REGISTRYINDEX);
}


static void object_delete_reftbl(userdata_t *u, lua_State *L)
{
    luaL_unref(L, LUA_REGISTRYINDEX, u->refs.priv);
    u->refs.priv = LUA_NOREF;
}


/** Refcount the object @idx for or within the given object. */
int mrp_lua_object_ref_value(void *data, lua_State *L, int idx)
{
    userdata_t *u = userdata_get(data, CHECK);
    int ref;

    if (u->refs.priv != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, u->refs.priv);
        lua_pushvalue(L, idx > 0 ? idx : idx - 1);
        ref = luaL_ref(L, -2);
        lua_pop(L, 1);
    }
    else
        ref = LUA_NOREF;

    return ref;
}

/** Release the given reference for/from within the given object. */
void mrp_lua_object_unref_value(void *data, lua_State *L, int ref)
{
    userdata_t *u = userdata_get(data, CHECK);

    if (ref != LUA_NOREF && ref != LUA_REFNIL) {
        if (u->refs.priv != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, u->refs.priv);
            luaL_unref(L, -1, ref);
            lua_pop(L, 1);
        }
    }
}

/** Obtain and push the object for the given reference on the stack. */
int mrp_lua_object_deref_value(void *data, lua_State *L, int ref, int pushnil)
{
    userdata_t *u = userdata_get(data, CHECK);

    if (ref == LUA_REFNIL) {
    nilref:
        lua_pushnil(L);
        return 1;
    }

    if (ref == LUA_NOREF) {
        if (pushnil)
            goto nilref;
        else
            return 0;
    }

    if (u->refs.priv == LUA_NOREF) {
        if (pushnil)
            goto nilref;
        else
            return 0;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->refs.priv);
    lua_rawgeti(L, -1, ref);
    lua_remove(L, -2);

    return 1;
}

/** Obtain a new reference based on the reference owned by owner. */
int mrp_lua_object_getref(void *owner, void *data, lua_State *L, int ref)
{
    userdata_t *uo = userdata_get(owner, CHECK);
    userdata_t *ud = userdata_get(data , CHECK);

    if (ref == LUA_NOREF || ref == LUA_REFNIL)
        return ref;

    if (uo->refs.priv == LUA_NOREF || ud->refs.priv == LUA_NOREF)
        return LUA_NOREF;

    lua_rawgeti(L, LUA_REGISTRYINDEX, uo->refs.priv);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->refs.priv);

    lua_rawgeti(L, -2, ref);
    ref = luaL_ref(L, -2);

    lua_pop(L, 2);

    return ref;
}


static void object_create_exttbl(userdata_t *u, lua_State *L)
{
    lua_newtable(L);
    u->refs.ext = luaL_ref(L, LUA_REGISTRYINDEX);
}


static void object_delete_exttbl(userdata_t *u, lua_State *L)
{
    int extidx;

    if (u->refs.ext == LUA_NOREF)
        return;

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->refs.ext);
    extidx = lua_gettop(L);

    /*
     * Notes:
     *     I'm not sure whether explicitly unreffing all references
     *     is necessary... In principle this should not be necessary.
     *     We should have the only reference to our exttbl and we are
     *     about to remove that making exttb garbage-collectable.
     */

    lua_pushnil(L);
    while (lua_next(L, extidx) != 0) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_pushnil(L);

        mrp_debug("freeing extended member [%s] of %s", lua_tostring(L, -2),
                  __object(u, "*"));

        lua_rawset(L, extidx);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, u->refs.ext);
    u->refs.ext = LUA_NOREF;

    lua_settop(L, extidx);
    lua_pop(L, 1);
}


static int object_setext(void *data, lua_State *L, const char *name,
                         int vidx, char *err, size_t esize)
{
    userdata_t *u = DATA_TO_USER(data);

    if (u->refs.ext == LUA_NOREF) {
        if (err)
            return seterr(L, err, esize, "trying to set user-defined field %s "
                          "for non-extensible object %s", name,
                          u->def->class_name);
        else
            return luaL_error(L, "trying to set user-defined field %s "
                              "for non-extensible object %s", name,
                              u->def->class_name);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->refs.ext);
    lua_pushvalue(L, vidx > 0 ? vidx : vidx - 1);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);

    return 1;
}


static int object_getext(void *data, lua_State *L, const char *name)
{
    userdata_t *u = DATA_TO_USER(data);

    if (u->refs.ext == LUA_NOREF) {
        lua_pushnil(L);
        return 1;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->refs.ext);
    lua_getfield(L, -1, name);
    lua_remove(L, -2);

    return 1;
}


static int object_setiext(void *data, lua_State *L, int idx, int val)
{
    userdata_t *u = DATA_TO_USER(data);

    if (u->refs.ext == LUA_NOREF) {
        return luaL_error(L, "trying to set user-defined index %d "
                          "for non-extensible object %s", idx,
                          u->def->class_name);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->refs.ext);
    lua_pushvalue(L, val > 0 ? val : val - 1);
    lua_rawseti(L, -2, idx);
    lua_pop(L, 1);

    return 1;
}


static int object_getiext(void *data, lua_State *L, int idx)
{
    userdata_t *u = DATA_TO_USER(data);

    if (u->refs.ext == LUA_NOREF) {
        lua_pushnil(L);
        return 1;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, u->refs.ext);
    lua_rawgeti(L, -1, idx);
    lua_remove(L, -2);

    return 1;
}


static inline int array_lua_type(int type)
{
    switch (type) {
    case MRP_LUA_STRING_ARRAY:  return LUA_TSTRING;
    case MRP_LUA_BOOLEAN_ARRAY: return LUA_TBOOLEAN;
    case MRP_LUA_INTEGER_ARRAY: return LUA_TNUMBER;
    case MRP_LUA_DOUBLE_ARRAY:  return LUA_TNUMBER;
    default:                    return LUA_TNONE;
    }
}


static inline int array_murphy_type(int type)
{
    switch (type) {
    case LUA_TSTRING:  return MRP_LUA_STRING_ARRAY;
    case LUA_TBOOLEAN: return MRP_LUA_BOOLEAN_ARRAY;
    case LUA_TNUMBER:  return MRP_LUA_INTEGER_ARRAY;
    default:           return MRP_LUA_NONE;
    }
}


static inline int array_item_size(int type)
{
    switch (type) {
    case MRP_LUA_STRING_ARRAY:  return sizeof(char *);
    case MRP_LUA_BOOLEAN_ARRAY: return sizeof(bool);
    case MRP_LUA_INTEGER_ARRAY: return sizeof(int32_t);
    case MRP_LUA_DOUBLE_ARRAY:  return sizeof(double);
    default:                    return 0;
    }
}


static inline const char *array_type_name(int type)
{
    switch (type) {
    case MRP_LUA_STRING_ARRAY:  return "string";
    case MRP_LUA_BOOLEAN_ARRAY: return "boolean";
    case MRP_LUA_INTEGER_ARRAY: return "integer";
    case MRP_LUA_DOUBLE_ARRAY:  return "double";
    case MRP_LUA_ANY:           return "any";
    default:                    return "<invalid array type>";
    }
}


/** Collect, optionally dupping, all items from an assumed array @tidx. */
int mrp_lua_object_collect_array(lua_State *L, int tidx, void **itemsp,
                                 size_t *nitemp, int *expectedp, int dup,
                                 char *e, size_t esize)
{
    const char *name, *str;
    int         ktype, vtype, ltype, i, expected, popnil;
    size_t      max, idx, isize;
    void       *items;

    max   = *nitemp;
    tidx  = mrp_lua_absidx(L, tidx);
    items = *itemsp;
    ltype = LUA_TNONE;
    isize = 0;

    expected = *expectedp;
    popnil   = false;

    if (expected != MRP_LUA_ANY) {
        ltype = array_lua_type(expected);
        isize = array_item_size(expected);

        if (ltype == LUA_TNONE || !isize)
            goto type_error;
    }

    lua_pushnil(L);
    popnil = true;
    MRP_LUA_FOREACH_ALL(L, i, tidx, ktype, name, idx) {
        vtype = lua_type(L, -1);

        mrp_debug("collecting <%s>:<%s> element for %s array",
                  lua_typename(L, ktype), lua_typename(L, vtype),
                  array_type_name(expected));

        if (ktype != LUA_TNUMBER)
            goto not_pure;

        if (expected == MRP_LUA_ANY) {
            expected = array_murphy_type(vtype);

            if (!expected)
                goto type_error;

            if (expected == MRP_LUA_INTEGER_ARRAY)
                expected = MRP_LUA_DOUBLE_ARRAY;        /* safer for ANY */

            ltype = array_lua_type(expected);
            isize = array_item_size(expected);
        }
        else
            /* bail out for type mismatch (null is a valid string array) */
            if (vtype != ltype &&
                !(expected == MRP_LUA_STRING_ARRAY && vtype == LUA_TNIL))
                goto type_error;

        if (max != (size_t)-1 && i >= (int)max)
            goto overflow;

        if (dup && mrp_realloc(items, (i + 1) * isize) == NULL)
            goto nomem;

        switch (expected) {
        case MRP_LUA_STRING_ARRAY:
            str = (vtype != LUA_TNIL ? lua_tostring(L, -1) : NULL);
            if (dup) {
                ((char **)items)[i] = str ? mrp_strdup(str) : NULL;
                if (!((char **)items)[i] && str)
                    goto nomem;
            }
            else
                ((char **)items)[i] = (char *)str;
            break;
        case MRP_LUA_BOOLEAN_ARRAY:
            ((bool *)items)[i] = lua_toboolean(L, -1);
            break;
        case MRP_LUA_INTEGER_ARRAY:
            ((int32_t *)items)[i] = lua_tointeger(L, -1);
            break;
        case MRP_LUA_DOUBLE_ARRAY:
            ((double *)items)[i] = lua_tonumber(L, -1);
            break;
        default:
            goto type_error;
        }
    }
    lua_pop(L, 1);

    *itemsp    = items;
    *nitemp    = i;
    *expectedp = expected;

    return 0;


#define CLEANUP() do {                                                  \
        mrp_lua_object_free_array(itemsp, nitemp, expected);            \
        if (popnil) lua_pop(L, 1);                                      \
    } while (0)

 type_error:
    CLEANUP(); return seterr(L, e, esize, "array or element of wrong type");
 not_pure:
    CLEANUP(); return seterr(L, e, esize, "not a pure array");
 nomem:
    CLEANUP(); return seterr(L, e, esize, "could not allocate array");
 overflow:
    CLEANUP(); return seterr(L, e, esize, "array too large");
#undef CLEANUP
}

/** Free an array collected and duplicated by the collector above. */
void mrp_lua_object_free_array(void **itemsp, size_t *nitemp, int type)
{
    size_t   nitem = *nitemp;
    char   **saptr;
    size_t   i;

    switch (type) {
    case MRP_LUA_STRING_ARRAY:
        saptr = *itemsp;
        for (i = 0; i < nitem; i++)
            mrp_free(saptr[i]);
    case MRP_LUA_BOOLEAN_ARRAY:
    case MRP_LUA_INTEGER_ARRAY:
    case MRP_LUA_DOUBLE_ARRAY:
        mrp_free(*itemsp);
        *itemsp = NULL;
        *nitemp = 0;
        break;
    default:
        break;
    }
}

/** Push the given array of simple native C type on the stack. */
int mrp_lua_object_push_array(lua_State *L, int type, void *items, size_t nitem)
{
    int i;

    lua_createtable(L, nitem, 0);

    for (i = 0; i < (int)nitem; i++) {
        switch (type) {
        case MRP_LUA_STRING_ARRAY:
            lua_pushstring(L, ((char **)items)[i]);
            break;
        case MRP_LUA_BOOLEAN_ARRAY:
            lua_pushboolean(L, ((bool *)items)[i]);
            break;
        case MRP_LUA_INTEGER_ARRAY:
            lua_pushinteger(L, ((int32_t *)items)[i]);
            break;
        case MRP_LUA_DOUBLE_ARRAY:
            lua_pushnumber(L, ((double *)items)[i]);
            break;
        default:
            lua_pop(L, 1);
            return -1;
        }

        lua_rawseti(L, -2, i + 1);
    }

    return 1;
}

/** Perform a setfield-like member assignment on the given object. */
int mrp_lua_set_member(void *data, lua_State *L, char *err, size_t esize)
{
    userdata_t             *u = DATA_TO_USER(data);
    int                     midx = class_member(u, L, -2);
    mrp_lua_class_member_t *m;
    mrp_lua_value_t         v;
    int                     vtype, etype;
    void                   *items;
    size_t                  nitem;

    if (midx < 0)
        goto notfound;

    mrp_lua_checkstack(L, -1);

    m     = u->def->members + midx;
    vtype = lua_type(L, -1);

    mrp_debug("setting %s.%s of Lua object %p(%p)", u->def->class_name,
              m->name, u, data);

    if (!u->initializing && (m->flags & MRP_LUA_CLASS_READONLY))
        return seterr(L, err, esize, "%s.%s of Lua object is readonly",
                      u->def->class_name, m->name);

    if (u->initializing && (m->flags & MRP_LUA_CLASS_NOINIT))
        goto ok_noinit;

    if (m->flags & MRP_LUA_CLASS_RAWSETTER) {
        if (m->setter(data, L, midx, NULL) == 1)
            goto ok;
        else
            goto error;
    }

    switch (m->type) {
    case MRP_LUA_STRING:
        if (vtype != LUA_TSTRING && vtype != LUA_TNIL)
            return seterr(L, err, esize, "%s.%s expects string or nil, got %s",
                          u->def->class_name, m->name,
                          lua_typename(L, vtype), m->name);

        v.str = lua_tostring(L, -1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    case MRP_LUA_BOOLEAN:
        v.bln = lua_toboolean(L, -1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    case MRP_LUA_INTEGER:
        if (vtype != LUA_TNUMBER)
            return seterr(L, err, esize, "%s.%s expects number, got %s",
                          u->def->class_name, m->name, lua_typename(L, vtype));

        v.s32 = lua_tointeger(L, -1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    case MRP_LUA_DOUBLE:
        if (vtype != LUA_TNUMBER)
            return seterr(L, err, esize, "%s.%s expects number, got %s",
                          u->def->class_name, m->name, lua_typename(L, vtype));

        v.dbl = lua_tonumber(L, -1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    case MRP_LUA_CFUNC:
        if (vtype != LUA_TFUNCTION && vtype != LUA_TNIL)
            return seterr(L, err, esize, "%s.%s expects function, got %s",
                          u->def->class_name, m->name, lua_typename(L, vtype));
        if (!lua_iscfunction(L, -1))
            return seterr(L, err, esize, "%s.%s expects Lua C-function.",
                          u->def->class_name, m->name);
        goto setfn;

    case MRP_LUA_LFUNC:
        if (vtype != LUA_TFUNCTION && vtype != LUA_TNIL)
            return seterr(L, err, esize, "%s.%s expects function, got %s",
                          u->def->class_name, m->name, lua_typename(L, vtype));
        if (lua_iscfunction(L, -1))
            return seterr(L, err, esize, "%s.%s expects pure Lua function.",
                          u->def->class_name, m->name);
        goto setfn;

    case MRP_LUA_FUNC:
        if (vtype != LUA_TFUNCTION && vtype != LUA_TNIL)
            return seterr(L, err, esize, "%s.%s expects function, got %s",
                          u->def->class_name, m->name, lua_typename(L, vtype));

    setfn:
        v.lfn = mrp_lua_object_ref_value(data, L, -1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    case MRP_LUA_BFUNC:
        seterr(L, err, esize, "BFUNC is not implemented");
        goto error;

    case MRP_LUA_NULL:
        seterr(L, err, esize, "setting member of invalid type NULL");
        goto error;

    case MRP_LUA_NONE:
        seterr(L, err, esize, "setting member of invalid type NONE");
        goto error;

    case MRP_LUA_ANY:
        v.any = mrp_lua_object_ref_value(data, L, -1);
        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;

    case MRP_LUA_STRING_ARRAY:
    case MRP_LUA_BOOLEAN_ARRAY:
    case MRP_LUA_INTEGER_ARRAY:
    case MRP_LUA_DOUBLE_ARRAY:
        items = NULL;
        nitem = (size_t)-1;
        etype = m->type;
        if (mrp_lua_object_collect_array(L, -1, &items, &nitem, &etype, true,
                                         err, esize) < 0)
            return -1;
        else {
            v.array.items   = data + m->offs;
            v.array.nitem64 = data + m->size;

            *v.array.items = items;
            if (m->sizew == 8)
                *v.array.nitem64 = nitem;
            else
                *v.array.nitem32 = (uint32_t)nitem;
        }
        goto ok;

    case MRP_LUA_OBJECT:
        if (m->type_id == MRP_LUA_NONE)
            m->type_id = class_by_type_name(m->type_name)->type_id;

        if (m->type_id == MRP_LUA_NONE) {
            seterr(L, err, esize, "can't set member of unknown type %s",
                   m->type_name);
            goto error;
        }

        if (!mrp_lua_object_of_type(L, -1, m->type_id)) {
            seterr(L, err, esize, "object type mismatch, expecting '%s'",
                   class_by_type(m->type_id)->type_name);
            goto error;
        }

        v.obj.ref = mrp_lua_object_ref_value(data, L, -1);

        lua_pushliteral(L, "userdata");
        lua_rawget(L, -2);
        if ((v.obj.ptr = lua_touserdata(L, -1)) != NULL) {
            v.obj.ptr = *(void **)v.obj.ptr;
            v.obj.ptr = USER_TO_DATA(v.obj.ptr);
        }
        lua_pop(L, 1);

        if (m->setter(data, L, midx, &v) == 1)
            goto ok;
        else
            goto error;
        break;

    default:
        seterr(L, err, esize, "type %d not implemented");
        break;
    }

 ok:
    if ((m->flags & MRP_LUA_CLASS_NOTIFY) && u->def->notify)
        u->def->notify(data, L, midx);
 ok_noinit:
    return 1;

 notfound:
    return 0;

 error:
    return -1;
}

/** Perform a getfield-like member-lookup on the given object. */
int mrp_lua_get_member(void *data, lua_State *L, char *err, size_t esize)
{
    userdata_t             *u = DATA_TO_USER(data);
    mrp_lua_class_member_t *m;
    mrp_lua_class_bridge_t *b;
    int                     midx, bidx;
    mrp_lua_value_t         v;
    void                  **items;
    size_t                 *nitem;

    mrp_lua_checkstack(L, -1);

    if ((midx = class_member(u, L, -1)) >= 0)
        m = u->def->members + midx;
    else {
        if ((bidx = class_bridge(u, L, -1)) >= 0) {
            b = u->def->bridges + bidx;

            return mrp_funcbridge_push(L, b->fb);
        }

        goto notfound;
    }

    if (m->getter(data, L, midx, &v) != 1)
        goto error;

    if (m->flags & MRP_LUA_CLASS_RAWGETTER)
        goto ok;

    switch (m->type) {
    case MRP_LUA_STRING:
        if (v.str != NULL)
            lua_pushstring(L, v.str);
        else
            lua_pushnil(L);
        goto ok;

    case MRP_LUA_BOOLEAN:
        lua_pushboolean(L, v.bln);
        goto ok;

    case MRP_LUA_INTEGER:
        lua_pushinteger(L, v.s32);
        goto ok;

    case MRP_LUA_DOUBLE:
        lua_pushnumber(L, v.dbl);
        goto ok;

    case MRP_LUA_FUNC:
        mrp_lua_object_deref_value(data, L, v.lfn, true);
        goto ok;

    case MRP_LUA_LFUNC:
        mrp_lua_object_deref_value(data, L, v.lfn, true);
        goto ok;

    case MRP_LUA_CFUNC:
        mrp_lua_object_deref_value(data, L, v.lfn, true);
        goto ok;

    case MRP_LUA_BFUNC:
        seterr(L, err, esize, "BFUNC is not implemented");
        goto error;

    case MRP_LUA_NULL:
        lua_pushnil(L);
        goto ok;

    case MRP_LUA_NONE:
        seterr(L, err, esize, "invalid type");
        goto error;

    case MRP_LUA_ANY:
        mrp_lua_object_deref_value(data, L, v.any, true);
        goto ok;

    case MRP_LUA_STRING_ARRAY:
    case MRP_LUA_BOOLEAN_ARRAY:
    case MRP_LUA_INTEGER_ARRAY:
    case MRP_LUA_DOUBLE_ARRAY:
        items = data + m->offs;
        nitem = data + m->size;
        if (mrp_lua_object_push_array(L, m->type, *items, *nitem) > 0)
            goto ok;
        else {
            seterr(L, err, esize, "failed to push array");
            goto error;
        }

    case MRP_LUA_OBJECT:
        mrp_lua_object_deref_value(data, L, v.obj.ref, true);
        break;

    default:
        goto error;
    }

 ok:
    return 1;

 notfound:
    return 0;

 error:
    return -1;
}

/** Perform a table-based member initialization for the given object. */
int mrp_lua_init_members(void *data, lua_State *L, int idx,
                         char *err, size_t esize)
{
    userdata_t *u = userdata_get(data, CHECK);
    const char *n;
    size_t      l;
    int         top, set;

    if (err != NULL)
        *err = '\0';

    if (idx < 0)
        idx = lua_gettop(L) + idx + 1;

    if (lua_type(L, idx) != LUA_TTABLE)
        return 0;

    if (u->def->flags & MRP_LUA_CLASS_NOINIT) {
        mrp_log_warning("Explicit table-based member initializer called for");
        mrp_log_warning("object %s marked for NOINIT.", u->def->class_name);
    }

    top = lua_gettop(L);
    u->initializing = true;
    MRP_LUA_FOREACH_FIELD(L, idx, n, l) {
        mrp_debug("initializing %s.%s", u->def->class_name, n);

        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);

        switch (mrp_lua_set_member(data, L, err, esize)) {
        case -1:
            goto error;
        case 0:
            /*
             * Okay, this was not an ordinary predefined member, so
             *     - pass this to setfield if we have one and this field is
             *       declared native, or no native fields have been declared
             *     - if there is no setfield or setfield said it does not
             *       handle this field, pass this to setext if the class is
             *       extensible
             *
             * Note that, in principle, we probably should treat it an error
             * if setfield does not handle a field that has been declared
             * native but currently we don't.
             */

            set = 0;

            if (u->def->setfield && (!u->def->natives || is_native(u, n))) {
                mrp_lua_push_object(L, data);
                lua_insert(L, -3);
                set = u->def->setfield(L);
                lua_remove(L, -3);
            }

            if (set == 0 && (u->def->flags & MRP_LUA_CLASS_EXTENSIBLE))
                set = object_setext(data, L, n, -1, NULL, 0);

            if (set <= 0)
                goto error;
            break;
        case 1:
            break;
        }
    }
    u->initializing = false;
    lua_settop(L, top);
    return 1;

 error:
    u->initializing = false;
    lua_settop(L, top);
    return -1;

}


static inline int is_native(userdata_t *u, const char *name)
{
    int i;

    /*
     * XXX TODO, ditto as for class_member() and class_bridge()
     */

    for (i = 0; i < u->def->nnative; i++)
        if (u->def->natives[i][0] == name[0] &&
            !strcmp(u->def->natives[i] + 1, name + 1))
            return 1;

    return 0;
}


static int override_setfield(lua_State *L)
{
    void       *data = mrp_lua_check_object(L, NULL, 1);
    userdata_t *u    = DATA_TO_USER(data);
    char        err[128] = "";
    const char *name;
    int         status;

    if (data == NULL)
        return luaL_error(L, "failed to find class userdata");

    mrp_debug("setting field for object of type '%s'", u->def->class_name);

    switch (mrp_lua_set_member(data, L, err, sizeof(err))) {
    case 1:  /* ok */
        status = 0;
        goto out;
    case 0:  /* field not found */
        break;
    default: /* error */
        return luaL_error(L, "failed to set member (%s)", err);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        name = lua_tostring(L, 2);
        break;
    case LUA_TNUMBER:
        name = NULL;
        break;
    default:
        return luaL_argerror(L, 2, "expecting string or integer");
    }

    luaL_checkany(L, 3);

    /*
     * Okay, this was not an ordinary predefined member, so
     *     - if this is a field (named vs. indexed member)
     *         * pass this to setfield if we have one and this field is
     *           declared native, or no native fields have been declared
     *         * if there is no setfield or setfield said it does not handle
     *           this field, pass this to setext if the class is extensible
     *     - otherwise (IOW an indexed member) pass this to setixet if the
     *       class is extensible
     *
     * Note that, in principle, we probably should treat it an error if
     * setfield does not handle a field that has been declared native but
     * currently we don't.
     */

    status = 0;

    if (name != NULL) {
        if (u->def->setfield && (!u->def->natives || is_native(u, name)))
            status = u->def->setfield(L);

        if (status == 0 && u->def->flags & MRP_LUA_CLASS_EXTENSIBLE)
            status = object_setext(data, L, name, 3, NULL, 0);
    }
    else
        status = object_setiext(data, L, lua_tointeger(L, 2), 3);

 out:
    return status;
}


static int override_getfield(lua_State *L)
{
    void       *data = mrp_lua_check_object(L, NULL, 1);
    userdata_t *u    = DATA_TO_USER(data);
    char        err[128] = "";
    const char *name;
    int         status;

    mrp_debug("getting field for object of type '%s'", u->def->class_name);

    switch (mrp_lua_get_member(data, L, err, sizeof(err))) {
    case 1:  /* ok */
        status = 1;
        goto out;
    case 0:  /* field not found */
        break;
    default: /* error */
        return luaL_error(L, "failed to set member (%s)", err);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        name = lua_tostring(L, 2);
        break;
    case LUA_TNUMBER:
        name = NULL;
        break;
    default:
        return luaL_argerror(L, 2, "expecting string or integer");
    }

    /*
     * Okay, this was not an ordinary predefined member, so
     *     - if this is a field (named vs. indexed member)
     *         * pass this to setfield if we have one and this field is
     *           declared native, or no native fields have been declared
     *         * if there is no setfield or setfield said it does not handle
     *           this field, pass this to setext if the class is extensible
     *     - otherwise (IOW an indexed member) pass this to setixet if the
     *       class is extensible
     *
     * Note that, in principle, we probably should treat it an error if
     * setfield does not handle a field that has been declared native but
     * currently we don't.
     */

    status = 0;

    if (name != NULL) {
        if (u->def->getfield && (!u->def->natives || is_native(u, name)))
            status = u->def->getfield(L);

        if (status == 0 && u->def->flags & MRP_LUA_CLASS_EXTENSIBLE)
            status = object_getext(data, L, name);
    }
    else
        status = object_getiext(data, L, 2);

 out:
    return status;
}


static int override_tostring(lua_State *L)
{
    void       *data;
    userdata_t *u;
    char        buf[1024];

    data = mrp_lua_check_object(L, NULL, 1);
    u    = DATA_TO_USER(data);

    if (u != NULL) {
        if (u->def->tostring == NULL ||
            u->def->tostring(MRP_LUA_TOSTR_LUA, buf, sizeof(buf),
                             L, data) <= 0) {
            snprintf(buf, sizeof(buf), "<%s)", __object(u, "*"));
        }

        lua_pushstring(L, buf);

        return 1;
    }
    else {
        snprintf(buf, sizeof(buf),
                 "<tostring called for invalid Murphy Lua object %p>", data);
        return luaL_error(L, buf);
    }

}


static ssize_t userdata_minimal_tostr(char *buf, size_t size, userdata_t *u)
{
    return snprintf(buf, size, "<%d:%p>", u->def->type_id, u);
}


static ssize_t userdata_compact_tostr(char *buf, size_t size, userdata_t *u)
{
    return snprintf(buf, size, "<%c:%s(%p)>",
                    (u->initializing ? 'I' : (u->dead ? 'D' : 'a')),
                    u->def->type_name, u);
}


static ssize_t userdata_oneline_tostr(char *buf, size_t size, userdata_t *u)
{
    void *data = USER_TO_DATA(u);

    return snprintf(buf, size, "<%c:%s(%p/%p)>",
                    (u->initializing ? 'I' : (u->dead ? 'D' : 'a')),
                    u->def->type_name, u, data);
}


static ssize_t userdata_short_tostr(char *buf, size_t size, userdata_t *u)
{
    return userdata_oneline_tostr(buf, size, u);
}


static ssize_t userdata_medium_tostr(char *buf, size_t size, userdata_t *u)
{
    return userdata_oneline_tostr(buf, size, u);
}


static ssize_t userdata_full_tostr(char *buf, size_t size, userdata_t *u)
{
    return userdata_oneline_tostr(buf, size, u);
}


static ssize_t userdata_verbose_tostr(char *buf, size_t size, userdata_t *u)
{
    return userdata_oneline_tostr(buf, size, u);
}


static ssize_t userdata_tostr(mrp_lua_tostr_mode_t mode, char *buf, size_t size,
                              userdata_t *u)
{
    switch (mode & MRP_LUA_TOSTR_MODEMASK) {
    case MRP_LUA_TOSTR_MINIMAL: return userdata_minimal_tostr(buf, size, u);
    default:
    case MRP_LUA_TOSTR_COMPACT: return userdata_compact_tostr(buf, size, u);
    case MRP_LUA_TOSTR_ONELINE: return userdata_oneline_tostr(buf, size, u);
    case MRP_LUA_TOSTR_SHORT:   return userdata_short_tostr  (buf, size, u);
    case MRP_LUA_TOSTR_MEDIUM:  return userdata_medium_tostr (buf, size, u);
    case MRP_LUA_TOSTR_FULL:    return userdata_full_tostr   (buf, size, u);
    case MRP_LUA_TOSTR_VERBOSE: return userdata_verbose_tostr(buf, size, u);
    }

    return 0;
}


/** Dump the given object to the provided buffer. */
ssize_t mrp_lua_object_tostr(mrp_lua_tostr_mode_t mode, char *buf, size_t size,
                             lua_State *L, void *data)
{
    userdata_t *u = userdata_get(data, CHECK);
    char       *p = buf;
    ssize_t     n = 0;;

    if (u == NULL)
        return snprintf(p, size, "<non-object %p>", u);

    if (mode & MRP_LUA_TOSTR_META) {
        n = userdata_tostr(mode, p, size, u);

        if (n <= 0)
            goto error;
        if ((size_t)n >= size)
            goto overflow;
    }

    if (mode & MRP_LUA_TOSTR_DATA) {
        p    += (size_t)n;
        size -= (size_t)n;

        if (u->def->tostring != NULL) {
            n = u->def->tostring(mode, p, size, L, data);

            if (n < 0)
                goto error;
            if ((size_t)n >= size)
                goto overflow;
        }
    }

    return (ssize_t)(p + n - buf);

 overflow:
    return (ssize_t)(p + n - buf);

 error:
    return n;
}


/** Dump the object at the given stack location to the provided buffer. */
ssize_t mrp_lua_index_tostr(mrp_lua_tostr_mode_t mode, char *buf, size_t size,
                            lua_State *L, int index)
{
    userdata_t **userdatap;

    userdatap = (userdata_t **)lua_touserdata(L, index);

    if (userdatap != NULL)
        return mrp_lua_object_tostr(mode, buf, size, L, *userdatap);
    else
        return snprintf(buf, size, "<invalid object, no userdata>");;
}


/** Dump all live or zombie Lua objects. */
void mrp_lua_dump_objects(mrp_lua_tostr_mode_t mode, lua_State *L, FILE *fp)
{
    mrp_lua_classdef_t *def;
    mrp_list_hook_t    *p, *n;
    userdata_t         *u;
    void               *d;
    int                 i, cnt;
    char                active[64], dead[64], created[64], destroyed[64];
    char                obj[4096];

    fprintf(fp, "Lua memory usage: %d k, %.2f M\n",
            lua_gc(L, LUA_GCCOUNT, 0), lua_gc(L, LUA_GCCOUNT, 0) / 1024.0);

    fprintf(fp, "Objects by class/type: A=active, D=dead, "
            "c=created, d=destroyed\n");
    for (i = 0; i < nclassdef; i++) {
        def = classdefs[i];

        snprintf(active, sizeof(active), "%u", def->nactive);
        snprintf(dead, sizeof(dead), "%u", def->ndead);
        snprintf(created, sizeof(created), "%u", def->ncreated);
        snprintf(destroyed, sizeof(destroyed), "%u", def->ndestroyed);

        fprintf(fp, "<%s/%s>: A:%s, D:%s, c:%s, d:%s\n",
                def->class_name, def->type_name,
                active, dead, created, destroyed);

        cnt = 0;
        mrp_list_foreach(&def->objects, p, n) {
            u = mrp_list_entry(p, typeof(*u), hook[0]);
            d = USER_TO_DATA(u);

            if (mrp_lua_object_tostr(mode, obj, sizeof(obj), L, d) > 0)
                fprintf(fp, "    #%d: %s\n", cnt, obj);
            else
                fprintf(fp, "    failed to dump object %p(%p)\n", u, d);
            cnt++;
        }
    }
}


void mrp_lua_track_objects(bool enable)
{
    if (cfg.busy) {
        if (cfg.track != enable)
            mrp_log_warning("Can't %s Murphy Lua object tracking, "
                            "already in use.", enable ? "enable" : "disable");
        return;
    }

    if (!cfg.set) {
        cfg.track = enable;
        cfg.set   = true;

        mrp_log_info("Murphy Lua object tracking is now %s.",
                     enable ? "enabled" : "disabled");
    }
}


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
