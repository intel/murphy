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

#ifndef __MURPHY_LUA_MDB_H__
#define __MURPHY_LUA_MDB_H__

#include <murphy-db/mqi.h>

typedef struct mrp_lua_mdb_table_s        mrp_lua_mdb_table_t;
typedef struct mrp_lua_mdb_select_s       mrp_lua_mdb_select_t;
typedef struct mrp_lua_mdb_dependency_s   mrp_lua_mdb_dependency_t;

void mrp_lua_create_mdb_class(lua_State *L);
mrp_lua_mdb_table_t *mrp_lua_create_builtin_table(lua_State *L,
                                                  mqi_handle_t handle);
mrp_lua_mdb_table_t *mrp_lua_table_check(lua_State *L, int idx);
mrp_lua_mdb_table_t *mrp_lua_to_table(lua_State *L, int idx);
int mrp_lua_push_table(lua_State *L, mrp_lua_mdb_table_t *tbl);
const char *mrp_lua_table_name(mrp_lua_mdb_table_t *tbl);

mrp_lua_mdb_select_t *mrp_lua_select_check(lua_State *L, int idx);
mrp_lua_mdb_select_t *mrp_lua_to_select(lua_State *L, int idx);
int mrp_lua_push_select(lua_State *L,mrp_lua_mdb_select_t *sel,bool singleval);
const char * mrp_lua_select_name(mrp_lua_mdb_select_t *sel);
int mrp_lua_select_get_column_index(mrp_lua_mdb_select_t *sel,
                                    const char *colnam);
int mrp_lua_select_get_column_count(mrp_lua_mdb_select_t *sel);
mqi_data_type_t mrp_lua_select_get_column_type(mrp_lua_mdb_select_t *sel,
                                               int colidx);
int mrp_lua_select_get_row_count(mrp_lua_mdb_select_t *sel);
const char *mrp_lua_select_get_string(mrp_lua_mdb_select_t *sel,
                                      int colidx, int rowidx,
                                      char * buf, int len);
int32_t mrp_lua_select_get_integer(mrp_lua_mdb_select_t *sel,
                                   int colidx, int rowidx);

uint32_t mrp_lua_select_get_unsigned(mrp_lua_mdb_select_t *sel,
                                     int colidx, int rowidx);
double mrp_lua_select_get_floating(mrp_lua_mdb_select_t *sel,
                                   int colidx, int rowidx);

int mrp_lua_dependency_add(lua_State *L, const char *name);


#endif  /* __MURPHY_LUA_MDB_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
