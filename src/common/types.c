/*
 * Copyright (c) 2012-2014, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/tlv.h>

#include <murphy/common/types.h>

#define __MRP_TYPE_CHECKS__


typedef struct mrp_enum_s     mrp_enum_t;
typedef struct mrp_type_s     mrp_type_t;
typedef struct mrp_member_s   mrp_member_t;


/*
 * a declared enum (is an alias to an int)
 */

typedef enum {
    DUMMY_ENUM = 0,
} enum_t;

struct mrp_enum_s {
    char            *name;               /* enum name */
    mrp_list_hook_t  hook;               /* hook to enums */
};


/*
 * a type member
 */

struct mrp_member_s {
    char            *name;               /* member name */
    mrp_type_t      *type;               /* member type */
    mrp_type_id_t    mod;                /* member type id */
    size_t           offs;               /* member offset from base */
    size_t           size;               /* member size */
    mrp_layout_t     layout;             /* member layout (if relevant) */
    int              idx;                /* member index within type */
    mrp_list_hook_t  hook;               /* to encoding order list */
    mrp_list_hook_t  init;               /* */
    union {                              /* type-specific extra info */
        struct {                         /* array info */
            mrp_array_type_t  type;      /*     dimension type */
            mrp_member_t     *size;      /*     size/guard member */
            char             *name;      /*     size/guard member name */
            mrp_value_t       guard;     /*     guard sentinel value */
            int               flexi : 1; /*     flexible guarded array */
        } array;
        struct {                         /* list info */
            mrp_member_t     *hook;      /*     hook member */
            char             *name;      /*     hook member name */
        } list;
        struct {                         /* union info */
            char             *name;      /*     key name */
            mrp_member_t     *key;       /*     key member */
            mrp_value_t       value;     /*     key value for this type */
            mrp_list_hook_t   hook;      /*     encoding order hook */
        } unio/*n*/;
    };

};


/*
 * a registered (or forward-declared) type
 */

struct mrp_type_s {
    char            *name;               /* type name */
    mrp_type_id_t    id;                 /* type id */
    size_t           size;               /* size of instances of this type */
    mrp_list_hook_t  hook;               /* to list of types */
    mrp_member_t    *members;            /* type members, if any */
    int              nmember;            /* number of members */
    mrp_member_t   **ordered;            /* members ordered by offset */
    mrp_list_hook_t  init;               /* members to initialize */
    mrp_list_hook_t  encode;             /* member encoding order */
    int              flexible : 1;       /* whether a flexible type */
    mrp_member_t    *key;                /* union type key */
    int              is_union : 1;       /* whether this is a union */
};


/*
 * header used to keep track of memory allocated during decoding
 */

typedef struct {
    mrp_list_hook_t hook;                /* to list of allocated chunks */
    char            data[0];             /* user-visible block of memory */
} chunk_t;


static mrp_type_t  **types;              /* defined types */
static int           ntype;              /* number of types */
static MRP_LIST_HOOK(incomplete);        /* incomplete types */
static MRP_LIST_HOOK(enums);             /* declared enum types */


static MRP_INIT void register_default_types(void)
{
#define UNKNOWN_TYPE(_type, _ctype)                             \
    static mrp_type_t _type##_entry = {                         \
        .name     = #_ctype,                                    \
        .size     = 0,                                          \
        .hook     = MRP_LIST_INIT(_type##_entry.hook),          \
        .members  = NULL,                                       \
        .nmember  = 0,                                          \
        .flexible = false,                                      \
        .is_union = false,                                      \
    }

#define DEFINE_TYPE(_type, _ctype)                              \
    static mrp_type_t _type##_entry = {                         \
        .name     = #_ctype,                                    \
        .size     = sizeof(_ctype),                             \
        .hook     = MRP_LIST_INIT(_type##_entry.hook),          \
        .members  = NULL,                                       \
        .nmember  = 0,                                          \
        .flexible = false,                                      \
        .is_union = false,                                      \
    }

#define REGISTER_TYPE(_type) do {                               \
        _type##_entry.id = ntype;                               \
        MRP_ASSERT(MRP_TYPE_##_type == ntype,                   \
                   "builtin type id mismatch");                 \
        types[ntype++] = &_type##_entry;                        \
    } while (0)

    types = mrp_allocz(MRP_NUM_BASIC_TYPES * sizeof(types[0]));

    MRP_ASSERT(ntype == 0, "called with busy type table");
    MRP_ASSERT(types != NULL, "failed to allocate type table");

    UNKNOWN_TYPE(UNKNOWN, <unknown>      );
    DEFINE_TYPE (INT8   , int8_t         );
    DEFINE_TYPE (UINT8  , uint8_t        );
    DEFINE_TYPE (INT16  , int16_t        );
    DEFINE_TYPE (UINT16 , uint16_t       );
    DEFINE_TYPE (INT32  , int32_t        );
    DEFINE_TYPE (UINT32 , uint32_t       );
    DEFINE_TYPE (INT64  , int64_t        );
    DEFINE_TYPE (UINT64 , uint64_t       );
    DEFINE_TYPE (SHORT  , short          );
    DEFINE_TYPE (USHORT , unsigned short );
    DEFINE_TYPE (ENUM   , enum_t         );
    DEFINE_TYPE (INT    , int            );
    DEFINE_TYPE (UINT   , unsigned int   );
    DEFINE_TYPE (LONG   , long           );
    DEFINE_TYPE (ULONG  , unsigned long  );
    DEFINE_TYPE (SSIZE  , ssize_t        );
    DEFINE_TYPE (SIZE   , size_t         );
    DEFINE_TYPE (FLOAT  , float          );
    DEFINE_TYPE (DOUBLE , double         );
    DEFINE_TYPE (BOOL   , bool           );
    DEFINE_TYPE (STRING , char *         );
    DEFINE_TYPE (HOOK   , mrp_list_hook_t);

    REGISTER_TYPE(UNKNOWN);
    REGISTER_TYPE(INT8   );
    REGISTER_TYPE(UINT8  );
    REGISTER_TYPE(INT16  );
    REGISTER_TYPE(UINT16 );
    REGISTER_TYPE(INT32  );
    REGISTER_TYPE(UINT32 );
    REGISTER_TYPE(INT64  );
    REGISTER_TYPE(UINT64 );
    REGISTER_TYPE(SHORT  );
    REGISTER_TYPE(USHORT );
    REGISTER_TYPE(ENUM   );
    REGISTER_TYPE(INT    );
    REGISTER_TYPE(UINT   );
    REGISTER_TYPE(LONG   );
    REGISTER_TYPE(ULONG  );
    REGISTER_TYPE(SSIZE  );
    REGISTER_TYPE(SIZE   );
    REGISTER_TYPE(FLOAT  );
    REGISTER_TYPE(DOUBLE );
    REGISTER_TYPE(BOOL   );
    REGISTER_TYPE(STRING );
    REGISTER_TYPE(HOOK   );

    MRP_ASSERT(ntype == MRP_TYPE_CUSTOM_MIN,
               "type id mismatch after builtin type registration");

#undef DEFINE_TYPE
#undef REGISTER_TYPE
}


static inline bool enum_type(const char *name)
{
    mrp_list_hook_t *p, *n;
    mrp_enum_t      *e;

    mrp_list_foreach(&enums, p, n) {
        e = mrp_list_entry(p, typeof(*e), hook);

        if (!strcmp(e->name, name))
            return true;
    }

    return false;
}


static inline mrp_type_t *type_by_name(const char *name)
{
    static mrp_type_t *e = NULL;
    int                i;

    for (i = 0; i < ntype; i++)
        if (!strcmp(types[i]->name, name))
            return types[i];

    if (enum_type(name)) {
        if (e == NULL)
            e = type_by_name("enum_t");

        return e;
    }

    return NULL;
}


static inline mrp_type_t *type_by_id(mrp_type_id_t id)
{
    if (0 <= id && id < ntype)
        return types[id];
    else
        return NULL;
}


static mrp_type_id_t id_by_name(const char *name)
{
    mrp_type_t *t;

    if ((t = type_by_name(name)) != NULL)
        return t->id;
    else
        return -1;
}


static mrp_member_def_t *member_def_by_name(mrp_type_def_t *t, const char *name)
{
    mrp_member_def_t *m;
    int               i;

    if (MRP_UNLIKELY(name == NULL))
        return NULL;

    for (i = 0, m = t->members; i < t->nmember; i++, m++)
        if (!strcmp(m->name, name))
            return m;

    return NULL;
}


static mrp_member_t *member_by_name(mrp_type_t *t, const char *name)
{
    mrp_member_t *m;
    int           i;

    for (i = 0, m = t->members; i < t->nmember; i++, m++)
        if (!strcmp(m->name, name))
            return m;

    return NULL;
}


mrp_type_id_t mrp_declare_type(const char *name)
{
    mrp_type_t *t;

    if ((t = type_by_name(name)) != NULL)
        return t->id;

    if ((t = mrp_allocz(sizeof(*t))) == NULL)
        goto fail;

    mrp_list_init(&t->hook);
    mrp_list_init(&t->init);
    mrp_list_init(&t->encode);

    if ((t->name = mrp_strdup(name)) == NULL)
        goto fail;

    if (mrp_reallocz(types, ntype, ntype + 1) == NULL)
        goto fail;

    t->id = ntype;
    mrp_list_append(&incomplete, &t->hook);

    types[ntype++] = t;

    return t->id;

 fail:
    if (t != NULL) {
        mrp_free(t->name);
        mrp_free(t);
    }

    return -1;
}


mrp_type_id_t mrp_declare_enum(const char *name)
{
    mrp_enum_t *e;

    if (enum_type(name))
        return MRP_TYPE_ENUM;

    if ((e = mrp_allocz(sizeof(*e))) == NULL ||
        (e->name = mrp_strdup(name)) == NULL) {
        mrp_free(e);
        return MRP_TYPE_INVALID;
    }

    mrp_list_init(&e->hook);
    mrp_list_append(&enums, &e->hook);

    return MRP_TYPE_ENUM;
}


