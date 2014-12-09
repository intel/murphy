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

#include <stdarg.h>
#include <errno.h>

#include <murphy/common/mm.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>

#include "scanner.h"
#include "resolver-types.h"
#include "target.h"
#include "target-sorter.h"
#include "fact.h"
#include "resolver.h"


mrp_resolver_t *mrp_resolver_create(mrp_context_t *ctx)
{
    mrp_resolver_t *r;

    r = mrp_allocz(sizeof(mrp_resolver_t));

    if (r != NULL) {
        r->ctx  = ctx;
        r->ctbl = mrp_create_context_table();
        r->bus  = mrp_event_bus_get(ctx->ml, MRP_RESOLVER_BUS);

        if (r->ctbl != NULL && r->bus != NULL)
            return r;

        mrp_free(r);
    }

    return NULL;
}


mrp_resolver_t *mrp_resolver_parse(mrp_resolver_t *r, mrp_context_t *ctx,
                                   const char *path)
{
    yy_res_parser_t parser;

    mrp_clear(&parser);

    if (r == NULL) {
        r = mrp_resolver_create(ctx);

        if (r == NULL)
            return NULL;
    }

    if (parser_parse_file(&parser, path)) {
        if (create_targets(r, &parser) == 0 &&
            sort_targets(r)            == 0 &&
            compile_target_scripts(r)  == 0) {
            parser_cleanup(&parser);
            return r;
        }
    }
    else
        mrp_log_error("Failed to parse resolver input.");

    mrp_resolver_destroy(r);
    parser_cleanup(&parser);

    return NULL;
}


int mrp_resolver_prepare(mrp_resolver_t *r)
{
    return (prepare_target_scripts(r) == 0);
}


void mrp_resolver_destroy(mrp_resolver_t *r)
{
    if (r != NULL) {
        mrp_destroy_context_table(r->ctbl);
        destroy_targets(r);
        destroy_facts(r);

        mrp_free(r);
    }
}


int mrp_resolver_add_target(mrp_resolver_t *r, const char *target,
                            const char **depend, int ndepend,
                            const char *script_type,
                            const char *script_source)
{
    return (create_target(r, target, depend, ndepend,
                          script_type, script_source) != NULL);
}


int mrp_resolver_add_alias(mrp_resolver_t *r, const char *target,
                           const char *alias)
{
    const char *depend[1] = { target };

    return (create_target(r, alias, depend, 1, NULL, NULL) != NULL);
}


int mrp_resolver_add_prepared_target(mrp_resolver_t *r, const char *target,
                                     const char **depend, int ndepend,
                                     mrp_interpreter_t *interpreter,
                                     void  *compiled_data, void *target_data)
{
    mrp_scriptlet_t *script;
    target_t        *t;

    t = create_target(r, target, depend, ndepend, NULL, NULL);

    if (t != NULL) {
        if (interpreter != NULL) {
            script = mrp_allocz(sizeof(*script));

            if (script != NULL) {
                script->interpreter = interpreter;
                script->data        = target_data;
                script->compiled    = compiled_data;

                t->script = script;
            }
            else
                return FALSE;
        }

        t->precompiled = TRUE;
        t->prepared    = TRUE;

        return TRUE;
    }

    return FALSE;
}


int mrp_resolver_enable_autoupdate(mrp_resolver_t *r, const char *name)
{
    return generate_autoupdate_target(r, name);
}


