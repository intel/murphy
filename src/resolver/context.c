#include <errno.h>

#include <murphy/common/mm.h>
#include <murphy/common/debug.h>
#include <murphy/common/list.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/utils.h>

#include "resolver-types.h"


int init_context_table(mrp_resolver_t *r)
{
    context_tbl_t     *tbl;
    mrp_htbl_config_t  hcfg;

    tbl = mrp_allocz(sizeof(*tbl));
    tbl->frame = NULL;

    if (tbl != NULL) {
        mrp_clear(&hcfg);
        hcfg.comp = mrp_string_comp;
        hcfg.hash = mrp_string_hash;
        hcfg.free = NULL;

        tbl->names = mrp_htbl_create(&hcfg);

        if (tbl->names != NULL) {
            r->ctbl = tbl;

            return 0;
        }

        mrp_free(tbl);
    }

    return -1;
}


void cleanup_context_table(mrp_resolver_t *r)
{
    if (r->ctbl != NULL) {
        mrp_htbl_destroy(r->ctbl->names, FALSE);
    }
}


static context_var_t *lookup_context_var(mrp_resolver_t *r, const char *name)
{
    context_tbl_t *tbl;
    int            id;

    tbl = r->ctbl;
    id  = (int)(ptrdiff_t)mrp_htbl_lookup(tbl->names, (void *)name);

    if (0 < id && id <= tbl->nvariable)
        return tbl->variables + id - 1;
    else
        return NULL;
}


int declare_context_variable(mrp_resolver_t *r, const char *name,
                             mrp_script_type_t type)
{
    context_tbl_t *tbl;
    context_var_t *var;

    var = lookup_context_var(r, name);

    if (var != NULL) {
        if (!var->type) {
            var->type = type;
            return var->id;
        }

        if (!type || var->type == type)
            return var->id;

        errno = EEXIST;
        return -1;
    }
    else {
        size_t o, n;

        tbl = r->ctbl;
        o   = sizeof(*tbl->variables) * tbl->nvariable;
        n   = sizeof(*tbl->variables) * (tbl->nvariable + 1);

        if (!mrp_reallocz(tbl->variables, o, n))
            return -1;

        var = tbl->variables + tbl->nvariable++;

        var->name = mrp_strdup(name);
        var->type = type;
        var->id   = tbl->nvariable;        /* this is a 1-based index... */

        if (var->name != NULL) {
            if (mrp_htbl_insert(tbl->names, (void *)var->name,
                                (void *)(ptrdiff_t)var->id))
                return var->id;
        }

        return -1;
    }
}


int push_context_frame(mrp_resolver_t *r)
{
    context_tbl_t   *tbl;
    context_frame_t *f;

    tbl = r->ctbl;
    f   = mrp_allocz(sizeof(*f));

    if (f != NULL) {
        f->values  = NULL;
        f->prev    = tbl->frame;
        tbl->frame = f;

        mrp_debug("pushed new context frame...");

        return 0;
    }
    else
        return -1;
}


int pop_context_frame(mrp_resolver_t *r)
{
    context_tbl_t   *tbl;
    context_frame_t *f;
    context_value_t *v, *n;

    tbl = r->ctbl;
    f   = tbl->frame;

    if (f != NULL) {
        for (v = f->values; v != NULL; v = n) {
            n = v->next;

            if (v->value.type == MRP_SCRIPT_TYPE_STRING)
                mrp_free(v->value.str);

            mrp_debug("popped variable <%d>", v->id);
            mrp_free(v);
        }

        tbl->frame = f->prev;
        mrp_free(f);

        mrp_debug("popped context frame");

        return 0;
    }
    else {
        errno = ENOENT;
        return -1;
    }
}


int get_context_id(mrp_resolver_t *r, const char *name)
{
    context_tbl_t *tbl;

    tbl = r->ctbl;

    return (int)(ptrdiff_t)mrp_htbl_lookup(tbl->names, (void *)name);
}


int get_context_value(mrp_resolver_t *r, int id, mrp_script_value_t *value)
{
    context_tbl_t   *tbl;
    context_frame_t *f;
    context_value_t *v;

    tbl = r->ctbl;

    if (0 < id && id <= tbl->nvariable) {
        for (f = tbl->frame; f != NULL; f = f->prev) {
            for (v = f->values; v != NULL; v = v->next) {
                if (v->id == id) {
                    *value = v->value;
                    return 0;
                }
            }
        }
    }

    value->type = MRP_SCRIPT_TYPE_INVALID;
    errno       = ENOENT;

    return -1;
}


int set_context_value(mrp_resolver_t *r, int id, mrp_script_value_t *value)
{
    context_tbl_t   *tbl;
    context_frame_t *f;
    context_var_t   *var;
    context_value_t *val;
    char             vbuf[64];

    tbl = r->ctbl;

    if (!(0 < id && id <= tbl->nvariable)) {
        errno = ENOENT;
        return -1;
    }

    var = tbl->variables + id - 1;
    if (var->type != MRP_SCRIPT_TYPE_INVALID && var->type != value->type) {
        errno = EINVAL;
        return -1;
    }

    f = tbl->frame;
    if (f != NULL) {
        val = mrp_allocz(sizeof(*val));

        if (val != NULL) {
            val->id    = id;
            val->value = *value;

            if (val->value.type != MRP_SCRIPT_TYPE_STRING ||
                ((val->value.str = mrp_strdup(val->value.str)) != NULL)) {
                val->next = f->values;
                f->values = val;

                mrp_debug("set &%s=%s", var->name,
                          mrp_print_value(vbuf, sizeof(vbuf), value));

                return 0;
            }
            else
                mrp_free(val);
        }
    }
    else
        errno = ENOSPC;

    return -1;
}


int set_context_values(mrp_resolver_t *r, int *ids, mrp_script_value_t *values,
                       int nid)
{
    int i;

    for (i = 0; i < nid; i++) {
        if (set_context_value(r, ids[i], values + i) < 0)
            return -1;
    }

    return 0;
}