static inline bool incomplete_type(mrp_type_t *t)
{
    if (t != NULL && (t->id >= MRP_TYPE_CUSTOM_MIN)) {
        if (t->nmember == 0)
            return true;

        if (!mrp_list_empty(&t->hook))   /* not on the incomplete list */
            return true;
    }

    return false;
}


static inline bool basic_type(mrp_type_id_t id)
{
    if (id < MRP_TYPE_CUSTOM_MIN)
        return true;
    else
        return false;
}


static inline bool integer_type(mrp_type_id_t id)
{
    if (MRP_TYPE_INT_START <= id && id <= MRP_TYPE_INT_END)
        return true;
    else
        return false;
}


static inline bool custom_type(mrp_type_id_t id)
{
    if (MRP_TYPE_CUSTOM_MIN <= id && id <= MRP_TYPE_CUSTOM_MAX)
        return true;
    else
        return false;
}


static inline bool union_type(mrp_type_t *t)
{
    if (t != NULL)
        return t->is_union;
    else
        return false;
}


static inline bool struct_type(mrp_type_t *t)
{
    if (t != NULL && custom_type(t->id))
        return !t->is_union;
    else
        return false;
}


static int copy_basic_def(mrp_type_t *type, mrp_member_t *d,
                          mrp_type_def_t *def, mrp_member_def_t *s)
{
    MRP_UNUSED(def);

    if ((d->name = mrp_strdup(s->name)) == NULL)
        return -1;

    d->type = type_by_name(s->type);
    d->mod    = s->mod;
    d->offs   = s->offs;
    d->size   = s->size;
    d->layout = s->layout;

    type->nmember++;

    mrp_list_init(&d->unio.hook);        /* for implicit union type keys */

    return 0;
}


static int copy_array_def(mrp_type_t *type, mrp_member_t *d,
                          mrp_type_def_t *def, mrp_member_def_t *s)
{
    mrp_member_def_t *g;
    mrp_member_t     *m;
    int               layout;

    d->type = type_by_name(s->type);
    layout  = s->layout;

    switch (s->array.type) {
    case MRP_ARRAY_SIZED:
        if (s->array.size == NULL) {
            mrp_log_error("%s.%s: missing array size member.",
                          def->name, s->name);
            goto invalid;
        }

        if ((m = member_by_name(type, s->array.size)) != NULL) {
            d->array.size = m;
            d->array.type = s->array.type;
        }
        else {
            if ((g = member_def_by_name(def, s->array.size)) == NULL) {
                mrp_log_error("%s.%s: undefined array size member %s.",
                              def->name, s->name, s->array.size);
                goto invalid;
            }

            if (copy_basic_def(type, d, def, g) < 0) /* will overwrite d */
                goto fail;

            d = type->members + type->nmember;

            if ((m = member_by_name(type, s->array.size)) == NULL) {
                mrp_log_error("%s.%s: undefined (?) array size member %s.",
                              type->name, m->name, s->array.size);
                goto invalid;
            }

            d->array.size = m;
            d->array.type = s->array.type;

        }
        break;

    case MRP_ARRAY_GUARD:
        if (s->array.size == NULL) {
            mrp_log_error("%s.%s: missing array guard member.",
                          def->name, s->name);
            goto invalid;
        }

        d->array.type  = s->array.type;
        d->array.guard = s->array.guard;

        if (incomplete_type(d->type)) {
            if ((d->array.name = mrp_strdup(s->array.size)) == NULL)
                goto fail;
        }
        else {
            if (basic_type(d->type->id))
                break;

            if ((m = member_by_name(d->type, s->array.size)) == NULL) {
                mrp_log_error("%s.%s: undefined array guard member %s.",
                              def->name, s->name, s->array.size);
                goto invalid;
            }

            d->array.size = m;
        }
        break;

    default:
        d->array.type = s->array.type;
        break;
    }

    if ((d->name = mrp_strdup(s->name)) == NULL)
        goto fail;

    d->type   = type_by_name(s->type);
    d->mod    = s->mod;
    d->offs   = s->offs;
    d->size   = s->size;
    d->layout = layout;

    type->nmember++;

    if (incomplete_type(d->type) && mrp_list_empty(&type->hook))
        mrp_list_append(&incomplete, &type->hook);

    return 0;

 invalid:
    errno = EINVAL;
 fail:
    return -1;
}


static int copy_list_def(mrp_type_t *type, mrp_member_t *d,
                         mrp_type_def_t *def, mrp_member_def_t *s)
{
    mrp_member_t *h;

    if (s->list.hook == NULL) {
        mrp_log_error("%s.%s: missing list hook name.", def->name, s->name);
        goto invalid;
    }

    d->type = type_by_name(s->type);

    if (incomplete_type(d->type)) {
        if ((d->list.name = mrp_strdup(s->list.hook)) == NULL)
            goto fail;
    }
    else {
        if ((h = member_by_name(d->type, s->list.hook)) == NULL) {
            mrp_log_error("%s.%s: type %s has no list hook %s.",
                          def->name, s->name, d->type->name, s->list.hook);
            goto invalid;
        }

        d->list.hook = h;
    }

    if ((d->name = mrp_strdup(s->name)) == NULL)
        goto fail;

    d->mod    = s->mod;
    d->offs   = s->offs;
    d->size   = s->size;
    d->layout = s->layout;

    type->nmember++;

    if (incomplete_type(d->type) && mrp_list_empty(&type->hook))
        mrp_list_append(&incomplete, &type->hook);

    return 0;

 invalid:
    errno = EINVAL;
 fail:
    return -1;
}


static int copy_union_key_def(mrp_type_t *type, mrp_member_t *d,
                              mrp_type_def_t *def, mrp_member_def_t *s)
{
    if (copy_basic_def(type, d, def, s) < 0)
        return -1;

    d->mod = MRP_TYPE_UNION_KEY;
    /* d->unio.hook already initialized by copy_basic_def */

    return 0;
}


static int copy_union_def(mrp_type_t *type, mrp_member_t *d,
                          mrp_type_def_t *def, mrp_member_def_t *s)
{
    int complete;

    MRP_UNUSED(def);

    d->type = type_by_name(s->type);

    if (incomplete_type(d->type))
        complete = false;
    else
        complete = true;

    mrp_list_init(&d->unio.hook);

    if ((d->unio.name = mrp_strdup(s->unio.key)) == NULL)
        goto invalid;

    d->unio.value = s->unio.value;

    if ((d->name = mrp_strdup(s->name)) == NULL)
        goto invalid;

    d->mod    = s->mod;
    d->offs   = s->offs;
    d->size   = s->size;
    d->layout = s->layout;

    type->nmember++;

    if ((incomplete_type(d->type) || !complete) && mrp_list_empty(&type->hook))
        mrp_list_append(&incomplete, &type->hook);

    return 0;

 invalid:
    errno = EINVAL;
    return -1;
}


static int copy_custom_def(mrp_type_t *type, mrp_member_t *d,
                           mrp_type_def_t *def, mrp_member_def_t *s)
{
    MRP_UNUSED(def);

    if ((d->name = mrp_strdup(s->name)) == NULL)
        return -1;

    d->type = type_by_name(s->type);
    d->mod    = s->mod;
    d->offs   = s->offs;
    d->size   = s->size;
    d->layout = s->layout;

    type->nmember++;

    if (incomplete_type(d->type) && mrp_list_empty(&type->hook))
        mrp_list_append(&incomplete, &type->hook);

    return 0;
}


static bool check_flexible(mrp_type_t *type, mrp_member_t *f)
{
    mrp_member_t *m;
    int           i;

    if (f->offs + f->type->size != type->size) {
        mrp_log_error("%s.%s: flexible member not at the end.",
                      type->name, f->name);
        return false;
    }

    for (i = 0, m = type->members; i < type->nmember; i++, m++) {
        if (m->offs > f->offs) {
            mrp_log_error("%s.%s: can't have members after a flexible member.",
                          type->name, m->name);
            return false;
        }
    }

    return true;
}


static bool check_basic(mrp_type_t *type, mrp_member_t *m)
{
    switch (m->type->id) {
    case MRP_TYPE_STRING:
        if (m->size == 0) {
            if (m->layout != MRP_LAYOUT_INLINED) {
                mrp_log_warning("%s.%s: forcing inlined string layout.",
                                type->name, m->name);
                m->layout = MRP_LAYOUT_INLINED;
            }
            if (!type->flexible) {
                mrp_log_warning("%s.%s: forcing flexible member.",
                                type->name, m->name);
                type->flexible = true;
            }

            if (!check_flexible(type, m))
                return false;
        }
        break;

    case MRP_TYPE_HOOK:
        mrp_list_append(&type->init, &m->init);
        if (m->layout != MRP_LAYOUT_INLINED) {
            mrp_log_error("%s.%s: list hooks must be inlined.",
                          type->name, m->name);
            return false;
        }
        break;

    default:
        if (m->layout != MRP_LAYOUT_INLINED) {
            mrp_log_error("%s.%s: only inlined layout supported for type %s.",
                          type->name, m->name, m->type->name);
            return false;
        }
    }

#if 0
    if (m->layout == MRP_LAYOUT_INDIRECT) {
        mrp_log_error("%s.%s: indirect layout is not implemented.",
                      type->name, m->name);
        return false;
    }

    if (m->type->id == MRP_TYPE_STRING) {
        if (m->size == 0) {
            if (m->layout != MRP_LAYOUT_INLINED) {
                mrp_log_warning("%s.%s: forcing inlined string layout.",
                                type->name, m->name);
                m->layout = MRP_LAYOUT_INLINED;
            }
            if (!type->flexible) {
                mrp_log_warning("%s.%s: forcing flexible member.",
                                type->name, m->name);
                type->flexible = true;
            }

            if (!check_flexible(type, m))
                return false;
        }
    }

    if (m->type->id == MRP_TYPE_HOOK) {
        mrp_list_append(&type->init, &m->init);
    }
#endif

    return true;
}