int mrp_resolver_update_targetl(mrp_resolver_t *r, const char *target, ...)
{
    const char         *name;
    mrp_script_value_t  value;
    va_list             ap;
    int                 id, status;

    if (mrp_push_context_frame(r->ctbl) == 0) {
        va_start(ap, target);
        while ((name = va_arg(ap, char *)) != NULL) {
            id = mrp_get_context_id(r->ctbl, name);

            if (id > 0) {
                value.type = va_arg(ap, int);

#define         HANDLE_TYPE(_type, _member, _va_type)                   \
                case MRP_SCRIPT_TYPE_##_type:                           \
                    value._member =                                     \
                        (typeof(value._member))va_arg(ap, _va_type);    \
                    break

                switch (value.type) {
                    HANDLE_TYPE(STRING, str, char *  );
                    HANDLE_TYPE(BOOL  , bln, int     );
                    HANDLE_TYPE(UINT8 ,  u8, uint32_t);
                    HANDLE_TYPE(SINT8 ,  s8, int32_t );
                    HANDLE_TYPE(UINT16, u16, uint32_t);
                    HANDLE_TYPE(SINT16, s16, int32_t );
                    HANDLE_TYPE(UINT32, u32, uint32_t);
                    HANDLE_TYPE(SINT32, s32, int32_t );
                    HANDLE_TYPE(UINT64, u64, uint64_t);
                    HANDLE_TYPE(SINT64, u64, uint64_t);
                    HANDLE_TYPE(DOUBLE, dbl, double  );
                default:
                    errno  = EINVAL;
                    status = -1;
                    goto pop_frame;
                }
#undef          HANDLE_TYPE

                if (mrp_set_context_value(r->ctbl, id, &value) < 0) {
                    status = -1;
                    goto pop_frame;
                }
            }
            else {
                errno  = ESRCH;
                status = -1;
                goto pop_frame;
            }
        }

        status = update_target_by_name(r, target);

    pop_frame:
        mrp_pop_context_frame(r->ctbl);
        va_end(ap);
    }
    else
        status = -1;

    return status;
}


int mrp_resolver_update_targetv(mrp_resolver_t *r, const char *target,
                                const char **variables,
                                mrp_script_value_t *values,
                                int nvariable)
{
    const char         *name;
    mrp_script_value_t *value;
    int                 id, i, status;

    if (mrp_push_context_frame(r->ctbl) == 0) {
        for (i = 0; i < nvariable; i++) {
            name  = variables[i];
            value = values + i;
            id    = mrp_get_context_id(r->ctbl, name);

            if (id > 0) {
                if (mrp_set_context_value(r->ctbl, id, value) < 0) {
                    status = -1;
                    goto pop_frame;
                }
            }
            else {
                errno  = ESRCH;
                status = -1;
                goto pop_frame;
            }
        }

        status = update_target_by_name(r, target);

    pop_frame:
        mrp_pop_context_frame(r->ctbl);
    }
    else
        status = -1;

    return status;
}


void mrp_resolver_dump_targets(mrp_resolver_t *r, FILE *fp)
{
    fprintf(fp, "%d target%s\n", r->ntarget, r->ntarget != 1 ? "s" : "");
    dump_targets(r, fp);
}


void mrp_resolver_dump_facts(mrp_resolver_t *r, FILE *fp)
{
    int     i;
    fact_t *f;

    fprintf(fp, "%d fact%s\n", r->nfact, r->nfact != 1 ? "s" : "");
    for (i = 0; i < r->nfact; i++) {
        f = r->facts + i;
        fprintf(fp, "  #%d: %s (@%u)\n", i, f->name, fact_stamp(r, i));
    }
}


int mrp_resolver_register_interpreter(mrp_interpreter_t *i)
{
    return mrp_register_interpreter(i);
}


int mrp_resolver_unregister_interpreter(const char *name)
{
    return mrp_unregister_interpreter(name);
}


int mrp_resolver_declare_variable(mrp_resolver_t *r, const char *name,
                                  mrp_script_type_t type)
{
    return mrp_declare_context_variable(r->ctbl, name, type);
}


int mrp_resolver_get_value(mrp_resolver_t *r, int id, mrp_script_value_t *v)
{
    return mrp_get_context_value(r->ctbl, id, v);
}


int mrp_resolver_get_value_by_name(mrp_resolver_t *r, const char *name,
                                   mrp_script_value_t *v)
{
    return mrp_get_context_value(r->ctbl, mrp_get_context_id(r->ctbl, name), v);
}
