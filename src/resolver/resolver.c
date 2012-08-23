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
#include "context.h"
#include "resolver.h"

mrp_resolver_t *mrp_resolver_parse(const char *path)
{
    yy_res_parser_t  parser;
    mrp_resolver_t  *r;

    mrp_clear(&parser);
    r = mrp_allocz(sizeof(*r));

    if (r != NULL) {
        if (parser_parse_file(&parser, path)) {
            if (create_targets(r, &parser) == 0 &&
                sort_targets(r)            == 0 &&
                compile_target_scripts(r)  == 0 &&
                init_context_table(r)      == 0) {
                parser_cleanup(&parser);
                return r;
            }
        }
        else
            mrp_log_error("Failed to parse resolver input.");
    }

    mrp_resolver_cleanup(r);
    parser_cleanup(&parser);

    return NULL;
}


void mrp_resolver_cleanup(mrp_resolver_t *r)
{
    if (r != NULL) {
        cleanup_context_table(r);
        destroy_targets(r);
        destroy_facts(r);

        mrp_free(r);
    }
}


int mrp_resolver_update_targetl(mrp_resolver_t *r, const char *target, ...)
{
    const char         *name;
    mrp_script_value_t  value;
    va_list             ap;
    int                 id, status;

    if (push_context_frame(r) == 0) {
        va_start(ap, target);
        while ((name = va_arg(ap, char *)) != NULL) {
            id = get_context_id(r, name);

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

                if (set_context_value(r, id, &value) < 0) {
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
        pop_context_frame(r);
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

    if (push_context_frame(r) == 0) {
        for (i = 0; i < nvariable; i++) {
            name  = variables[i];
            value = values + i;
            id    = get_context_id(r, name);

            if (id > 0) {
                if (set_context_value(r, id, value) < 0) {
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
        pop_context_frame(r);
    }
    else
        status = -1;

    return status;
}


void mrp_resolver_dump_targets(mrp_resolver_t *r, FILE *fp)
{
    dump_targets(r, fp);
}


void mrp_resolver_dump_facts(mrp_resolver_t *r, FILE *fp)
{
    int     i;
    fact_t *f;

    fprintf(fp, "%d facts\n", r->nfact);
    for (i = 0; i < r->nfact; i++) {
        f = r->facts + i;
        fprintf(fp, "  #%d: %s\n", i, f->name);
    }
}


int mrp_resolver_register_interpreter(mrp_interpreter_t *i)
{
    return register_interpreter(i);
}


int mrp_resolver_unregister_interpreter(const char *name)
{
    mrp_interpreter_t *i;

    i = lookup_interpreter(name);

    if (i != NULL) {
        unregister_interpreter(i);

        return TRUE;
    }
    else
        return FALSE;

}


int mrp_resolver_declare_variable(mrp_resolver_t *r, const char *name,
                                  mrp_script_type_t type)
{
    return declare_context_variable(r, name, type);
}


int mrp_resolver_get_value(mrp_resolver_t *r, int id, mrp_script_value_t *v)
{
    return get_context_value(r, id, v);
}


int mrp_resolver_get_value_by_name(mrp_resolver_t *r, const char *name,
                                   mrp_script_value_t *v)
{
    return get_context_value(r, get_context_id(r, name), v);
}