static bool check_array(mrp_type_t *type, mrp_member_t *m)
{
    switch (m->array.type) {
    case MRP_ARRAY_SIZED:
        if (!integer_type(m->array.size->type->id)) {
            mrp_log_error("%s.%s: non-integer array size member %s.",
                          type->name, m->name, m->array.size->name);
            return false;
        }

        if (m->size == 0) {
            if (!m->array.flexi) {
                mrp_log_warning("%s.%s: forcing to be flexible member.",
                                type->name, m->name);
                m->array.flexi = true;
                type->flexible = true;
            }

            if (m->layout != MRP_LAYOUT_INLINED) {
                mrp_log_warning("%s.%s: forcing inlined (flexible) layout.",
                                type->name, m->name);
                m->layout = MRP_LAYOUT_INLINED;
            }
        }
        break;

    case MRP_ARRAY_GUARD:
        if (m->size == 0) {
            if (!m->array.flexi) {
                mrp_log_warning("%s.%s: forcing to be flexible member.",
                                type->name, m->name);
                m->array.flexi = true;
                type->flexible = true;
            }

            if (m->layout != MRP_LAYOUT_INLINED) {
                mrp_log_warning("%s.%s: forcing inlined (flexible) layout.",
                                type->name, m->name);
                m->layout = MRP_LAYOUT_INLINED;
            }
        }

        if (!basic_type(m->type->id)) {
            if (!basic_type(m->array.size->type->id)) {
                mrp_log_error("%s.%s: guard (%s) is not of basic type.",
                              type->name, m->name, m->array.size->name);
                return false;
            }
        }
        break;

    default:
        if (m->array.type <= 0) {
            mrp_log_error("%s.%s: invalid size %d for fixed size array member.",
                          type->name, m->name, m->array.type);
            return false;
        }
        break;
    }

    return true;
}


static bool check_list(mrp_type_t *type, mrp_member_t *m)
{
    if (m->list.hook->type->id != MRP_TYPE_HOOK) {
        mrp_log_error("%s.%s: %s.%s is not a hook (%s).",
                      type->name, m->name, m->type->name, m->list.hook->name,
                      m->list.hook->type->name);
        return false;
    }

    return true;
}


static inline bool union_member_has_key(mrp_member_t *m,
                                        mrp_member_t *key, void *keyd)
{
    mrp_value_t *kv = keyd;
    mrp_value_t *mv = &m->unio.value;

    if (key->type->id == MRP_TYPE_STRING)
        return ((kv->strp == NULL && mv->strp == NULL) ||
                (kv->strp != NULL && mv->strp != NULL &&
                 !strcmp(kv->strp, mv->strp)));
    else
        return !memcmp(kv, mv, key->type->size);
}


static mrp_member_t *union_member_by_key(mrp_member_t *key, void *keyd)
{
    mrp_list_hook_t *p, *n;
    mrp_member_t *m;

    mrp_list_foreach(&key->unio.hook, p, n) {
        m = mrp_list_entry(p, typeof(*m), unio.hook);

        if (union_member_has_key(m, key, keyd))
            return m;
    }

    return NULL;
}


static bool check_union_key(mrp_type_t *type, mrp_member_t *m)
{
    if (!basic_type(m->type->id)) {
        mrp_log_error("%s.%s: union type key is not of basic type (%s).",
                      type->name, m->name, m->type->name);
        return false;
    }
    else
        return true;
}


static bool check_union(mrp_type_t *type, mrp_member_t *m)
{
    mrp_member_t *key;

    key = m->unio.key = member_by_name(type, m->unio.name);

    if (key == NULL) {
        mrp_log_error("%s.%s: union type key is an undefined member (%s).",
                      type->name, m->name, m->unio.name);
        return false;
    }

    if (!basic_type(key->type->id)) {
        mrp_log_error("%s.%s: given type key %s is not of basic type (%s).",
                      type->name, m->name, m->unio.name, key->type->name);
        return false;
    }

    if (key->type->id == MRP_TYPE_STRING)
        m->unio.value.strp = mrp_strdup(m->unio.value.strp);

    if (key->unio.hook.next == NULL)
        mrp_list_init(&key->unio.hook);

    mrp_list_append(&key->unio.hook, &m->unio.hook);

    return true;
}


static int offscmp(const void *ptr1, const void *ptr2)
{
    const mrp_member_t *m1 = *(mrp_member_t **)ptr1;
    const mrp_member_t *m2 = *(mrp_member_t **)ptr2;

    return m1->offs - m2->offs;
}


static int sort_members(mrp_type_t *type)
{
    mrp_member_t  *m;
    int            i;

    if (type->nmember < 1)
        return 0;

    if ((type->ordered = mrp_allocz(type->nmember * sizeof(void *))) == NULL)
        return -1;

    for (i = 0, m = type->members; i < type->nmember; i++, m++)
        type->ordered[i] = m;

    qsort(type->ordered, type->nmember, sizeof(void *), offscmp);

    for (i = 0, m = type->members; i < type->nmember; i++, m++)
        type->ordered[i]->idx = i;

    return 0;
}


static bool check_type(mrp_type_t *type)
{
    mrp_member_t  *m;
    size_t         tot_offs, max_size;
    int            i;

    tot_offs = 0;
    max_size = 0;
    for (i = 0, m = type->members; i < type->nmember; i++, m++) {
        tot_offs += m->offs;
        if (m->size > max_size)
            max_size = m->size;

        switch (m->mod) {
        case MRP_TYPE_SCALAR:
            if (basic_type(m->type->id)) {
                if (!check_basic(type, m))
                    return false;
            }
            else {
                if (!check_type(m->type))
                    return false;
                if (m->type->flexible) {
                    if (m->offs + m->type->size != type->size) {
                        mrp_log_error("%s.%s: flexible type not at the end.",
                                      type->name, m->name);
                        return false;
                    }
                    else
                        type->flexible = true;
                }
            }
            mrp_list_append(&type->encode, &m->hook);
            break;

        case MRP_TYPE_ARRAY:
            if (!check_array(type, m))
                return false;

            if (m->array.flexi) {
                if (m->offs != type->size) {
                    mrp_log_error("%s.%s: flexible array not at the end.",
                                 type->name, m->name);
                    return false;
                }
                else
                    type->flexible = true;
            }
            mrp_list_append(&type->encode, &m->hook);
            break;

        case MRP_TYPE_LIST:
            if (!check_list(type, m))
                return false;
            mrp_list_append(&type->encode, &m->hook);
            break;

        case MRP_TYPE_UNION_KEY:
            if (!check_union_key(type, m))
                return false;
            mrp_list_prepend(&type->encode, &m->hook);
            type->key = m;
            break;

        case MRP_TYPE_UNION:
            if (!check_union(type, m))
                return false;
            break;

        default:
            mrp_log_error("%s.%s: member of invalid type (0x%x|0x%x).",
                          type->name, m->name, m->mod, m->type->id);
            return false;
        }
    }

    if (max_size > type->size) {
        mrp_log_error("%s: max. members size exceeds type size (%zd > %zd).",
                      type->name, max_size, type->size);
        return false;
    }

    if (tot_offs == 0 && type->nmember > 1)
        type->is_union = true;

    if (type->nmember > 0) {
        if (sort_members(type) < 0)
            return -1;

        m = type->ordered[type->nmember - 1];

        if (m->layout == MRP_LAYOUT_INLINED && m->type->flexible) {
            type->flexible = true;
            if (!check_flexible(type, m))
                return false;
        }
    }

    return true;
}


static int resolve_type(mrp_type_t *type)
{
    mrp_member_t *m;
    int           i;

    if (type->nmember == 0)
        return false;

    for (i = 0, m = type->members; i < type->nmember; i++, m++) {
        switch (m->mod) {
        case MRP_TYPE_SCALAR:
            if (custom_type(m->type->id)) {
                switch (resolve_type(m->type)) {
                case -1: return -1;
                case  0: return  0;
                case  1: break;
                }
            }
            break;

        case MRP_TYPE_ARRAY:
            if (m->array.type != MRP_ARRAY_GUARD)
                break;

            if (m->array.size == NULL) {
                if (m->array.name == NULL)
                    break;

                if (incomplete_type(m->type))
                    return false;
                else
                    m->array.size = member_by_name(m->type, m->array.name);
            }

            if (m->array.size == NULL) {
                mrp_log_error("%s.%s: undefined guard member %s.",
                              type->name, m->name, m->array.name);
                return -1;
            }

            mrp_free(m->array.name);
            m->array.name = NULL;

            if (m->size == 0) {
                if (!check_flexible(type, m))
                    return -1;

                mrp_debug("%s.%s: flexible array (type size = %zd)",
                          type->name, m->name, m->type->size);

                type->flexible = m->array.flexi = true;

                if (m->layout != MRP_LAYOUT_INLINED) {
                    mrp_log_warning("%s.%s: forcing inlined layout for flexible "
                                    "array member.", type->name, m->name);
                    m->layout = MRP_LAYOUT_INLINED;
                }
            }
            break;

        case MRP_TYPE_LIST:
            if (m->list.name == NULL)
                break;

            m->list.hook = member_by_name(m->type, m->list.name);

            if (m->list.hook == NULL) {
                mrp_log_error("%s.%s: type %s has no list hook %s.",
                              type->name, m->name, m->type->name, m->list.name);
                return -1;
            }

            mrp_free(m->list.name);
            m->list.name = NULL;
            break;

        case MRP_TYPE_UNION_KEY:
            break;

        case MRP_TYPE_UNION:
            if (incomplete_type(m->type))
                return false;
            break;

        default:
            mrp_log_error("%s.%s: inalid modifier 0x%x.",
                          type->name, m->name, m->mod);
            return -1;
        }
    }

    return true;
}


