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

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/hashtbl.h>
#include <murphy/common/utils.h>

#include <murphy/core/scripting.h>

static MRP_LIST_HOOK(interpreters);                /* registered interpreters */
static const char *default_interpreter = "simple"; /* the default interpreter */


/*
 * a context variable
 */

typedef struct {
    const char        *name;             /* variable name */
    mrp_script_type_t  type;             /* type if declared */
    int                id;               /* variable id */
} context_var_t;


/*
 * a context frame (a set of context variable values)
 */

typedef struct context_value_s context_value_t;
struct context_value_s {
    int                 id;              /* variable id */
    mrp_script_value_t  value;           /* value for this variable */
    context_value_t    *next;            /* next value in this frame */
};

typedef struct context_frame_s context_frame_t;
struct context_frame_s {
    context_value_t *values;             /* hook to more value */
    context_frame_t *prev;               /* previous frame */
};


/*
 * table of context variables and context frames
 */

struct mrp_context_tbl_s {
    context_var_t   *variables;          /* known/declared context variables */
    int              nvariable;          /* number of variables */
    mrp_htbl_t      *names;              /* variable name to id mapping */
    context_frame_t *frame;              /* active frame */
};



int mrp_register_interpreter(mrp_interpreter_t *i)
{
    MRP_UNUSED(default_interpreter);

    mrp_list_init(&i->hook);
    mrp_list_append(&interpreters, &i->hook);

    return TRUE;
}


static void unregister_interpreter(mrp_interpreter_t *i)
{
    mrp_list_delete(&i->hook);
    mrp_list_init(&i->hook);
}


int mrp_unregister_interpreter(const char *name)
{
    mrp_interpreter_t *i;

    i = mrp_lookup_interpreter(name);

    if (i != NULL) {
        unregister_interpreter(i);

        return TRUE;
    }
    else
        return FALSE;

}


mrp_interpreter_t *mrp_lookup_interpreter(const char *name)
{
    mrp_list_hook_t   *p, *n;
    mrp_interpreter_t *i;

    mrp_list_foreach(&interpreters, p, n) {
        i = mrp_list_entry(p, typeof(*i), hook);
        if (!strcmp(i->name, name))
            return i;
    }

    return NULL;
}


mrp_scriptlet_t *mrp_create_script(const char *type, const char *source)
{
    mrp_interpreter_t *i;
    mrp_scriptlet_t   *s;

    s = NULL;
    i = mrp_lookup_interpreter(type);

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


void mrp_destroy_script(mrp_scriptlet_t *script)
{
    if (script != NULL) {
        if (script->interpreter && script->interpreter->cleanup)
            script->interpreter->cleanup(script);

        mrp_free(script->source);
        mrp_free(script);
    }
}


int mrp_compile_script(mrp_scriptlet_t *s)
{
    if (s != NULL)
        return s->interpreter->compile(s);
    else
        return 0;
}


int mrp_prepare_script(mrp_scriptlet_t *s)
{
    if (s != NULL && s->interpreter->prepare != NULL)
        return s->interpreter->prepare(s);
    else
        return 0;
}


int mrp_execute_script(mrp_scriptlet_t *s, mrp_context_tbl_t *ctbl)
{
    if (s != NULL)
        return s->interpreter->execute(s, ctbl);
    else
        return TRUE;
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


mrp_context_tbl_t *mrp_create_context_table(void)
{
    mrp_context_tbl_t *tbl;
    mrp_htbl_config_t  hcfg;

    tbl = mrp_allocz(sizeof(*tbl));

    if (tbl != NULL) {
        mrp_clear(&hcfg);
        hcfg.comp = mrp_string_comp;
        hcfg.hash = mrp_string_hash;
        hcfg.free = NULL;

        tbl->frame = NULL;
        tbl->names = mrp_htbl_create(&hcfg);

        if (tbl->names != NULL)
            return tbl;

        mrp_free(tbl);
    }

    return NULL;
}


void mrp_destroy_context_table(mrp_context_tbl_t *tbl)
{
    if (tbl != NULL) {
        while (mrp_pop_context_frame(tbl) == 0)
            ;

        mrp_htbl_destroy(tbl->names, FALSE);
        mrp_free(tbl);
    }
}


static context_var_t *lookup_context_var(mrp_context_tbl_t *tbl,
                                         const char *name)
{
    int id;

    id = (int)(ptrdiff_t)mrp_htbl_lookup(tbl->names, (void *)name);

    if (0 < id && id <= tbl->nvariable)
        return tbl->variables + id - 1;
    else
        return NULL;
}


int mrp_declare_context_variable(mrp_context_tbl_t *tbl, const char *name,
                                 mrp_script_type_t type)
{
    context_var_t *var;

    var = lookup_context_var(tbl, name);

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

        o = sizeof(*tbl->variables) * tbl->nvariable;
        n = sizeof(*tbl->variables) * (tbl->nvariable + 1);

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


int mrp_push_context_frame(mrp_context_tbl_t *tbl)
{
    context_frame_t *f;

    f = mrp_allocz(sizeof(*f));

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


int mrp_pop_context_frame(mrp_context_tbl_t *tbl)
{
    context_frame_t *f;
    context_value_t *v, *n;

    f = tbl->frame;

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


int get_context_id(mrp_context_tbl_t *tbl, const char *name)
{
    return (int)(ptrdiff_t)mrp_htbl_lookup(tbl->names, (void *)name);
}


int get_context_value(mrp_context_tbl_t *tbl, int id, mrp_script_value_t *value)
{
    context_frame_t *f;
    context_value_t *v;

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


int set_context_value(mrp_context_tbl_t *tbl, int id, mrp_script_value_t *value)
{
    context_frame_t *f;
    context_var_t   *var;
    context_value_t *val;
    char             vbuf[64];

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


int set_context_values(mrp_context_tbl_t *tbl, int *ids,
                       mrp_script_value_t *values, int nid)
{
    int i;

    for (i = 0; i < nid; i++) {
        if (set_context_value(tbl, ids[i], values + i) < 0)
            return -1;
    }

    return 0;
}


int mrp_get_context_id(mrp_context_tbl_t *tbl, const char *name)
{
    int id;

    id = get_context_id(tbl, name);

    if (id <= 0)
        id = mrp_declare_context_variable(tbl, name, MRP_SCRIPT_TYPE_UNKNOWN);

    return id;
}

int mrp_get_context_value(mrp_context_tbl_t *tbl, int id,
                          mrp_script_value_t *value)
{
    return get_context_value(tbl, id, value);

}

int mrp_set_context_value(mrp_context_tbl_t *tbl, int id,
                          mrp_script_value_t *value)
{
    return set_context_value(tbl, id, value);
}


int mrp_get_context_value_by_name(mrp_context_tbl_t *tbl, const char *name,
                                  mrp_script_value_t *value)
{
    return get_context_value(tbl, get_context_id(tbl, name), value);
}


int mrp_set_context_value_by_name(mrp_context_tbl_t *tbl, const char *name,
                                  mrp_script_value_t *value)
{
    int id;

    id = get_context_id(tbl, name);

    if (id <= 0)            /* auto-declare as an untyped variable */
        id = mrp_declare_context_variable(tbl, name, MRP_SCRIPT_TYPE_UNKNOWN);

    return set_context_value(tbl, id, value);
}
