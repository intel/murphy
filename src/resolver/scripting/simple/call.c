#include <murphy/common/mm.h>

#include "call.h"

function_call_t *create_call(char *function, arg_t *args, int narg)
{
    function_call_t *c;

    c = mrp_allocz(sizeof(*c));

    if (c != NULL) {
        c->name = mrp_strdup(function);

        if (c->name != NULL) {
            mrp_list_init(&c->hook);
            c->args = args;
            c->narg = narg;

            return c;
        }

        mrp_free(c);
    }

    return NULL;
}


void destroy_call(function_call_t *c)
{
    if (c != NULL) {
        mrp_list_delete(&c->hook);
        mrp_free(c->name);
        destroy_arguments(c->args, c->narg);
        mrp_free(c);
    }
}


int set_constant_value_arg(arg_t *arg, mrp_script_value_t *value)
{
    arg->cst.type  = ARG_CONST_VALUE;
    arg->cst.value = *value;

    if (value->type == MRP_SCRIPT_TYPE_STRING) {
        arg->cst.value.str = mrp_strdup(value->str);

        if (arg->cst.value.str != NULL)
            return TRUE;
        else
            return FALSE;
    }
    else
        return TRUE;
}


int set_context_value_arg(arg_t *arg, char *name)
{
    arg->val.type = ARG_CONTEXT_VAR;
    arg->val.name = mrp_strdup(name);

    if (arg->val.name != NULL)
        return TRUE;
    else
        return FALSE;
}


int set_context_set_arg(arg_t *arg, char *name, mrp_script_value_t *value)
{
    arg->set.type  = ARG_CONTEXT_SET;
    arg->set.name  = mrp_strdup(name);

    if (arg->set.name == NULL)
        return FALSE;

    arg->set.id    = -1;
    arg->set.value = *value;

    if (value->type == MRP_SCRIPT_TYPE_STRING) {
        arg->set.value.str = mrp_strdup(value->str);

        if (arg->set.value.str == NULL)
            return FALSE;
    }

    return TRUE;
}


void destroy_arguments(arg_t *args, int narg)
{
    arg_t *a;
    int    i;

    for (i = 0, a = args; i < narg; i++, a++) {
        if (a->type           == ARG_CONST_VALUE &&
            a->cst.value.type == MRP_SCRIPT_TYPE_STRING)
            mrp_free(a->cst.value.str);
        else if (a->type == ARG_CONTEXT_VAR)
            mrp_free(a->val.name);
    }

    mrp_free(args);
}


static void dump_arg(FILE *fp, arg_t *arg)
{
    mrp_script_value_t *val;
    char                vbuf[64];

    switch (arg->type) {
    case ARG_CONST_VALUE:
        val = &arg->cst.value;
        fprintf(fp, "%s", mrp_print_value(vbuf, sizeof(vbuf), val));
        break;

    case ARG_CONTEXT_VAR:
        fprintf(fp, "&%s", arg->val.name);
        break;

    case ARG_CONTEXT_SET:
        val = &arg->set.value;
        fprintf(fp, "&%s=%s", arg->set.name,
                mrp_print_value(vbuf, sizeof(vbuf), val));
        break;

    default:
        fprintf(fp, "<unknown/unhandled argument type>");
    }
}


void dump_call(FILE *fp, function_call_t *c)
{
    int   i;
    char *t;

    fprintf(fp, "    %s", c->name);

    fprintf(fp, "(");
    for (i = 0, t = ""; i < c->narg; i++, t = ", ") {
        fprintf(fp, "%s", t);
        dump_arg(fp, c->args + i);
    }
    fprintf(fp, ")\n");
}