static int resolve_incomplete_types(void)
{
    mrp_type_t      *type;
    mrp_list_hook_t *p, *n;
    bool             change;

 recheck:
    change = false;
    mrp_list_foreach(&incomplete, p, n) {
        type = mrp_list_entry(p, typeof(*type), hook);

        switch (resolve_type(type)) {
        case -1: goto invalid;
        case  0: continue;
        case  1: break;
        }

        mrp_debug("type %s is now fully defined", type->name);

        if (!check_type(type))
            goto invalid;

        mrp_list_delete(&type->hook);
        change = true;
    }

    if (mrp_list_empty(&incomplete)) {
        mrp_debug("all declared types are now fully defined");
        return 1;
    }

    if (change)
        goto recheck;

    mrp_debug("there are still incomplete types");
    return 0;

 invalid:
    errno = EINVAL;
    return -1;
}


mrp_type_id_t mrp_register_type(mrp_type_def_t *def)
{
    mrp_type_t       *type, *t;
    mrp_member_def_t *s;
    mrp_member_t     *d, *m;
    int               i;

    if (def->name == NULL) {
        mrp_log_error("Can't register type (%p) with no name.", def);
        goto invalid;
    }

    if (def->nmember <= 0) {
        mrp_log_error("%s: user-defined types must have members.", def->name);
        goto invalid;
    }

    if ((type = type_by_name(def->name)) != NULL) {
        if (basic_type(type->id)) {
            mrp_log_error("%s: basic type can't be redefined.", def->name);
            goto invalid;
        }

        if (type->nmember > 0) {
            mrp_log_error("%s: trying to redefine existing type.", def->name);
            goto existing;
        }
    }
    else {
        if ((type = type_by_id(mrp_declare_type(def->name))) == NULL)
            goto fail;
    }

    type->size    = def->size;
    type->members = mrp_allocz(def->nmember * sizeof(type->members[0]));

    if (type->members == NULL)
        goto fail;

    s = def->members;
    d = type->members;

    mrp_list_delete(&type->hook);

    for (i = 0; i < def->nmember; i++) {
        if (s->name == NULL) {
            mrp_log_error("%s: member #%d has no name.", def->name, i);
            goto invalid;
        }

        if (s->offs + s->size > type->size) {
            mrp_log_error("%s.%s: member ends beyond containing type.",
                          def->name, s->name);
            goto invalid;
        }

        if (s->chkerr) {
            mrp_log_error("%s.%s: type check of declared type failed.",
                          def->name, s->name);
            goto invalid;
        }

        if ((t = type_by_name(s->type)) == NULL) {
            mrp_log_error("%s.%s: unknown member type '%s'.",
                          def->name, s->name, s->type);
            goto invalid;
        }

        if ((m = member_by_name(type, s->name)) != NULL) {
            if (m->type == t && m->mod == s->mod)
                goto skip;
            else {
                mrp_log_error("%s.%s: member defined multiple times.",
                              def->name, s->name);
                goto invalid;
            }
        }

        if (s->mod) {
            switch (s->mod) {
            case MRP_TYPE_ARRAY:
                if (copy_array_def(type, d, def, s) < 0)
                    goto fail;
                break;
            case MRP_TYPE_LIST:
                if (copy_list_def(type, d, def, s) < 0)
                    goto fail;
                break;
            case MRP_TYPE_UNION_KEY:
                if (copy_union_key_def(type, d, def, s) < 0)
                    goto fail;
                break;
            case MRP_TYPE_UNION:
                if (copy_union_def(type, d, def, s) < 0)
                    goto fail;
                break;
            default:
                mrp_log_error("%s.%s: invalid type modifier 0x%x.",
                              def->name, s->name, s->mod);
                goto fail;
            }
        }
        else {
            if (basic_type(t->id)) {
                if (copy_basic_def(type, d, def, s) < 0)
                    goto fail;
            }
            else if (custom_type(t->id)) {
                if (copy_custom_def(type, d, def, s) < 0)
                    goto fail;
            }
            else {
                mrp_log_error("%s.%s: unhandled type 0x%x.",
                              def->name, s->name, t->id);
                goto invalid;
            }
        }

        d = type->members + type->nmember;
    skip:
        s++;
    }

    if (!incomplete_type(type)) {
        mrp_debug("type '%s' is fully defined", def->name);
        if (!check_type(type))
            goto fail;
        if (resolve_incomplete_types() < 0)
            goto fail;
    }
    else {
        mrp_debug("type '%s' is still incomplete", type->name);
    }

    return type->id;

 invalid:
    errno = EINVAL;
 fail:
    return -1;

 existing:
    errno = EEXIST;
    return -1;
}


static char *print_value(mrp_value_t *v, mrp_type_id_t id)
{
    static char buf[1024];

#define P(fmt, v) snprintf(buf, sizeof(buf), fmt, v);

    switch (id) {
    case MRP_TYPE_INT8:    P("%d"  , v->s8  ); break;
    case MRP_TYPE_UINT8:   P("%u"  , v->u8  ); break;
    case MRP_TYPE_INT16:   P("%d"  , v->s16 ); break;
    case MRP_TYPE_UINT16:  P("%u"  , v->u16 ); break;
    case MRP_TYPE_INT32:   P("%d"  , v->s32 ); break;
    case MRP_TYPE_UINT32:  P("%u"  , v->u32 ); break;
    case MRP_TYPE_INT64:   P("%ld" , v->s64 ); break;
    case MRP_TYPE_UINT64:  P("%lu" , v->u64 ); break;
    case MRP_TYPE_SHORT:   P("%d"  , v->ssi ); break;
    case MRP_TYPE_USHORT:  P("%u"  , v->usi ); break;
    case MRP_TYPE_ENUM:    /* fallthrough, alias for int */
    case MRP_TYPE_INT:     P("%d"  , v->si  ); break;
    case MRP_TYPE_UINT:    P("%u"  , v->ui  ); break;
    case MRP_TYPE_LONG:    P("%ld" , v->sli ); break;
    case MRP_TYPE_ULONG:   P("%lu" , v->uli ); break;
    case MRP_TYPE_SSIZE:   P("%zd" , v->ssz ); break;
    case MRP_TYPE_SIZE:    P("%zu" , v->usz ); break;
    case MRP_TYPE_FLOAT:   P("%f"  , v->flt ); break;
    case MRP_TYPE_DOUBLE:  P("%f"  , v->dbl ); break;
    case MRP_TYPE_STRING:  P("'%s'", v->strp); break;
    default:
        P("<value of type 0x%x>", id);
        break;
    }

#undef P

    return buf;
}


ssize_t mrp_print_type_def(char *buf, size_t size, mrp_type_id_t id)
{
    mrp_type_t      *type;
    mrp_member_t    *m, *u;
    mrp_list_hook_t *ep, *en, *up, *un;
    int              i;
    const char      *lo, *flx, *mod;
    char            *p;
    ssize_t          n, l;

    if ((type = type_by_id(id)) == NULL)
        return snprintf(buf, size, "<0x%x: undeclared type>\n", id);

    if (basic_type(type->id))
        return snprintf(buf, size, "<0x%x: %s (%zd bytes)>\n", type->id,
                        type->name, type->size);

    p = buf;
    l = (ssize_t)size;

    n  = snprintf(p, l, "<0x%x: %s%stype %s (%zd bytes)>\n", type->id,
                  type->flexible ? "flexible " : "",
                  type->is_union ? "union " : "", type->name, type->size);
    p += n;
    l -= n;

    for (i = 0, m = type->members; i < type->nmember; i++, m++) {
        flx = "";
        switch (m->mod) {
        case MRP_TYPE_ARRAY:
            switch (m->array.type) {
            case MRP_ARRAY_SIZED: mod = "sized array of ";   break;
            case MRP_ARRAY_GUARD: mod = "guarded array of "; break;
            case MRP_ARRAY_FIXED: mod = "fixed array of ";   break;
            default:              mod = "unknown array of";  break;
            }
            flx = m->array.flexi ? "flexible ":"";
            break;
        case MRP_TYPE_LIST:
            mod = "list of ";
            break;
        case MRP_TYPE_UNION_KEY:
            mod = "union key ";
            break;
        case MRP_TYPE_UNION:
            mod = "union member ";
            break;
        default:
            if (basic_type(m->type->id))
                mod = mrp_list_empty(&m->unio.hook) ? "" : "implicit union key ";
            else {
                mod = "";
                flx = m->type->flexible ? "(flexible) " : "";
            }
            break;
        }
        switch (m->layout) {
        case MRP_LAYOUT_INDIRECT: lo = "indirect ";  break;
        case MRP_LAYOUT_INLINED:  lo = "inlined " ;  break;
        default:                  lo = "<layout?> "; break;
        }

        n  = snprintf(p, l>0?l:0, "    <#%d %s%s%s%s (%zd bytes @ %zd)> %s\n",
                      m->idx, lo, flx, mod,
                      m->type->name, m->size, m->offs, m->name);
        p += n;
        l -= n;
    }

    n  = snprintf(p, l>0?l:0, "    encoding order:\n");
    p += n;
    l -= n;

    mrp_list_foreach(&type->encode, ep, en) {
        m  = mrp_list_entry(ep, typeof(*m), hook);
        n  = snprintf(p, l>0?l:0, "        %s\n", m->name);
        p += n;
        l -= n;

        if ((m->mod == MRP_TYPE_SCALAR || m->mod == MRP_TYPE_UNION_KEY) &&
            basic_type(m->type->id)) {
            mrp_list_foreach(&m->unio.hook, up, un) {
                u  = mrp_list_entry(up, typeof(*u), unio.hook);
                n  = snprintf(p, l>0?l:0, "            %s (%s %s)\n",
                              u->name, u->unio.name,
                              print_value(&u->unio.value,
                                          u->unio.key->type->id));
                p += n;
                l -= n;
            }
        }
    }

    return (ssize_t)size - l;
}


