#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/core/plugin.h>
#include <murphy/core/lua-bindings/murphy.h>

static mrp_context_t *context;
static MRP_LIST_HOOK(bindings);


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

    return TRUE;
}


int mrp_lua_register_murphy_bindings(mrp_lua_bindings_t *b)
{
    mrp_context_t   *ctx;
    lua_State       *L;

    mrp_list_append(&bindings, &b->hook);

    if ((ctx = context) != NULL && (L = ctx->lua_state) != NULL)
        return register_bindings(b);
    else
        return TRUE;
}


lua_State *mrp_lua_set_murphy_context(mrp_context_t *ctx)
{
    lua_State          *L;
    mrp_list_hook_t    *p, *n;
    mrp_lua_bindings_t *b;
    int                 success;

    if (context == NULL) {
        L = luaL_newstate();

        if (L != NULL) {
            ctx->lua_state = L;
            context        = ctx;

            if (register_murphy(ctx)) {
                success = TRUE;

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


mrp_context_t *mrp_lua_check_murphy_context(lua_State *L, int index)
{
    mrp_lua_murphy_t *m;

    m = (mrp_lua_murphy_t *)luaL_checkudata(L, index, "murphy");
    luaL_argcheck(L, m, index, "murphy object expected");

    if (*m->ctxp == NULL)
        luaL_error(L, "murphy context is not set");

    return *m->ctxp;
}


lua_State *mrp_lua_get_lua_state(void)
{
    if (context != NULL)
        return context->lua_state;
    else
        return NULL;
}
