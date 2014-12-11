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

#include <unistd.h>

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/core/plugin.h>
#include <murphy/core/lua-utils/funcbridge.h>
#include <murphy/core/lua-decision/mdb.h>
#include <murphy/core/lua-decision/element.h>
#include <murphy/core/lua-bindings/murphy.h>

static mrp_context_t *context;
static MRP_LIST_HOOK(bindings);
static MRP_LIST_HOOK(pending);
static int debug_level;
static char *config_file;
static char *config_dir;

static lua_Alloc setup_allocator(void);


static int create_murphy_object(lua_State *L)
{
    mrp_lua_murphy_t *m;

    m = (mrp_lua_murphy_t *)lua_newuserdata(L, sizeof(*m));

    m->ctxp = &context;

    luaL_getmetatable(L, "murphy");
    lua_setmetatable(L, -2);

    return 1;
}


static int register_murphy(mrp_context_t *ctx)
{
    static luaL_reg functions[] = {
        { "get", create_murphy_object },
        { NULL , NULL                 }
    };
    lua_State *L = ctx->lua_state;

    luaL_newmetatable(L, "murphy");
    lua_pushliteral(L, "__index");       /* murphy.__index = murphy */
    lua_pushvalue(L, -2);
    lua_settable(L, -3);

    luaL_openlib(L, "murphy", functions, 0);

    return TRUE;
}


static int register_bindings(mrp_lua_bindings_t *b)
{
    lua_State *L = context->lua_state;
    luaL_reg  *m;

    luaL_getmetatable(L, b->meta);

    for (m = b->methods; m->name != NULL; m++) {
        lua_pushstring(L, m->name);
        lua_pushcfunction(L, m->func);
        lua_rawset(L, -3);
    }

    if (b->classdef != NULL) {
        if (mrp_lua_create_object_class(L, b->classdef) < 0) {
            mrp_log_error("Object class registration failed.");
            return FALSE;
        }
    }

    return TRUE;
}


int mrp_lua_register_murphy_bindings(mrp_lua_bindings_t *b)
{
    mrp_context_t   *ctx;
    lua_State       *L;

    mrp_list_init(&b->hook);
    mrp_list_append(&bindings, &b->hook);

    if ((ctx = context) != NULL && (L = ctx->lua_state) != NULL)
        return register_bindings(b);
    else
        return TRUE;
}


static void init_lua_utils(lua_State *L)
{
    mrp_create_funcbridge_class(L);
    mrp_create_funcarray_class(L);
}


static void init_lua_decision(lua_State *L)
{
    mrp_lua_create_mdb_class(L);
    mrp_lua_create_element_class(L);
}


static lua_State *init_lua(void)
{
    lua_Alloc  A = setup_allocator();
    lua_State *L;

    if (A == NULL)
        L = luaL_newstate();
    else
        L = lua_newstate(A, NULL);

    if (L != NULL) {
        luaopen_base(L);
        init_lua_utils(L);
        init_lua_decision(L);
    }

    return L;
}


lua_State *mrp_lua_set_murphy_context(mrp_context_t *ctx)
{
    lua_State          *L;
    mrp_list_hook_t    *p, *n;
    mrp_lua_bindings_t *b;
    int                 success;

    if (context == NULL) {
        L = init_lua();

        if (L != NULL) {
            ctx->lua_state = L;
            context        = ctx;

            if (register_murphy(ctx)) {
                success = TRUE;

                init_lua_utils(L);
                init_lua_decision(L);

                mrp_list_foreach(&bindings, p, n) {
                    b = mrp_list_entry(p, typeof(*b), hook);
                    success &= register_bindings(b);
                }

                return L;
            }
        }
    }

    return NULL;
}


void mrp_lua_set_murphy_lua_config_file(const char *path)
{
    if (config_file == NULL && path != NULL) {
        config_file = mrp_strdup(path);
        mrp_log_info("Lua config file is: '%s'.", config_file);
    }
}


mrp_context_t *mrp_lua_check_murphy_context(lua_State *L, int index)
{
    mrp_lua_murphy_t *m;

    m = (mrp_lua_murphy_t *)luaL_checkudata(L, index, "murphy");
    luaL_argcheck(L, m, index, "murphy object expected");

    if (*m->ctxp == NULL)
        return (void *)(ptrdiff_t)luaL_error(L, "murphy context is not set");
    else
        return *m->ctxp;
}


mrp_context_t *mrp_lua_get_murphy_context(void)
{
    return context;
}


lua_State *mrp_lua_get_lua_state(void)
{
    if (context != NULL)
        return context->lua_state;
    else
        return NULL;
}