ssize_t mrp_print_type_defs(char *buf, size_t size)
{
    char    *p;
    ssize_t  n, l;
    int      i;

    p = buf;
    l = (ssize_t)size;

    for (i = 0; i < ntype; i++) {
        n  = mrp_print_type_def(p, l>0?l:0, types[i]->id);
        p += n;
        l -= n;
    }

    return (ssize_t)size - l;
}


static mrp_type_id_t mapped_id(mrp_type_map_t *map, mrp_type_id_t id)
{
    mrp_type_map_t *m;

    if (map == NULL)
        return id;

    if (id == MRP_TYPE_INVALID)
        return id;

    if (basic_type(id))
        return MRP_TYPE_CUSTOM_MAX + id;

    for (m = map; m->native; m++)
        if (m->native == id)
            return m->mapped;

    return MRP_TYPE_INVALID;
}


static mrp_type_id_t native_id(mrp_type_map_t *map, mrp_type_id_t id)
{
    mrp_type_map_t *m;

    if (map == NULL)
        return id;

    if (id == MRP_TYPE_INVALID)
        return id;

    if (id > MRP_TYPE_CUSTOM_MAX)
        return id - MRP_TYPE_CUSTOM_MAX;

    for (m = map; m->native; m++)
        if (m->mapped == id)
            return m->native;

    return MRP_TYPE_INVALID;
}


/*
 * TLV encoding/decoding tags
 */

typedef enum {
    TAG_NONE = MRP_TLV_UNTAGGED,         /* untagged entry */
    TAG_ARRAY_START,                     /* start+count+type of an array */
    TAG_ARRAY_ITEM,                      /* index+array item */
    TAG_ARRAY_END,                       /* end of an array */
    TAG_LIST_START,                      /* start+count+type of a list */
    TAG_LIST_ITEM,                       /* index+list item */
    TAG_LIST_END,                        /* end of a list */
    TAG_STRUCT_START,                    /* start+type of a struct */
    TAG_STRUCT_END,                      /* end of a struct */
    TAG_UNION_START,                     /* start of a union */
    TAG_UNION_END,                       /* end of a union */
    TAG_UNION_MEMBER,                    /* a union struct member */
    TAG_FLEXI,                           /* flexible type+count */
    TAG_COUNT,                           /* item/member counter */
    TAG_MEMBER,                          /* member of a struct */
} type_tag_t;

static int encode_list(mrp_tlv_t *tlv, void *listp, mrp_type_t *type,
                       size_t hook_offs, mrp_type_map_t *map);
static int encode_union(mrp_tlv_t *tlv, mrp_type_t *type, void *data,
                        mrp_type_map_t *map);
static int encode_struct(mrp_tlv_t *tlv, mrp_type_t *type, void *data,
                         mrp_type_map_t *map);
static int encode_type(mrp_tlv_t *tlv, mrp_type_t *type, void *data,
                       mrp_type_map_t *map);


static inline void *member_address(void *base, mrp_member_t *m)
{
    if (m->layout == MRP_LAYOUT_INLINED)
        return base + m->offs;
    else
        return *(void **)(base + m->offs);
}


static inline int encode_basic(mrp_tlv_t *tlv, mrp_type_t *type, void *data)
{
    mrp_value_t *v = (mrp_value_t *)data;

    switch (type->id) {
    case MRP_TYPE_INT8:   return mrp_tlv_push_int8  (tlv, TAG_NONE, v->s8 );
    case MRP_TYPE_UINT8:  return mrp_tlv_push_uint8 (tlv, TAG_NONE, v->u8 );
    case MRP_TYPE_INT16:  return mrp_tlv_push_int16 (tlv, TAG_NONE, v->s16);
    case MRP_TYPE_UINT16: return mrp_tlv_push_uint16(tlv, TAG_NONE, v->u16);
    case MRP_TYPE_INT32:  return mrp_tlv_push_int32 (tlv, TAG_NONE, v->s32);
    case MRP_TYPE_UINT32: return mrp_tlv_push_uint32(tlv, TAG_NONE, v->u32);
    case MRP_TYPE_INT64:  return mrp_tlv_push_int64 (tlv, TAG_NONE, v->s64);
    case MRP_TYPE_UINT64: return mrp_tlv_push_uint64(tlv, TAG_NONE, v->u64);
    case MRP_TYPE_FLOAT:  return mrp_tlv_push_float (tlv, TAG_NONE, v->flt);
    case MRP_TYPE_DOUBLE: return mrp_tlv_push_double(tlv, TAG_NONE, v->dbl);
    case MRP_TYPE_BOOL:   return mrp_tlv_push_bool  (tlv, TAG_NONE, v->bln);
    case MRP_TYPE_SHORT:  return mrp_tlv_push_short (tlv, TAG_NONE, v->ssi);
    case MRP_TYPE_USHORT: return mrp_tlv_push_ushort(tlv, TAG_NONE, v->usi);
    case MRP_TYPE_ENUM:   /* fallthrough, alias for MRP_TYPE_INT */
    case MRP_TYPE_INT:    return mrp_tlv_push_int   (tlv, TAG_NONE, v->si );
    case MRP_TYPE_UINT:   return mrp_tlv_push_uint  (tlv, TAG_NONE, v->ui );
    case MRP_TYPE_LONG:   return mrp_tlv_push_long  (tlv, TAG_NONE, v->sli);
    case MRP_TYPE_ULONG:  return mrp_tlv_push_ulong (tlv, TAG_NONE, v->uli);
    case MRP_TYPE_SSIZE:  return mrp_tlv_push_ssize (tlv, TAG_NONE, v->ssz);
    case MRP_TYPE_SIZE:   return mrp_tlv_push_size  (tlv, TAG_NONE, v->usz);
    case MRP_TYPE_STRING: return mrp_tlv_push_string(tlv, TAG_NONE, v->str);
    case MRP_TYPE_HOOK:   return 0;
    default:              break;
    }

    errno = EINVAL;
    return -1;
}


static inline int get_integer_value(mrp_type_t *type, void *data)
{
    mrp_value_t *v = data;

    switch (type->id) {
    case MRP_TYPE_INT8:   return (int)v->s8;
    case MRP_TYPE_UINT8:  return (int)v->u8;
    case MRP_TYPE_INT16:  return (int)v->s16;
    case MRP_TYPE_UINT16: return (int)v->u16;
    case MRP_TYPE_INT32:  return (int)v->s32;
    case MRP_TYPE_UINT32: return (int)v->u32;
    case MRP_TYPE_INT64:  return (int)v->s64;
    case MRP_TYPE_UINT64: return (int)v->u64;
    case MRP_TYPE_SHORT:  return (int)v->ssi;
    case MRP_TYPE_USHORT: return (int)v->usi;
    case MRP_TYPE_ENUM:   /* fallthrough, alias for MRP_TYPE_INT */
    case MRP_TYPE_INT:    return (int)v->si;
    case MRP_TYPE_UINT:   return (int)v->ui;
    case MRP_TYPE_LONG:   return (int)v->sli;
    case MRP_TYPE_ULONG:  return (int)v->uli;
    case MRP_TYPE_SSIZE:  return (int)v->ssz;
    case MRP_TYPE_SIZE:   return (int)v->usz;
    default:              return -1;
    }
}


static inline int get_guard_size(mrp_type_t *type, mrp_value_t *v)
{
    switch (type->id) {
    case MRP_TYPE_INT8:   return sizeof(v->s8 );
    case MRP_TYPE_UINT8:  return sizeof(v->u8 );
    case MRP_TYPE_INT16:  return sizeof(v->s16);
    case MRP_TYPE_UINT16: return sizeof(v->u16);
    case MRP_TYPE_INT32:  return sizeof(v->s32);
    case MRP_TYPE_UINT32: return sizeof(v->u32);
    case MRP_TYPE_INT64:  return sizeof(v->s64);
    case MRP_TYPE_UINT64: return sizeof(v->u64);
    case MRP_TYPE_SHORT:  return sizeof(v->ssi);
    case MRP_TYPE_USHORT: return sizeof(v->usi);
    case MRP_TYPE_ENUM:   /* fallthrough, alias for MRP_TYPE_INT */
    case MRP_TYPE_INT:    return sizeof(v->si );
    case MRP_TYPE_UINT:   return sizeof(v->ui );
    case MRP_TYPE_LONG:   return sizeof(v->sli);
    case MRP_TYPE_ULONG:  return sizeof(v->uli);
    case MRP_TYPE_SSIZE:  return sizeof(v->ssz);
    case MRP_TYPE_SIZE:   return sizeof(v->usz);
    case MRP_TYPE_STRING: return v ? strlen(v->strp) + 1 : 0;
    default:              return -1;
    }
}


static int get_guard_info(mrp_member_t *m, mrp_type_id_t *idp, size_t *offsp,
                          size_t *sizep, mrp_layout_t *loutp)
{
    if (m->array.size == NULL) {         /* array of basic type */
        *idp   = m->type->id;
        *offsp = 0;

        if (m->type->id == MRP_TYPE_STRING) {
            *sizep = m->array.guard.strp ? strlen(m->array.guard.strp) + 1 : 0;
            *loutp = MRP_LAYOUT_INDIRECT;
        }
        else {
            *sizep = m->type->size;
            *loutp = MRP_LAYOUT_INLINED;
        }
    }
    else {
        *idp   = m->array.size->type->id;
        *offsp = m->array.size->offs;

        if (m->array.size->type->id == MRP_TYPE_STRING) {
            *sizep = m->array.guard.strp ? strlen(m->array.guard.strp) + 1 : 0;
            *loutp = MRP_LAYOUT_INDIRECT;
        }
        else {
            *sizep = m->array.size->type->size;
            *loutp = MRP_LAYOUT_INLINED;
        }
    }

    return 0;
}


