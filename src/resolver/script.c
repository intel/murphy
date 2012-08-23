#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>

#include "resolver.h"


/*
 * With the low expected number of interpreters it is probably
 * much faster to keep interpreters in a linked list than to
 * hash them in...
 */
static MRP_LIST_HOOK(interpreters);                /* registered interpreters */
static const char *default_interpreter = "simple"; /* the default interpreter */


void set_default_interpreter(const char *type)
{
    default_interpreter = type;
}


int register_interpreter(mrp_interpreter_t *i)
{
    mrp_list_append(&interpreters, &i->hook);

    return TRUE;
}


void unregister_interpreter(mrp_interpreter_t *i)
{
    mrp_list_delete(&i->hook);
}


mrp_interpreter_t *lookup_interpreter(const char *name)
{
    mrp_list_hook_t   *p, *n;
    mrp_interpreter_t *i;

    if (!strcmp(name, "default"))
        name = default_interpreter;

    mrp_list_foreach(&interpreters, p, n) {
        i = mrp_list_entry(p, typeof(*i), hook);
        if (!strcmp(i->name, name))
            return i;
    }

    return NULL;
}


mrp_script_t *create_script(char *type, const char *source)
{
    mrp_interpreter_t *i;
    mrp_script_t      *s;

    s = NULL;
    i = lookup_interpreter(type);

    if (i != NULL) {
        s = mrp_allocz(sizeof(*s));

        if (s != NULL) {
            s->interpreter = i;
            s->source      = mrp_strdup(source);

            if (s->source != NULL)
                return s;
            else {
                mrp_free(s);
                s = NULL;
            }
        }
    }
    else
        errno = ENOENT;

    return NULL;
}


void destroy_script(mrp_script_t *script)
{
    if (script != NULL) {
        script->interpreter->cleanup(script);

        mrp_free(script->source);
        mrp_free(script);
    }
}


int compile_script(mrp_script_t *s)
{
    if (s != NULL)
        return s->interpreter->compile(s);
    else
        return 0;
}


int execute_script(mrp_resolver_t *r, mrp_script_t *s)
{
    MRP_UNUSED(r);

    if (s != NULL)
        return s->interpreter->execute(s);
    else
        return TRUE;
}


int eval_script(mrp_resolver_t *r, char *script)
{
    MRP_UNUSED(r);

    if (script == NULL)
        return TRUE;
    else {
        printf("----- running update script -----\n");
        printf("%s", script);
        printf("---------------------------------\n");

        return TRUE;
    }
}


char *mrp_print_value(char *buf, size_t size, mrp_script_value_t *value)
{
#define HANDLE_TYPE(type, fmt, val)                     \
        case MRP_SCRIPT_TYPE_##type:                    \
            snprintf(buf, size, fmt, val);              \
            break

    switch (value->type) {
        HANDLE_TYPE(UNKNOWN, "%s"     , "<unknown/invalid type>");
        HANDLE_TYPE(STRING , "'%s'"   , value->str);
        HANDLE_TYPE(BOOL   , "%s"     , value->bln ? "true" : "false");
        HANDLE_TYPE(UINT8  , "%uU8"   , value->u8);
        HANDLE_TYPE(SINT8  , "%dS8"   , value->s8);
        HANDLE_TYPE(UINT16 , "%uU16"  , value->u16);
        HANDLE_TYPE(SINT16 , "%dS16"  , value->s16);
        HANDLE_TYPE(UINT32 , "%uU32"  , value->u32);
        HANDLE_TYPE(SINT32 , "%dS32"  , value->s32);
        HANDLE_TYPE(UINT64 , "%lluU64", (unsigned long long)value->u64);
        HANDLE_TYPE(SINT64 , "%lldS64", (  signed long long)value->s64);
        HANDLE_TYPE(DOUBLE , "%f"     , value->dbl);
    default:
        snprintf(buf, size, "<invalid type 0x%x>", value->type);
    }

#undef HANDLE_TYPE

    return buf;
}
