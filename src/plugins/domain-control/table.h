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

#ifndef __MURPHY_DOMAIN_CONTROL_TABLE_H__
#define __MURPHY_DOMAIN_CONTROL_TABLE_H__

#include <murphy-db/mql-result.h>

#include "client.h"
#include "domain-control-types.h"

int init_tables(pdp_t *pdp);
void destroy_tables(pdp_t *pdp);

int create_proxy_table(pep_table_t *t, int *errcode, const char **errmsg);

int create_proxy_watch(pep_proxy_t *proxy, int id,
                       const char *table, const char *mql_columns,
                       const char *mql_where, int max_rows,
                       int *error, const char **errmsg);

void destroy_watch_table(pdp_t *pdp, pep_table_t *t);

void destroy_proxy_table(pep_table_t *t);
void destroy_proxy_tables(pep_proxy_t *proxy);

void destroy_proxy_watches(pep_proxy_t *proxy);

int set_proxy_tables(pep_proxy_t *proxy, mrp_domctl_data_t *tables, int ntable,
                     int *error, const char **errmsg);

int exec_mql(mql_result_type_t type, mql_result_t **resultp,
             const char *format, ...);


#endif /* __MURPHY_DOMAIN_CONTROL_TABLE_H__ */