static int count_array_items(mrp_type_t *type, void *data,
                             mrp_member_t *m, void *md)
{
    mrp_member_t  *cntm;
    void          *cntd;
    mrp_type_id_t  id;
    void          *item, *guard;
    mrp_value_t   *grdv;
    size_t         offs, size;
    mrp_layout_t   layout;
    int            cnt, end;

    MRP_UNUSED(type);

    switch (m->array.type) {
    case MRP_ARRAY_SIZED:
        cntm = m->array.size;
        cntd = member_address(data, cntm);

        return get_integer_value(cntm->type, cntd);

    case MRP_ARRAY_GUARD:
        grdv = &m->array.guard;

        if (get_guard_info(m, &id, &offs, &size, &layout) < 0)
            return -1;

        for (cnt = 0, item = md; ; cnt++, item += m->type->size) {
            if (layout == MRP_LAYOUT_INDIRECT)
                guard = *(void **)(item + offs);
            else
                guard = (item + offs);

            if (id == MRP_TYPE_STRING)
                end = ((size == 0 && guard == NULL) ||
                       (size != 0 && strcmp(guard, grdv->strp)));
            else
                end = !memcmp(guard, grdv, size);

            if (end)
                break;
        }

        return cnt;

    default:
        return (int)m->array.type;
    }
}


static int encode_array(mrp_tlv_t *tlv, mrp_type_t *type, void *data, int cnt,
                        mrp_type_map_t *map)
{
    void *item;
    int   i;

    if (mrp_tlv_push_int32(tlv, TAG_ARRAY_START, mapped_id(map, type->id)) < 0)
        return -1;

    if (mrp_tlv_push_uint32(tlv, TAG_COUNT, (uint32_t)cnt) < 0)
        return -1;

    for (i = 0; i < cnt; i++) {
        if (mrp_tlv_push_uint32(tlv, TAG_ARRAY_ITEM, (uint32_t)i) < 0)
            return -1;

        if (type->id == MRP_TYPE_STRING)
            item = *(char **)(data + i * type->size);
        else
            item =           (data + i * type->size);

        if (encode_type(tlv, type, item, map) < 0)
            return -1;
    }

    if (mrp_tlv_push_int32(tlv, TAG_ARRAY_END, mapped_id(map, type->id)) < 0)
        return -1;

    return 0;
}


static int encode_list(mrp_tlv_t *tlv, void *listp, mrp_type_t *type,
                       size_t hook_offs, mrp_type_map_t *map)
{
    mrp_list_hook_t *p, *n;
    void            *data;
    uint32_t         cnt;

    cnt = 0;
    mrp_list_foreach((mrp_list_hook_t *)listp, p, n) {
        cnt++;
    }

    if (mrp_tlv_push_int32(tlv, TAG_LIST_START, mapped_id(map, type->id)) < 0)
        return -1;

    if (mrp_tlv_push_uint32(tlv, TAG_COUNT, cnt) < 0)
        return -1;

    cnt = 0;
    mrp_list_foreach((mrp_list_hook_t *)listp, p, n) {
        if (mrp_tlv_push_uint32(tlv, TAG_LIST_ITEM, cnt) < 0)
            return -1;

        data = (void *)p - hook_offs;

        if (encode_type(tlv, type, data, map) < 0)
            return -1;

        cnt++;
    }

    if (mrp_tlv_push_int32(tlv, TAG_LIST_END, mapped_id(map, type->id)) < 0)
        return -1;

    return 0;
}


static int count_flexi_items(mrp_type_t *type, void *data, mrp_type_id_t *idp)
{
    mrp_member_t *m, *um;
    void         *keyd;

    if (type->nmember < 1)
        return 0;

    if (union_type(type)) {
        keyd = member_address(data, type->key);
        um = union_member_by_key(type->key, keyd);

        if (um->type->flexible)
            return count_flexi_items(um->type, data + um->offs, idp);
        else {
            *idp = MRP_TYPE_UNKNOWN;
            return 0;
        }
    }

    m = type->ordered[type->nmember - 1];

    if (m->mod == MRP_TYPE_ARRAY && m->array.flexi) {
        *idp = m->type->id;
        return count_array_items(type, data, m, data + m->offs);
    }

    if (m->mod == MRP_TYPE_UNION) {
        mrp_member_t    *key = m->unio.key;
        mrp_list_hook_t *p, *n;

        mrp_list_foreach_back(&key->unio.hook, p, n) {
            keyd = member_address(data, key);
            um = mrp_list_entry(p, typeof(*um), unio.hook);

            if (um->offs + um->size != type->size)
                continue;

            if (union_member_has_key(um, key, keyd)) {
                if (!um->type->flexible) {
                    *idp = MRP_TYPE_UNKNOWN;
                    return 0;
                }
                else {
                    return count_flexi_items(um->type, data + um->offs, idp);
                }
            }
        }
    }

    if (m->type->flexible) {
        if (union_type(m->type)) {
            keyd = member_address(data + m->offs, m->type->key);
            um   = union_member_by_key(m->type->key, keyd);

            if (um->type->flexible)
                return count_flexi_items(um->type, data + um->offs, idp);
            else {
                *idp = MRP_TYPE_UNKNOWN;
                return 0;
            }
        }
        else
            return count_flexi_items(m->type, data + m->offs, idp);
    }

    errno = EINVAL;
    return -1;
}


static int encode_union(mrp_tlv_t *tlv, mrp_type_t *type, void *data,
                        mrp_type_map_t *map)
{
    mrp_member_t  *m;
    void          *keyd;
    int32_t        cnt;
    mrp_type_id_t  fid;

    if (mrp_tlv_push_int32(tlv, TAG_UNION_START, mapped_id(map, type->id)) < 0)
        return -1;

    if (type->flexible) {
        if ((cnt = count_flexi_items(type, data, &fid)) < 0)
            return -1;

        if (mrp_tlv_push_int32(tlv, TAG_FLEXI, mapped_id(map, fid)) < 0 ||
            mrp_tlv_push_int32(tlv, TAG_NONE , cnt)                 < 0)
            return -1;
    }

    keyd = member_address(data, type->key);
    m    = union_member_by_key(type->key, keyd);

    mrp_debug("%s: chose member %s (%s)", type->name, m->name, m->type->name);

    if (encode_type(tlv, m->type, data, map) < 0)
        return -1;

    if (mrp_tlv_push_int32(tlv, TAG_UNION_END, mapped_id(map, type->id)) < 0)
        return -1;

    return 0;
}


static int encode_union_member(mrp_tlv_t *tlv, mrp_type_t *type, void *data,
                               mrp_member_t *key, mrp_type_map_t *map)
{
    mrp_member_t    *m;
    mrp_list_hook_t *p, *n;
    void            *keyd;
    uint32_t         idx;
    int32_t          id;

    keyd = member_address(data, key);

    mrp_list_foreach(&key->unio.hook, p, n) {
        m = mrp_list_entry(p, typeof(*m), unio.hook);

        if (union_member_has_key(m, key, keyd)) {
            mrp_debug("%s: member %s (%s) matches ",
                      type->name, m->name, m->type->name);

            idx = (uint32_t)m->idx;
            id  = (int32_t) mapped_id(map, m->type->id);
            if (mrp_tlv_push_uint32(tlv, TAG_UNION_MEMBER, idx) < 0 ||
                mrp_tlv_push_int32 (tlv, TAG_NONE        , id ) < 0)
                return -1;

            if (encode_type(tlv, m->type, data + m->offs, map) < 0)
                return -1;
        }
    }

    return 0;
}


static int encode_struct(mrp_tlv_t *tlv, mrp_type_t *type, void *data,
                         mrp_type_map_t *map)
{
    mrp_list_hook_t *mp, *mn;
    mrp_member_t    *m;
    void            *md;
    int32_t          cnt;
    mrp_type_id_t    fid;

    if (mrp_tlv_push_int32(tlv, TAG_STRUCT_START, mapped_id(map, type->id)) < 0)
        return -1;

    if (type->flexible) {
        if ((cnt = count_flexi_items(type, data, &fid)) < 0)
            return -1;

        if (mrp_tlv_push_int32(tlv, TAG_FLEXI, mapped_id(map, fid)) < 0 ||
            mrp_tlv_push_int32(tlv, TAG_NONE , cnt)                 < 0)
            return -1;
    }

    mrp_list_foreach(&type->encode, mp, mn) {
        m  = mrp_list_entry(mp, typeof(*m), hook);
        md = member_address(data, m);

        if (mrp_tlv_push_int32(tlv, TAG_MEMBER, (uint32_t)m->idx) < 0)
            return -1;

        switch (m->mod) {
        case MRP_TYPE_ARRAY:
            cnt = count_array_items(type, data, m, md);

            if (cnt < 0)
                return -1;

            if (encode_array(tlv, m->type, md, cnt, map) < 0)
                return -1;
            continue;

        case MRP_TYPE_LIST:
            if (encode_list(tlv, md, m->type, m->list.hook->offs, map) < 0)
                return -1;
            continue;

        case MRP_TYPE_UNION_KEY:
        case MRP_TYPE_UNION:
            mrp_log_error("%s(): called with <union%s|%s>.", __FUNCTION__,
                          m->mod == MRP_TYPE_UNION ? "" : " key",
                          m->type->name);
            return -1;

        default:
            return -1;

        case MRP_TYPE_SCALAR:
            break;
        }

        if (basic_type(m->type->id) && !mrp_list_empty(&m->unio.hook)) {
            if (encode_type(tlv, m->type, md, map) < 0)
                return -1;
            if (encode_union_member(tlv, type, data, m, map) < 0)
                return -1;
        }
        else
            if (encode_type(tlv, m->type, md, map) < 0)
                return -1;
    }

    if (mrp_tlv_push_int32(tlv, TAG_STRUCT_END, mapped_id(map, type->id)) < 0)
        return -1;

    return 0;
}