const char *mrp_lua_get_murphy_lua_config_dir(void)
{
    char   *base, dir[PATH_MAX];
    size_t  offs, len;

    if (config_file == NULL)
        return NULL;

    if (config_dir == NULL) {
        if (config_file[0] != '/') {
            if (getcwd(dir, sizeof(dir)) == NULL)
                return NULL;
            else {
                offs = strlen(dir);

                if (offs >= sizeof(dir) - 1)
                    return NULL;

                dir[offs++] = '/';
                dir[offs]   = '\0';
            }
        }
        else
            offs = 0;

        base = strrchr(config_file, '/');

        if (base != NULL)
            while (base > config_file && base[-1] == '/')
                base--;

        if (base != NULL && base > config_file) {
            len = base - config_file;

            if (sizeof(dir) - offs - 1 <= len)
                return NULL;

            strncpy(dir + offs, config_file, len);
            offs += len;
            dir[offs] = '\0';

            config_dir = mrp_strdup(dir);

            mrp_log_info("Lua config directory is '%s'.", config_dir);
        }
    }

    return config_dir;
}


/*
 * runtime debugging
 */

void mrp_lua_dump_stack(lua_State *L, const char *prefix)
{
    char prebuf[256];
    int  i, n;

    n = lua_gettop(L);

    if (prefix != NULL && *prefix) {
        snprintf(prebuf, sizeof(prebuf), "%s: ", prefix);
        prefix = prebuf;
    }
    else
        prefix = "";

    if (n > 0) {
        mrp_debug("%sLua stack dump (%d items):", prefix, n);

        for (i = 1; i <= n; i++)
            mrp_debug("%s#%d(%d): %s", prefix, -i, (n - i) + 1,
                      lua_typename(L, lua_type(L, -i)));
    }
    else
        mrp_debug("%sLua stack is empty", prefix);
}


static void lua_debug(lua_State *L, lua_Debug *ar)
{
#define RUNNING(_ar, _what) ((_ar)->what != NULL && !strcmp((_ar)->what, _what))
#define ALIGNFMT "%*.*s"
#define ALIGNARG 4 * depth, 4 * depth, ""

    static int depth = 0;

    lua_Debug   f;
    const char *type, *name;
    char        loc[1024];

    switch (ar->event) {
    case LUA_HOOKRET:
        depth--;
        mrp_debug(ALIGNFMT"<= return", ALIGNARG);
        break;

#ifdef LUA_HOOKTAILRET
    case LUA_HOOKTAILRET:
        depth--;
        mrp_debug(ALIGNFMT"<= tail return", ALIGNARG);
        break;
#endif

    case LUA_HOOKCALL:
#ifdef LUA_HOOKTAILCALL
    case LUA_HOOKTAILCALL:
#endif

        mrp_clear(&f);
        if (lua_getstack(L, 1, &f) && lua_getinfo(L, "Snl", &f)) {
            if      (RUNNING(&f, "C"))    type = "Lua-C";
            else if (RUNNING(&f, "Lua"))  type = "Lua";
            else if (RUNNING(&f, "main")) type = "Lua-main";
            else if (RUNNING(&f, "tail")) {
                mrp_debug(ALIGNFMT"<=> tail-call", ALIGNARG);
#ifndef LUA_HOOKTAILRET
                depth++;
#endif
                return;
            }
            else
                type = "???";

            name = f.name ? f.name : NULL;

            if (f.currentline != -1 && f.short_src[0])
                snprintf(loc, sizeof(loc), "@ %s:%d", f.short_src,
                         f.currentline);
            else
                loc[0] = '\0';

            if (name)
                mrp_debug(ALIGNFMT"=> %s %s %s", ALIGNARG, type, name, loc);
            else
                mrp_debug(ALIGNFMT"=> %s %s", ALIGNARG, type, loc);
        }
        else
            mrp_debug(ALIGNFMT"=> Lua", ALIGNARG);

        depth++;
        break;

    case LUA_HOOKLINE:
        mrp_clear(&f);

        if (lua_getstack(L, 1, &f) && lua_getinfo(L, "Snl", &f))
            mrp_debug(ALIGNFMT" @ %s:%d", ALIGNARG, f.short_src, f.currentline);
        else
            mrp_debug(ALIGNFMT" @ line %d", ALIGNARG, ar->currentline);
        break;

    default:
        break;
    }

#undef RUNNING
#undef ALIGNFMT
#undef ALIGNARG
}


static int setup_debug_hook(int mask)
{
    mrp_context_t *ctx     = mrp_lua_get_murphy_context();
    lua_State     *L       = ctx ? ctx->lua_state : NULL;

    return (L != NULL && lua_sethook(L, lua_debug, mask, 0));
}


