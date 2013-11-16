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

#ifndef __MURPHY_LUA_FUNCBRIDGE_H__
#define __MURPHY_LUA_FUNCBRIDGE_H__

#include <stdint.h>
#include <stdbool.h>

typedef union  mrp_funcbridge_value_u  mrp_funcbridge_value_t;
typedef enum   mrp_funcbridge_type_e   mrp_funcbridge_type_t;
typedef struct mrp_funcbridge_s        mrp_funcbridge_t;
typedef struct mrp_funcarray_s         mrp_funcarray_t;

typedef bool (*mrp_funcbridge_cfunc_t)(lua_State *, void *,
                                       const char *, mrp_funcbridge_value_t *,
                                       char *, mrp_funcbridge_value_t *);

#include "murphy/core/lua-utils/object.h"

#define MRP_FUNCBRIDGE_NO_DATA      0
#define MRP_FUNCBRIDGE_UNSUPPORTED '?'
#define MRP_FUNCBRIDGE_STRING      's'
#define MRP_FUNCBRIDGE_INTEGER     'd'
#define MRP_FUNCBRIDGE_FLOATING    'f'
#define MRP_FUNCBRIDGE_DOUBLE      MRP_FUNCBRIDGE_FLOATING
#define MRP_FUNCBRIDGE_BOOLEAN     'b'
#define MRP_FUNCBRIDGE_POINTER     'p'
#define MRP_FUNCBRIDGE_OBJECT      'o'
#define MRP_FUNCBRIDGE_ARRAY       '['
#define MRP_FUNCBRIDGE_ARRAY_END   ']'
#define MRP_FUNCBRIDGE_MRPLUATYPE  'O'
#define MRP_FUNCBRIDGE_ANY         '*'
enum mrp_funcbridge_type_e {
    MRP_C_FUNCTION = 1,
    MRP_LUA_FUNCTION
};


union mrp_funcbridge_value_u {
    const char *string;
    int32_t     integer;
    double      floating;
    bool        boolean;
    void       *pointer;
    struct {
        void   *items;
        size_t  nitem;
        char    type;
    } array;
};

struct mrp_funcbridge_s {
    mrp_funcbridge_type_t   type;
    struct {
        char *signature;
        mrp_lua_type_t *sigtypes;
        mrp_funcbridge_cfunc_t func;
        void *data;
    }                       c;
    int                     luatbl;
    int                     refcnt;
    int                     dead : 1;
    int                     autobridge : 1;  /* autobridged member */
    int                     usestack : 1;    /* also uses the Lua stack */
};

struct mrp_funcarray_s {
    size_t             nfunc;
    mrp_funcbridge_t **funcs;
    int                luatbl;
};


void mrp_create_funcbridge_class(lua_State *);
void mrp_create_funcarray_class(lua_State *);


mrp_funcbridge_t *mrp_funcbridge_create_cfunc(lua_State *, const char *,
                                              const char *,
                                              mrp_funcbridge_cfunc_t, void *);
mrp_funcbridge_t *mrp_funcbridge_create_luafunc(lua_State *, int);
mrp_funcbridge_t *mrp_funcbridge_ref(lua_State *L, mrp_funcbridge_t *);
void              mrp_funcbridge_unref(lua_State *L, mrp_funcbridge_t *);
bool              mrp_funcbridge_call_from_c(lua_State *,  mrp_funcbridge_t *,
                                             const char *,
                                             mrp_funcbridge_value_t *,
                                             char *,
                                             mrp_funcbridge_value_t *);
int mrp_call_funcbridge(lua_State *L, mrp_funcbridge_t *fb, int f);
mrp_funcbridge_t *mrp_funcbridge_check(lua_State *, int);
int               mrp_funcbridge_push(lua_State *, mrp_funcbridge_t *);

mrp_funcarray_t  *mrp_funcarray_create(lua_State *);
bool              mrp_funcarray_call_from_c(lua_State *, mrp_funcarray_t *,
                                            const char *,
                                            mrp_funcbridge_value_t *);
mrp_funcarray_t  *mrp_funcarray_check(lua_State *, int);



#endif  /* __MURPHY_LUA_FUNCBRIDGE_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