static int encode_type(mrp_tlv_t *tlv, mrp_type_t *type, void *data,
                       mrp_type_map_t *map)
{
    if (basic_type(type->id))
        return encode_basic(tlv, type, data);
    else if (struct_type(type))
        return encode_struct(tlv, type, data, map);
    else if (union_type(type))
        return encode_union(tlv, type, data, map);

    mrp_log_error("%s(): can't handle <%s>", __FUNCTION__, type->name);
    errno = EINVAL;

    return -1;
}


int mrp_encode_type(mrp_type_id_t id, void *data, void **bufp, size_t *sizep,
                    mrp_type_map_t *map, size_t reserve)
{
    mrp_type_t *type = type_by_id(id);
    mrp_tlv_t   tlv  = MRP_TLV_EMPTY;

    *bufp  = NULL;
    *sizep = 0;

    if (type == NULL || incomplete_type(type))
        goto invalid_type;

    if (mrp_tlv_setup_write(&tlv, reserve + 4096) < 0)
        goto fail;

    if (reserve > 0)
        if (mrp_tlv_reserve(&tlv, reserve, 1) == NULL)
            goto fail;

    if (encode_type(&tlv, type, data, map) < 0)
        goto fail;

    mrp_tlv_trim(&tlv);
    mrp_tlv_steal(&tlv, bufp, sizep);

    return 0;

 invalid_type:
    errno = EINVAL;
 fail:
    mrp_tlv_cleanup(&tlv);

    return -1;
}




static int decode_type(mrp_tlv_t *tlv, mrp_list_hook_t *cl, mrp_type_t *type,
                       void **datap, mrp_type_map_t *map, int max);

static inline void *chunk_alloc(mrp_list_hook_t *cl, size_t size)
{
    chunk_t *c;

    if (size != 0) {
        if ((c = mrp_allocz(MRP_OFFSET(chunk_t, data[size]))) != NULL) {
            mrp_list_init(&c->hook);
            mrp_list_append(cl, &c->hook);

            return &c->data[0];
        }
    }

    return NULL;
}


static void *chunk_stralloc(size_t size, void *ptr)
{
    return chunk_alloc((mrp_list_hook_t *)ptr, size);
}


static void chunk_free(mrp_list_hook_t *h)
{
    chunk_t         *c;
    mrp_list_hook_t *p, *n;

    if (h == NULL)
        return;

    mrp_list_foreach(h, p, n) {
        c = mrp_list_entry(p, typeof(*c), hook);
        mrp_list_delete(&c->hook);
        mrp_free(c);
    }
}


static inline int decode_basic(mrp_tlv_t *tlv, mrp_list_hook_t *cl,
                               mrp_type_t *type, void **datap, int max)
{
    mrp_value_t *v = *datap;

    switch (type->id) {
    case MRP_TYPE_INT8:   return mrp_tlv_pull_int8  (tlv, TAG_NONE, &v->s8 );
    case MRP_TYPE_UINT8:  return mrp_tlv_pull_uint8 (tlv, TAG_NONE, &v->u8 );
    case MRP_TYPE_INT16:  return mrp_tlv_pull_int16 (tlv, TAG_NONE, &v->s16);
    case MRP_TYPE_UINT16: return mrp_tlv_pull_uint16(tlv, TAG_NONE, &v->u16);
    case MRP_TYPE_INT32:  return mrp_tlv_pull_int32 (tlv, TAG_NONE, &v->s32);
    case MRP_TYPE_UINT32: return mrp_tlv_pull_uint32(tlv, TAG_NONE, &v->u32);
    case MRP_TYPE_INT64:  return mrp_tlv_pull_int64 (tlv, TAG_NONE, &v->s64);
    case MRP_TYPE_UINT64: return mrp_tlv_pull_uint64(tlv, TAG_NONE, &v->u64);
    case MRP_TYPE_FLOAT:  return mrp_tlv_pull_float (tlv, TAG_NONE, &v->flt);
    case MRP_TYPE_DOUBLE: return mrp_tlv_pull_double(tlv, TAG_NONE, &v->dbl);
    case MRP_TYPE_BOOL:   return mrp_tlv_pull_bool  (tlv, TAG_NONE, &v->bln);
    case MRP_TYPE_SHORT:  return mrp_tlv_pull_short (tlv, TAG_NONE, &v->ssi);
    case MRP_TYPE_USHORT: return mrp_tlv_pull_ushort(tlv, TAG_NONE, &v->usi);
    case MRP_TYPE_ENUM:   /* fallthrough, alias for MRP_TYPE_INT */
    case MRP_TYPE_INT:    return mrp_tlv_pull_int   (tlv, TAG_NONE, &v->si );
    case MRP_TYPE_UINT:   return mrp_tlv_pull_uint  (tlv, TAG_NONE, &v->ui );
    case MRP_TYPE_LONG:   return mrp_tlv_pull_long  (tlv, TAG_NONE, &v->sli);
    case MRP_TYPE_ULONG:  return mrp_tlv_pull_ulong (tlv, TAG_NONE, &v->uli);
    case MRP_TYPE_SSIZE:  return mrp_tlv_pull_ssize (tlv, TAG_NONE, &v->ssz);
    case MRP_TYPE_SIZE:   return mrp_tlv_pull_size  (tlv, TAG_NONE, &v->usz);
    case MRP_TYPE_HOOK:   return 0;
    case MRP_TYPE_STRING:
        return mrp_tlv_pull_string(tlv, TAG_NONE, (char **)datap, max,
                                   chunk_stralloc, cl);
    default:
        break;
    }

    errno = EINVAL;
    return -1;

}


static int init_type(mrp_type_t *type, void *data)
{
    mrp_list_hook_t *p, *n;
    mrp_member_t    *m;

    mrp_list_foreach(&type->init, p, n) {
        m = mrp_list_entry(p, typeof(*m), hook);

        switch (m->type->id) {
        case MRP_TYPE_HOOK:
            mrp_list_init((mrp_list_hook_t *)(data + m->offs));
            break;
        default:
            break;
        }
    }

    return 0;
}


static void *alloc_array(mrp_list_hook_t *cl, mrp_type_t *type, uint32_t cnt)
{
    return chunk_alloc(cl, type->size * cnt);
}


static void *alloc_type(mrp_list_hook_t *cl, mrp_type_t *type, int flexible)
{
    return chunk_alloc(cl, type->size + flexible);
}


static int flexible_size(mrp_tlv_t *tlv, mrp_type_map_t *map)
{
    mrp_type_t *type;
    int32_t     id, cnt;

    if (mrp_tlv_pull_int32(tlv, TAG_FLEXI, &id ) < 0 ||
        mrp_tlv_pull_int32(tlv, TAG_NONE , &cnt) < 0) {
        errno = EILSEQ;
        return -1;
    }

    if (cnt == 0)
        return 0;

    if ((type = type_by_id(native_id(map, id))) == NULL) {
        errno = EINVAL;
        return -1;
    }

    return cnt * type->size;
}


static int decode_list(mrp_tlv_t *tlv, mrp_list_hook_t *cl, mrp_type_t *type,
                       size_t hook_offs, void **datap, mrp_type_map_t *map)
{
    mrp_list_hook_t *list, *hook;
    int32_t          id;
    uint32_t         i, idx, cnt;
    void            *item;

    if (mrp_tlv_pull_int32 (tlv, TAG_LIST_START, &id ) < 0 ||
        mrp_tlv_pull_uint32(tlv, TAG_COUNT     , &cnt) < 0)
        goto invalid;

    if (mapped_id(map, id) != type->id)
        goto invalid_type;

    list = (mrp_list_hook_t *)datap;

    for (i = 0; i < cnt; i++) {
        if (mrp_tlv_pull_uint32(tlv, TAG_LIST_ITEM, &idx) < 0 || idx != i)
            goto invalid;

        item = NULL;
        if (decode_type(tlv, cl, type, &item, map, type->size) < 0)
            goto fail;

        hook = item + hook_offs;
        mrp_list_init(hook);             /* shouldn't be necessary */
        mrp_list_append(list, hook);
    }

    if (mrp_tlv_pull_int32(tlv, TAG_LIST_END, &id) < 0)
        goto invalid;

    if (mapped_id(map, id) != type->id)
        goto invalid_type;

    return 0;

 invalid_type:
    errno = EINVAL;
    return -1;

 invalid:
    errno = EILSEQ;
 fail:
    return -1;
}


static int terminate_array(mrp_member_t *m, void *data, uint32_t cnt)
{
    mrp_type_id_t  id;
    mrp_layout_t   layout;
    mrp_value_t   *grdv;
    size_t         offs, size;
    void          *item, *guard;

    if (get_guard_info(m, &id, &offs, &size, &layout) < 0)
        return -1;

    item = data + cnt * m->type->size;
    grdv = &m->array.guard;

    if (layout == MRP_LAYOUT_INDIRECT)
        guard = *(void **)(item + offs);
    else
        guard = (item + offs);

    if (id != MRP_TYPE_STRING)
        memcpy(guard, grdv, size);
    else {
        if (layout == MRP_LAYOUT_INDIRECT) {
            if (size != 0) {
                mrp_log_error("%s: can't handle non-NULL indirect string guard"
                              "for array of %s.", m->name, m->type->name);
                errno = EINVAL;
                return -1;
            }
        }
        else {
            if (size > m->size) {
                mrp_log_error("%s: guard string overflow (%zd > %zd).",
                              m->name, size, m->size);
                errno = EOVERFLOW;
                return -1;
            }

            strcpy(guard, grdv->strp);
        }
    }

    return 0;
}