static void clear_debug_hook(void)
{
    mrp_context_t *ctx = mrp_lua_get_murphy_context();
    lua_State     *L   = ctx ? ctx->lua_state : NULL;

    if (L != NULL)
        lua_sethook(L, lua_debug, 0, 0);
}


int mrp_lua_set_debug(mrp_lua_debug_t level)
{
    const char *cfg;
    int         ena, mask, success;

    if (debug_level)
        clear_debug_hook();

    mask = 0;

    switch (level) {
    case MRP_LUA_DEBUG_DISABLED:
        ena = FALSE;
        cfg = "-lua_debug";
        break;

    case MRP_LUA_DEBUG_DETAILED:
        mask |= LUA_MASKLINE;
    case MRP_LUA_DEBUG_ENABLED:
        mask |= LUA_MASKCALL | LUA_MASKRET;
        ena   = TRUE;
        cfg   = "+lua_debug";
        break;

    default:
        return FALSE;
    }

    if (ena) {
        success = setup_debug_hook(mask);
        mrp_debug_enable(TRUE);
    }
    else
        success = TRUE;

    mrp_debug_set_config(cfg);

    if (success)
        debug_level = level;

    return success;
}


/*
 * Lua memory allocation tracking
 *
 * This is intended for debugging and diagnostic purposes. By default
 * tracking Lua allocations is off. Lua allocation tracking can be
 * enabled by including lua=true in the environment variable controlling
 * Murphy memory management (__MURPHY_MM_CONFIG).
 */

/*
 * a tracked block of memory allocated for Lua by us
 */

#define MEMBLK_SIZE(lsize) \
    ((lsize) ? ((void *)&((memblk_t *)NULL)->mem[(lsize)] - NULL) : 0)

typedef struct {
    mrp_list_hook_t hook;                /* hook to hash bucket */
    char            mem[0];              /* memory passed on to Lua */
} memblk_t;

static MRP_LIST_HOOK(memblks);           /* allocated blocks */


static inline void *memblk_store(memblk_t *blk)
{
    mrp_list_init(&blk->hook);
    mrp_list_append(&memblks, &blk->hook);

    return &blk->mem[0];
}


static void memblk_clear(memblk_t *blk)
{
    mrp_list_delete(&blk->hook);
}


static inline memblk_t *ptr_to_memblk(void *ptr)
{
    memblk_t *blk;

    if (ptr != NULL)
        blk = (memblk_t *)(((char *)ptr) - MRP_OFFSET(typeof(*blk), mem[0]));
    else
        blk = NULL;

    return blk;
}


static inline void *memblk_to_ptr(memblk_t *blk)
{
    if (blk != NULL)
        return (void *)&blk->mem[0];
    else
        return NULL;
}


static inline void *memblk_alloc(size_t lsize)
{
    memblk_t *blk;

    blk = mrp_alloc(MEMBLK_SIZE(lsize));

    if (blk != NULL) {
        return memblk_store(blk);
    }
    else
        return NULL;
}


static inline void *memblk_resize(memblk_t *blk, size_t osize, size_t nsize)
{
    memblk_clear(blk);

    if (mrp_reallocz(blk, osize, nsize)) {
        return memblk_store(blk);
    }
    else {
        mrp_free(blk);
        return NULL;
    }
}


static inline void memblk_free(memblk_t *blk)
{
    if (blk != NULL) {
        memblk_clear(blk);
        mrp_free(blk);
    }
}


static void *lua_alloc(void *ud, void *optr, size_t olsize, size_t nlsize)
{
    memblk_t *oblk;
    size_t    obsize, nbsize;
    void     *nptr;

    MRP_UNUSED(ud);

    mrp_debug("Lua allocation request <%p, %zd, %zd>", optr, olsize, nlsize);

    oblk = ptr_to_memblk(optr);

    if (nlsize > 0) {
        nbsize = MEMBLK_SIZE(nlsize);

        if (oblk != NULL) {
            obsize = MEMBLK_SIZE(olsize);
            nptr   = memblk_resize(oblk, obsize, nbsize);
        }
        else
            nptr = memblk_alloc(nbsize);
    }
    else {
        memblk_free(oblk);
        nptr = NULL;
    }

    mrp_debug("Lua allocation reply %p", nptr);

    return nptr;
}


static lua_Alloc setup_allocator(void)
{
    int debug;

    debug = mrp_mm_config_bool("lua", FALSE);

    if (!debug) {
        mrp_debug("%s not set to debug*, using native Lua allocator",
                  MRP_MM_CONFIG_ENVVAR);
        return NULL;
    }
    else {
        mrp_debug("Lua memory tracking enabled, overriding native allocator");

        mrp_list_init(&memblks);
        mrp_lua_track_objects(true);

        return lua_alloc;
    }
}
