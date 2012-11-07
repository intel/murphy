#ifndef __MURPHY_LUA_BINDINGS_H__
#define __MURPHY_LUA_BINDINGS_H__

#include <lualib.h>
#include <lauxlib.h>

#include <murphy/common/list.h>
#include <murphy/core/context.h>


typedef struct {
    const char      *meta;               /* add method to this metatable */
    luaL_reg        *methods;            /* Lua method table to register */
    mrp_list_hook_t  hook;               /* to list of registered bindings */
} mrp_lua_bindings_t;


typedef struct {
    mrp_context_t **ctxp;                /* murphy context */
} mrp_lua_murphy_t;


/** Macro to automatically register murphy Lua bindings on startup. */
#define MURPHY_REGISTER_LUA_BINDINGS(_metatbl, ...)            \
    static void register_##_metatbl##_bindings(void) MRP_INIT; \
                                                               \
    static void register_##_metatbl##_bindings(void) {         \
        static struct luaL_reg methods[] = {                   \
            __VA_ARGS__,                                       \
            { NULL, NULL }                                     \
        };                                                     \
        static mrp_lua_bindings_t b = {                        \
            .meta    = #_metatbl,                              \
            .methods = methods,                                \
        };                                                     \
                                                               \
        mrp_list_init(&b.hook);                                \
        mrp_lua_register_murphy_bindings(&b);                  \
    }


/** Set murphy context for the bindings. */
lua_State *mrp_lua_set_murphy_context(mrp_context_t *ctx);

/** Get murphy context for the bindings. */
mrp_context_t *mrp_lua_get_murphy_context(void);

/** Get the common Lua state for the bindings. */
lua_State *mrp_lua_get_lua_state(void);

/** Register the given lua murphy bindings. */
int mrp_lua_register_murphy_bindings(mrp_lua_bindings_t *b);

/** Check and get murphy context for the bindings. */
mrp_context_t *mrp_lua_check_murphy_context(lua_State *L, int index);


#endif /* __MURPHY_LUA_BINDINGS_H__ */