static int decode_array(mrp_tlv_t *tlv, mrp_list_hook_t *cl, mrp_member_t *m,
                        void **datap, mrp_type_map_t *map)
{
    mrp_type_t *type = m->type;
    int32_t     id;
    uint32_t    cnt, grd, i, idx;
    void       *data, *md, **mdp;
    int         max;

    if (mrp_tlv_pull_int32 (tlv, TAG_ARRAY_START, &id ) < 0 ||
        mrp_tlv_pull_uint32(tlv, TAG_COUNT      , &cnt) < 0)
        goto invalid;

    if (native_id(map, id) != type->id)
        goto invalid_type;

    data = *datap;

    if (m->array.type == MRP_ARRAY_GUARD)
        grd = 1;
    else
        grd = 0;

    if (data == NULL && (data = alloc_array(cl, type, cnt + grd)) == NULL)
        if (cnt != 0)
            return -1;

    for (i = 0; i < cnt; i++) {
        if (mrp_tlv_pull_uint32(tlv, TAG_ARRAY_ITEM, &idx) < 0 || idx != i)
            goto invalid;

        if (type->id != MRP_TYPE_STRING) {
            md  = data + i * type->size;
            mdp = &md;
            max = type->size;
        }
        else {
            mdp = (void **)(data + i * type->size);
            max = -1;
        }

        if (decode_type(tlv, cl, type, mdp, map, max) < 0)
            return -1;
    }

    if (mrp_tlv_pull_int32 (tlv, TAG_ARRAY_END, &id) < 0 ||
        native_id(map, id) != type->id)
        goto invalid;

    if (*datap == NULL)
        *datap = data;

    if (grd) {
        if (terminate_array(m, data, cnt) < 0)
            return -1;
    }

    return 0;

 invalid:
    errno = EILSEQ;
    return -1;

 invalid_type:
    errno = EINVAL;
    return -1;
}


static int decode_union_member(mrp_tlv_t *tlv, mrp_list_hook_t *cl,
                               mrp_type_t *type, void *data, mrp_member_t *m,
                               mrp_type_map_t *map)
{
    mrp_member_t *u;
    uint32_t      idx;
    int32_t       id;
    void         *md, **mdp;
    int           max;

    MRP_UNUSED(m);

    while (mrp_tlv_pull_uint32(tlv, TAG_UNION_MEMBER, &idx) == 0) {
        if (mrp_tlv_pull_int32(tlv, TAG_NONE        , &id ) < 0)
            goto invalid;

        if (idx > (uint32_t)type->nmember)
            goto invalid_type;

        u = type->ordered[idx];

        if (native_id(map, id) != u->type->id)
            goto invalid_type;

        if (u->layout == MRP_LAYOUT_INLINED) {
            md  = data + u->offs;
            mdp = &md;
            max = u->size;
        }
        else {
            mdp = (void **)(data + u->offs);
            max = -1;
        }

        if (decode_type(tlv, cl, u->type, mdp, map, max) < 0)
            return -1;
    }

    return 0;

 invalid_type:
    errno = EINVAL;
    return -1;

 invalid:
    errno = EILSEQ;
    return -1;
}


static int decode_union(mrp_tlv_t *tlv, mrp_list_hook_t *cl, mrp_type_t *type,
                        void **datap, mrp_type_map_t *map)
{
    mrp_type_t *t;
    int32_t     id;
    int         flexible;
    void       *data;

    if (mrp_tlv_pull_int32(tlv, TAG_UNION_START, &id) < 0)
        goto invalid;

    if (native_id(map, id) != type->id)
        goto invalid_type;

    data = *datap;

    if (type->flexible) {
        if ((flexible = flexible_size(tlv, map)) < 0)
            return -1;
    }
    else
        flexible = 0;

    if (data == NULL && (data = alloc_type(cl, type, flexible)) == NULL)
        return -1;

    init_type(type, data);

    if (mrp_tlv_peek_int32(tlv, TAG_STRUCT_START, &id) < 0)
        goto invalid;

    t = type_by_id(native_id(map, id));

    if (t == NULL)
        goto invalid_type;

    if (decode_type(tlv, cl, t, datap, map, -1) < 0)
        return -1;

    if (mrp_tlv_pull_int32(tlv, TAG_UNION_END, &id) < 0)
        goto invalid;

    if (native_id(map, id) != type->id)
        goto invalid;

    return 0;

 invalid_type:
    errno = EINVAL;
    return -1;

 invalid:
    errno = EILSEQ;
    return -1;
}


static int decode_struct(mrp_tlv_t *tlv, mrp_list_hook_t *cl, mrp_type_t *type,
                         void **datap, mrp_type_map_t *map)
{
    int32_t       id;
    uint32_t      tag, idx;
    void         *data, *md, *mdp;
    int           flexible, max;
    mrp_member_t *m;

    if (mrp_tlv_pull_int32(tlv, TAG_STRUCT_START, &id) < 0)
        goto invalid;

    if (native_id(map, id) != type->id)
        goto invalid_type;

    data = *datap;

    if (type->flexible) {
        if ((flexible = flexible_size(tlv, map)) < 0)
            return -1;
    }
    else
        flexible = 0;

    if (data == NULL && (data = alloc_type(cl, type, flexible)) == NULL)
        return -1;

    init_type(type, data);

    while (mrp_tlv_peek_tag(tlv, &tag) == 0 && tag == TAG_MEMBER) {
        if (mrp_tlv_pull_uint32(tlv, TAG_MEMBER, &idx) < 0)
            goto invalid;

        if (idx >= (uint32_t)type->nmember)
            goto invalid_member;

        m  = type->ordered[idx];
        id = m->type->id;

        if (m->layout == MRP_LAYOUT_INLINED) {
            md  = data + m->offs;
            mdp = &md;
            max = m->size;
        }
        else {
            mdp = (void **)(data + m->offs);
            max = -1;
        }

        switch (m->mod) {
        case MRP_TYPE_ARRAY:
            if (decode_array(tlv, cl, m, mdp, map) < 0)
                return -1;
            else
                continue;

        case MRP_TYPE_LIST:
            if (decode_list(tlv, cl, m->type, m->list.hook->offs, mdp, map) < 0)
                return -1;
            else
                continue;

        case MRP_TYPE_UNION_KEY:
        case MRP_TYPE_UNION:
            mrp_log_error("%s(): called with <union%s|%s>.", __FUNCTION__,
                          m->mod == MRP_TYPE_UNION ? "" : " key",
                          m->type->name);
            goto invalid_type;

        default:
            goto invalid_type;

        case MRP_TYPE_SCALAR:
            break;
        }

        if (basic_type(m->type->id) && !mrp_list_empty(&m->unio.hook)) {
            if (decode_type(tlv, cl, m->type, mdp, map, max) < 0)
                return -1;
            if (decode_union_member(tlv, cl, type, data, m, map) < 0)
                return -1;
        }
        else
            if (decode_type(tlv, cl, m->type, mdp, map, max) < 0)
                return -1;
    }

    if (mrp_tlv_pull_int32(tlv, TAG_STRUCT_END, &id) < 0)
        return -1;

    if (native_id(map, id) != type->id)
        goto invalid_type;

    if (*datap == NULL)
        *datap = data;

    return 0;

 invalid_type:
    errno = EINVAL;
    return -1;

 invalid:
    errno = EILSEQ;
    return -1;

 invalid_member:
    errno = EINVAL;
    return -1;
}


static int decode_type(mrp_tlv_t *tlv, mrp_list_hook_t *cl, mrp_type_t *type,
                       void **datap, mrp_type_map_t *map, int max)
{
    if (struct_type(type))
        return decode_struct(tlv, cl, type, datap, map);
    else if (union_type(type))
        return decode_union(tlv, cl, type, datap, map);
    else if (basic_type(type->id))
        return decode_basic(tlv, cl, type, datap, max);

    errno = EINVAL;
    return -1;
}


int mrp_decode_type(mrp_type_id_t *idp, void **datap, void *buf, size_t size,
                    mrp_type_map_t *map)
{
    mrp_list_hook_t  chunks = MRP_LIST_INIT(chunks);
    mrp_tlv_t        tlv    = MRP_TLV_READ(buf, size);
    int32_t          id     = *idp;
    mrp_type_t      *type;
    uint32_t         tag;
    int              len;

    *datap = NULL;

    if (mrp_tlv_peek_tag(&tlv, &tag) < 0)
        goto invalid;

    switch (tag) {
    case TAG_STRUCT_START:
    case TAG_UNION_START:
        if (mrp_tlv_peek_int32(&tlv, tag, &id) < 0)
            goto invalid;

        if (*idp == MRP_TYPE_UNKNOWN)
            *idp = native_id(map, id);
        else
            if (native_id(map, id) != *idp)
                goto invalid_type;

        id   = native_id(map, id);
        type = type_by_id(id);

        if (type == NULL)
            goto invalid_type;

        if (decode_type(&tlv, &chunks, type, datap, map, -1) < 0) {
            chunk_free(&chunks);

            return -1;
        }
        break;

    default:
        goto invalid;
    }

    mrp_list_delete(&chunks);
    len = (int)mrp_tlv_offset(&tlv);

    return len;

 invalid:
    errno = EILSEQ;
    return -1;

 invalid_type:
    errno = EINVAL;
    return -1;
}


void mrp_free_type(mrp_type_id_t id, void *data)
{
    chunk_t *c;

    MRP_UNUSED(id);

    if (data != NULL) {
        c = (chunk_t *)((void *)data - MRP_OFFSET(chunk_t, data[0]));
        chunk_free(&c->hook);
        mrp_free(c);
    }
}
