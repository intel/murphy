#ifndef __MURPHY_LUA_COMPAT_H__
#define __MURPHY_LUA_COMPAT_H__

#include <lauxlib.h>
#include <murphy/common/log.h>

#if LUA_VERSION_NUM >= 503
#    ifndef LUA_COMPAT_MODULE

static void mrp_lua_compat_openlib(lua_State *L, const char *name,
                                   const luaL_Reg *lib, int nup)
{
    if (name == NULL)
        lua_newtable(L);
    else {
        lua_getglobal(L, name);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
        }
        else {
            mrp_log_error("%s: global \"%s\" already exists", __func__, name);
            mrp_log_error("%s: will reuse existing one", __func__);
        }
    }

    luaL_setfuncs(L, lib, nup);

    if (name != NULL)
        lua_setglobal(L, name);
    else {
        /* hmm... should we lua_pop(L, 1) here ? */
    }
}

#define luaL_openlib mrp_lua_compat_openlib

#    endif /* !LUA_COMPAT_MODULE */
#endif /* LUA_VERSION_NUM >= 503 */

#endif /* __MURPHY_LUA_COMPAT_H__ */
