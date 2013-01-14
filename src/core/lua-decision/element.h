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

#ifndef __MURPHY_LUA_ELEMENT_H__
#define __MURPHY_LUA_ELEMENT_H__

#include <lua.h>
#include <murphy-db/mqi-types.h>

#define MRP_LUA_ELEMENT_FIELDS                                  \
    const char              *name;                              \
    mrp_lua_element_mask_t   inpmask;                           \
    size_t                   ninput;                            \
    mrp_lua_element_input_t *inputs;                            \
    size_t                   noutput;                           \
    mrp_lua_mdb_table_t    **outputs;                           \
    void                   (*install)(lua_State *, void *);     \
    mrp_funcbridge_t        *update

#define mrp_lua_get_sink_name(s) \
    mrp_lua_get_element_name((mrp_lua_element_t *)(s))
#define mrp_lua_get_sink_name(s) \
    mrp_lua_get_element_name((mrp_lua_element_t *)(s))
#define mrp_lua_sink_get_input_count(s) \
    mrp_lua_element_get_input_count((mrp_lua_element_t *)(s))
#define mrp_lua_sink_get_input_name(s,i) \
    mrp_lua_element_get_input_name((mrp_lua_element_t *)(s),i)
#define mrp_lua_sink_get_input_index(s,n) \
    mrp_lua_element_get_input_index((mrp_lua_element_t *)(s),n);
#define mrp_lua_sink_get_column_index(s,i,n) \
    mrp_lua_element_get_column_index((mrp_lua_element_t *)(s),i,n)
#define mrp_lua_sink_get_column_count(s,i) \
    mrp_lua_element_get_column_count((mrp_lua_element_t *)(s),i)
#define mrp_lua_sink_get_column_type(s,i,c) \
    mrp_lua_element_get_column_type((mrp_lua_element_t *)(s),i,c)
#define mrp_lua_sink_get_row_count(s,i) \
    mrp_lua_element_get_row_count((mrp_lua_element_t *)(s),i)
#define mrp_lua_sink_get_string(s,i,c,r,b,l) \
    mrp_lua_element_get_string((mrp_lua_element_t *)(s),i,c,r,b,l)
#define mrp_lua_sink_get_integer(s,i,c,r) \
    mrp_lua_element_get_integer((mrp_lua_element_t *)(s),i,c,r)
#define mrp_lua_sink_get_unsigned(s,i,c,r) \
    mrp_lua_element_get_unsigned((mrp_lua_element_t *)(s),i,c,r)
#define mrp_lua_sink_get_floating(s,i,c,r) \
    mrp_lua_element_get_floating((mrp_lua_element_t *)(s),i,c,r)


typedef struct mrp_lua_element_s         mrp_lua_element_t;
typedef struct mrp_lua_sink_s            mrp_lua_sink_t;
typedef struct mrp_lua_element_input_s   mrp_lua_element_input_t;
typedef uint32_t                         mrp_lua_element_mask_t;

void mrp_lua_create_element_class(lua_State *L);

const char *mrp_lua_get_element_name(mrp_lua_element_t *el);
int mrp_lua_element_get_input_count(mrp_lua_element_t *el);
const char *mrp_lua_element_get_input_name(mrp_lua_element_t *el, int inpidx);
int mrp_lua_element_get_input_index(mrp_lua_element_t *el, const char *inpnam);
int mrp_lua_element_get_column_index(mrp_lua_element_t *el, int inpidx,
                                     const char *colnam);
int mrp_lua_element_get_column_count(mrp_lua_element_t *el, int inpidx);

mqi_data_type_t mrp_lua_element_get_column_type(mrp_lua_element_t *el,
                                                int inpidx, int colidx);
int mrp_lua_element_get_row_count(mrp_lua_element_t *el, int inpidx);
const char *mrp_lua_element_get_string(mrp_lua_element_t *el, int inpidx,
                                      int colidx, int rowidx,
                                      char * buf, int len);
int32_t mrp_lua_element_get_integer(mrp_lua_element_t *el, int inpidx,
                                   int colidx, int rowidx);

uint32_t mrp_lua_element_get_unsigned(mrp_lua_element_t *el, int inpidx,
                                     int colidx, int rowidx);
double mrp_lua_element_get_floating(mrp_lua_element_t *el, int inpidx,
                                   int colidx, int rowidx);

const char *mrp_lua_sink_get_interface(mrp_lua_sink_t *s);
const char *mrp_lua_sink_get_object(mrp_lua_sink_t *s);
const char *mrp_lua_sink_get_type(mrp_lua_sink_t *s);
const char *mrp_lua_sink_get_property(mrp_lua_sink_t *s);


#endif  /* __MURPHY_LUA_ELEMENT_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
