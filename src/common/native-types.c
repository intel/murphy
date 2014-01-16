/*
 * Copyright (c) 2012, 2013, Intel Corporation
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

#include <murphy/common/macros.h>
#include <murphy/common/debug.h>
#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/tlv.h>
#include <murphy/common/native-types.h>


/*
 * TLV tags we use when encoding/decoding our native types
 */

typedef enum {
    TAG_NONE = MRP_TLV_UNTAGGED,         /* untagged data */
    TAG_STRUCT,                          /* a native structure */
    TAG_MEMBER,                          /* a native structure member */
    TAG_ARRAY,                           /* an array */
    TAG_NELEM,                           /* size of an array (in elements) */
} tag_t;


/*
 * extra header we use to keep track of memory while decoding
 */

typedef struct {
    mrp_list_hook_t hook;                /* hook to chunk list */
    char            data[0];             /* user-visible data */
} chunk_t;


static int encode_struct(mrp_tlv_t *tlv, void *data, mrp_native_type_t *t,
                         mrp_typemap_t *idmap);
static int decode_struct(mrp_tlv_t *tlv, mrp_list_hook_t **chunks,
                         void **datap, uint32_t *idp, mrp_typemap_t *idmap);
static int print_struct(char **buf, size_t *size, int level,
                        void *data, mrp_native_type_t *t);
static void free_native(mrp_native_type_t *t);

static void *alloc_chunk(mrp_list_hook_t **chunks, size_t size);
static void free_chunks(mrp_list_hook_t *chunks);


/*
 * list and table of registered native types
 */

static MRP_LIST_HOOK(types);
static int           ntype;

static mrp_native_type_t **typetbl;


static mrp_native_member_t *native_member(mrp_native_type_t *t, int idx)
{
    if (0 <= idx && idx < (int)t->nmember)
        return t->members + idx;
    else {
        errno = EINVAL;
        return NULL;
    }
}


static int member_index(mrp_native_type_t *t, const char *name)
{
    mrp_native_member_t *m;
    size_t               i;

    for (i = 0, m = t->members; i < t->nmember; i++, m++)
        if (!strcmp(m->any.name, name))
            return m - t->members;

    return -1;
}


static int copy_member(mrp_native_type_t *t, mrp_native_member_t *m)
{
    mrp_native_member_t *tm;
    size_t               size;

    if ((tm = native_member(t, member_index(t, m->any.name))) != NULL)
        return tm - t->members;
    else
        tm = t->members + t->nmember;

    *tm = *m;

    if (*m->any.name != '"')
         tm->any.name = mrp_strdup(m->any.name);
     else {
         size = strlen(m->any.name) + 1 - 2;

         if ((tm->any.name = mrp_allocz(size)) != NULL)
             strncpy(tm->any.name, m->any.name + 1, size - 1);
     }

    if (tm->any.name != NULL) {
        t->nmember++;
        return tm - t->members;
    }
    else
        return -1;
}


static mrp_native_type_t *find_type(const char *type_name)
{
    mrp_native_type_t *t;
    mrp_list_hook_t   *p, *n;

    mrp_list_foreach(&types, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);

        if (!strcmp(t->name, type_name))
            return t;
    }

    return NULL;
}


static mrp_native_type_t *lookup_type(uint32_t id)
{
    mrp_native_type_t *t;
    mrp_list_hook_t   *p, *n;

    /* XXX TODO: turn this into a real lookup instead of linear search */

    if (1 <= id && id <= (uint32_t)ntype)
        if ((t = typetbl[id]) != NULL && t->id == id)
            return t;

    mrp_log_warning("Type lookup for %u failed, doing linear search...\n", id);

    mrp_list_foreach(&types, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);

        if (t->id == id)
            return t;
    }

    return NULL;
}


static mrp_native_type_t *member_type(mrp_native_member_t *m)
{
    mrp_native_type_t *t;

    if (m->any.type != MRP_TYPE_STRUCT)
        t = lookup_type(m->any.type);
    else
        t = lookup_type(m->strct.data_type.id);

    if (t == NULL)
        errno = EINVAL;

    return t;
}


static inline uint32_t map_type(uint32_t id, mrp_typemap_t *idmap)
{
    uint32_t mapped = MRP_INVALID_TYPE;

    if (id < MRP_TYPE_STRUCT || idmap == NULL)
        mapped = id;
    else {
        while (idmap->type_id != MRP_INVALID_TYPE) {
            if (idmap->type_id == id) {
                mapped = MRP_TYPE_STRUCT + idmap->mapped;
                break;
            }
            else
                idmap++;
        }
    }

    return mapped;
}


static inline uint32_t mapped_type(uint32_t mapped, mrp_typemap_t *idmap)
{
    uint32_t id = MRP_INVALID_TYPE;

    if (mapped < MRP_TYPE_STRUCT || idmap == NULL)
        id = mapped;
    else {
        while (idmap->type_id != MRP_INVALID_TYPE) {
            if (MRP_TYPE_STRUCT + idmap->mapped == mapped) {
                id = idmap->type_id;
                break;
            }
            else
                idmap++;
        }
    }

    return id;
}


uint32_t mrp_type_id(const char *type_name)
{
    mrp_native_type_t *t;

    if ((t = find_type(type_name)) != NULL)
        return t->id;
    else
        return MRP_INVALID_TYPE;
}


static size_t type_size(uint32_t id)
{
    mrp_native_type_t *t = lookup_type(id);

    if (t != NULL)
        return t->size;
    else
        return 0;
}


static int matching_types(mrp_native_type_t *t1, mrp_native_type_t *t2)
{
    MRP_UNUSED(t1);
    MRP_UNUSED(t2);

    /* XXX TODO */
    return 0;
}


static void register_default_types(void)
{
#define DEFAULT_NTYPE (MRP_TYPE_STRUCT + 1)

#define DECLARE_TYPE(_ctype, _mtype)            \
    static mrp_native_type_t _mtype##_type = {  \
        .name    = #_ctype,                     \
        .id      = MRP_TYPE_##_mtype,           \
        .size    = sizeof(_ctype),              \
        .members = NULL,                        \
        .nmember = 0,                           \
        .hook    = { NULL, NULL }               \
    }

#define REGISTER_TYPE(_type)                    \
    mrp_list_init(&(_type)->hook);              \
    mrp_list_append(&types, &(_type)->hook);    \
    typetbl[(_type)->id] = (_type)

    if (mrp_reallocz(typetbl, 0, DEFAULT_NTYPE) == NULL) {
        mrp_log_error("Failed to initialize native type table.");
        abort();
    }

    DECLARE_TYPE( int8_t       , INT8  );
    DECLARE_TYPE(uint8_t       , UINT8 );
    DECLARE_TYPE(int16_t       , INT16 );
    DECLARE_TYPE(uint16_t      , UINT16);
    DECLARE_TYPE(int32_t       , INT32 );
    DECLARE_TYPE(uint32_t      , UINT32);
    DECLARE_TYPE(int64_t       , INT64 );
    DECLARE_TYPE(uint64_t      , UINT64);
    DECLARE_TYPE(float         , FLOAT );
    DECLARE_TYPE(double        , DOUBLE);
    DECLARE_TYPE(bool          , BOOL  );
    DECLARE_TYPE(int           , INT   );
    DECLARE_TYPE(unsigned int  , UINT  );
    DECLARE_TYPE(short         , SHORT );
    DECLARE_TYPE(unsigned short, USHORT);
    DECLARE_TYPE(size_t        , SIZET );
    DECLARE_TYPE(ssize_t       , SSIZET);
    DECLARE_TYPE(char *       , STRING);
    DECLARE_TYPE(void *       , BLOB  );
    DECLARE_TYPE(void *       , ARRAY );
    DECLARE_TYPE(void *       , STRUCT);

    REGISTER_TYPE(&INT8_type);
    REGISTER_TYPE(&UINT8_type);
    REGISTER_TYPE(&INT16_type);
    REGISTER_TYPE(&UINT16_type);
    REGISTER_TYPE(&INT32_type);
    REGISTER_TYPE(&UINT32_type);
    REGISTER_TYPE(&INT64_type);
    REGISTER_TYPE(&UINT64_type);
    REGISTER_TYPE(&FLOAT_type);
    REGISTER_TYPE(&DOUBLE_type);
    REGISTER_TYPE(&BOOL_type);
    REGISTER_TYPE(&INT_type);
    REGISTER_TYPE(&UINT_type);
    REGISTER_TYPE(&SHORT_type);
    REGISTER_TYPE(&USHORT_type);
    REGISTER_TYPE(&SIZET_type);
    REGISTER_TYPE(&SSIZET_type);
    REGISTER_TYPE(&STRING_type);
    REGISTER_TYPE(&BLOB_type);
    REGISTER_TYPE(&ARRAY_type);
    REGISTER_TYPE(&STRUCT_type);

    ntype = DEFAULT_NTYPE;

#undef DECLARE_TYPE
#undef REGISTER_TYPE
}


uint32_t mrp_register_native(mrp_native_type_t *type)
{
    mrp_native_type_t   *existing = find_type(type->name);
    mrp_native_type_t   *t, *elemt;
    mrp_native_member_t *s, *d, *m;
    int                  idx;

    (void)member_type;

    if (existing != NULL && !matching_types(existing, type)) {
        errno = EEXIST;
        return MRP_INVALID_TYPE;
    }

    if (ntype == 0)
        register_default_types();

    if ((t = mrp_allocz(sizeof(*t))) == NULL)
        return MRP_INVALID_TYPE;

    mrp_list_init(&t->hook);
    t->name = mrp_strdup(type->name);

    if (t->name == NULL)
        goto fail;

    t->size    = type->size;
    t->members = mrp_allocz_array(mrp_native_member_t, type->nmember);

    if (t->members == NULL && type->nmember != 0)
        goto fail;

    /*
     * Notes:
     *
     *   While we copy the members, we also take care of reordering them
     *   so that any member that another one depends on ('size' members)
     *   get registered (and consequently encoded and decoded) before the
     *   dependant members.
     */

    s = type->members;
    d = t->members;
    while (t->nmember < type->nmember) {
        /* make sure there are no duplicate members */
        if (native_member(type, member_index(type, s->any.name)) != s) {
            errno = EINVAL;
            goto fail;
        }

        /* skip already copied members */
        while (member_index(t, s->any.name) >= 0)
            s++;

        switch (s->any.type) {
        case MRP_TYPE_BLOB:
            m = native_member(t, member_index(t, s->blob.size.name));

            if (m == NULL) {
                m = native_member(type,
                                  member_index(type, s->blob.size.name));

                if (m == NULL)
                    goto fail;
                else
                    idx = copy_member(t, m);

                if (idx < 0)
                    goto fail;
            }
            else
                idx = m - t->members;

            if (copy_member(t, s) < 0)
                goto fail;

            d = t->members + t->nmember;
            d->blob.size.idx = idx;

            break;

        case MRP_TYPE_ARRAY:
            if (s->array.kind == MRP_ARRAY_SIZE_EXPLICIT) {
                m = native_member(t, member_index(t, s->array.size.name));

                if (m == NULL) {
                    m = native_member(type,
                                      member_index(type, s->array.size.name));

                    if (m == NULL)
                        goto fail;
                    else
                        idx = copy_member(t, m);

                    if (idx < 0)
                        goto fail;

                }
                else
                    idx = m - t->members;

                d = t->members + t->nmember;

                if (copy_member(t, s) < 0)
                    goto fail;

                d->array.size.idx = idx;
            }
            else {
                d = t->members + t->nmember;

                if (copy_member(t, s) < 0)
                    goto fail;
            }

            d->array.elem.id = mrp_type_id(d->array.elem.name);

            if (d->array.elem.id == MRP_INVALID_TYPE)
                goto fail;

            if (s->array.kind == MRP_ARRAY_SIZE_GUARDED) {
                elemt = lookup_type(d->array.elem.id);

                if (elemt == NULL)
                    goto fail;

                if (elemt->id < MRP_TYPE_ARRAY)
                    idx = 0;
                else {
                    idx = member_index(elemt, s->array.size.name);
                    d->array.size.idx = member_index(elemt, s->array.size.name);

                    if (d->array.size.idx == (uint32_t)-1)
                        goto fail;
                }
            }

            break;

        case MRP_TYPE_STRUCT:
            d = t->members + t->nmember;

            if (copy_member(t, s) < 0)
                goto fail;

            d->strct.data_type.id = mrp_type_id(d->strct.data_type.name);

            if (d->strct.data_type.id == MRP_INVALID_TYPE)
                goto fail;
            break;

        default:
            if (copy_member(t, s) < 0)
                goto fail;
        }
    }

    if (mrp_reallocz(typetbl, ntype, ntype + 1) == NULL)
        goto fail;

    t->id = ntype;
    mrp_list_append(&types, &t->hook);
    typetbl[ntype] = t;
    ntype++;

    return t->id;

 fail:
    free_native(t);

    return MRP_INVALID_TYPE;
}


static void free_native(mrp_native_type_t *t)
{
    mrp_native_member_t *m;
    size_t               i;

    if (t == NULL)
        return;

    mrp_list_delete(&t->hook);

    mrp_free(t->name);
    for (i = 0, m = t->members; i < t->nmember; i++, m++)
        mrp_free(m->any.name);
    mrp_free(t);
}


static int encode_basic(mrp_tlv_t *tlv, mrp_type_t type, mrp_value_t *v)
{
    switch (type) {
    case MRP_TYPE_INT8:    return mrp_tlv_push_int8  (tlv, TAG_NONE, v->s8);
    case MRP_TYPE_UINT8:   return mrp_tlv_push_uint8 (tlv, TAG_NONE, v->u8);
    case MRP_TYPE_INT16:   return mrp_tlv_push_int16 (tlv, TAG_NONE, v->s16);
    case MRP_TYPE_UINT16:  return mrp_tlv_push_uint16(tlv, TAG_NONE, v->u16);
    case MRP_TYPE_INT32:   return mrp_tlv_push_int32 (tlv, TAG_NONE, v->s32);
    case MRP_TYPE_UINT32:  return mrp_tlv_push_uint32(tlv, TAG_NONE, v->u32);
    case MRP_TYPE_INT64:   return mrp_tlv_push_int64 (tlv, TAG_NONE, v->s64);
    case MRP_TYPE_UINT64:  return mrp_tlv_push_uint64(tlv, TAG_NONE, v->u64);
    case MRP_TYPE_FLOAT:   return mrp_tlv_push_float (tlv, TAG_NONE, v->flt);
    case MRP_TYPE_DOUBLE:  return mrp_tlv_push_double(tlv, TAG_NONE, v->dbl);
    case MRP_TYPE_BOOL:    return mrp_tlv_push_bool  (tlv, TAG_NONE, v->bln);
    case MRP_TYPE_STRING:  return mrp_tlv_push_string(tlv, TAG_NONE, v->str);

    case MRP_TYPE_INT:
        return mrp_tlv_push_int32 (tlv, TAG_NONE, (int32_t)v->i);
    case MRP_TYPE_UINT:
        return mrp_tlv_push_uint32(tlv, TAG_NONE, (uint32_t)v->ui);
    case MRP_TYPE_SHORT:
        return mrp_tlv_push_int32 (tlv, TAG_NONE, (int32_t)v->si);
    case MRP_TYPE_USHORT:
        return mrp_tlv_push_uint32(tlv, TAG_NONE, (uint32_t)v->usi);
    case MRP_TYPE_SIZET:
        return mrp_tlv_push_uint32(tlv, TAG_NONE, (uint32_t)v->sz);
    case MRP_TYPE_SSIZET:
        return mrp_tlv_push_int32 (tlv, TAG_NONE, (int32_t)v->ssz);

    default:
        return -1;
    }
}


static inline int get_blob_size(void *base, mrp_native_type_t *t,
                                mrp_native_blob_t *m, size_t *sizep)
{
    mrp_native_member_t *sizem;
    mrp_value_t         *v;

    if ((sizem = native_member(t, m->size.idx)) == NULL)
        return -1;

    if (sizem->any.layout == MRP_LAYOUT_INDIRECT)
        v = *(void **)base;
    else
        v = base;

    switch (sizem->any.type) {
    case MRP_TYPE_INT8:   *sizep = v->s8;          return 0;
    case MRP_TYPE_UINT8:  *sizep = v->u8;          return 0;
    case MRP_TYPE_INT16:  *sizep = v->s16;         return 0;
    case MRP_TYPE_UINT16: *sizep = v->u16;         return 0;
    case MRP_TYPE_INT32:  *sizep = v->s32;         return 0;
    case MRP_TYPE_UINT32: *sizep = v->u32;         return 0;
    case MRP_TYPE_INT64:  *sizep = (size_t)v->s32; return 0;
    case MRP_TYPE_UINT64: *sizep = (size_t)v->u32; return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}


static int guard_offset_and_size(mrp_native_array_t *m, size_t *offsp,
                                 size_t *sizep)
{
    mrp_native_type_t   *t = lookup_type(m->elem.id);
    mrp_native_member_t *g;

    if (t == NULL)
        return -1;

    switch (t->id) {
    case MRP_TYPE_INT8:
    case MRP_TYPE_UINT8:
    case MRP_TYPE_INT16:
    case MRP_TYPE_UINT16:
    case MRP_TYPE_INT32:
    case MRP_TYPE_UINT32:
    case MRP_TYPE_INT64:
    case MRP_TYPE_UINT64:
    case MRP_TYPE_FLOAT:
    case MRP_TYPE_DOUBLE:
    case MRP_TYPE_BOOL:
    case MRP_TYPE_STRING:
    case MRP_TYPE_INT:
    case MRP_TYPE_UINT:
    case MRP_TYPE_SHORT:
    case MRP_TYPE_USHORT:
    case MRP_TYPE_SIZET:
    case MRP_TYPE_SSIZET:
        *offsp = 0;
        *sizep = t->size;
        return 0;

    default:
        if ((g = native_member(t, m->size.idx)) == NULL)
            return -1;

        *offsp = g->any.offs;
        *sizep = type_size(g->any.type);
        return 0;
    }
}


static inline int get_explicit_array_size(void *base, mrp_native_type_t *t,
                                          mrp_native_array_t *m)
{
    mrp_native_member_t *nelemm;
    mrp_value_t         *v;
    int                  n;

    if ((nelemm = native_member(t, m->size.idx)) == NULL)
        return -1;
    if (nelemm->any.layout == MRP_LAYOUT_INDIRECT)
        v = *(void **)(base + nelemm->any.offs);
    else
        v = base + nelemm->any.offs;

    switch (nelemm->any.type) {
    case MRP_TYPE_INT8:   n = v->s8;       break;
    case MRP_TYPE_UINT8:  n = v->u8;       break;
    case MRP_TYPE_INT16:  n = v->s16;      break;
    case MRP_TYPE_UINT16: n = v->u16;      break;
    case MRP_TYPE_INT32:  n = v->s32;      break;
    case MRP_TYPE_UINT32: n = v->u32;      break;
    case MRP_TYPE_INT64:  n = (int)v->s64; break;
    case MRP_TYPE_UINT64: n = (int)v->u64; break;

    case MRP_TYPE_INT:    n = (int)           v->i;   break;
    case MRP_TYPE_UINT:   n = (unsigned int)  v->ui;  break;
    case MRP_TYPE_SHORT:  n = (short)         v->si;  break;
    case MRP_TYPE_USHORT: n = (unsigned short)v->usi; break;
    case MRP_TYPE_SIZET:  n = (size_t)        v->sz;  break;
    case MRP_TYPE_SSIZET: n = (ssize_t)       v->ssz; break;

    default:
        errno = EINVAL;
        return -1;
    }

    return n;
}


static inline int get_guarded_array_size(void *arrp, mrp_native_array_t *m)
{
    mrp_value_t *guard;
    size_t       goffs, gsize, esize;
    int          n;

    if ((esize = type_size(m->elem.id)) == 0)
        return -1;

    if (guard_offset_and_size(m, &goffs, &gsize) < 0)
        return -1;

    guard = &m->sentinel;

    for (n = 0; memcmp(arrp + n * esize + goffs, guard, gsize); n++)
            ;
    return n;
}


static int get_array_size(void *base, mrp_native_type_t *t, void *arrp,
                          mrp_native_array_t *m, size_t *nelemp,
                          size_t *esizep)
{
    int n;

    if ((*esizep = type_size(m->elem.id)) == 0)
        return -1;

    switch (m->kind) {
    case MRP_ARRAY_SIZE_FIXED:
        *nelemp = m->size.nelem;
        return 0;

    case MRP_ARRAY_SIZE_EXPLICIT:
        if ((n = get_explicit_array_size(base, t, m)) < 0)
            return -1;

        *nelemp = (size_t)n;
        return 0;

    case MRP_ARRAY_SIZE_GUARDED:
        if ((n = get_guarded_array_size(arrp, m)) < 0)
            return -1;

        *nelemp = (size_t)n;
        return 0;

    default:
        return -1;
    }
}


static int terminate_guarded_array(void *elem, mrp_native_array_t *m,
                                   mrp_native_type_t *mt)
{
    mrp_native_member_t *g;

    if (m->elem.id <= MRP_TYPE_STRING)
        memcpy(elem, &m->sentinel, mt->size);
    else if (m->elem.id > MRP_TYPE_STRUCT) {
        if ((g = native_member(mt, m->size.idx)) == NULL)
            return -1;

        memcpy(elem + g->any.offs, &m->sentinel, type_size(g->any.type));
    }

    return 0;
}


static int encode_array(mrp_tlv_t *tlv, void *arrp, mrp_native_array_t *m,
                        size_t nelem, size_t elem_size, mrp_typemap_t *idmap)
{
    mrp_native_type_t *t;
    mrp_value_t       *v;
    void              *elem;
    size_t             i;

    if (mrp_tlv_push_uint32(tlv, TAG_ARRAY, map_type(m->elem.id, idmap)) < 0)
        return -1;

    if (mrp_tlv_push_uint32(tlv, TAG_NELEM, nelem) < 0)
        return -1;

    if ((t = lookup_type(m->elem.id)) == NULL)
        return -1;

    for (i = 0, elem = arrp; i < nelem; i++, elem += elem_size) {
        v = elem;

        switch (t->id) {
        case MRP_TYPE_STRING:
            v = *(void **)elem;
        case MRP_TYPE_INT8:
        case MRP_TYPE_UINT8:
        case MRP_TYPE_INT16:
        case MRP_TYPE_UINT16:
        case MRP_TYPE_INT32:
        case MRP_TYPE_UINT32:
        case MRP_TYPE_INT64:
        case MRP_TYPE_UINT64:
        case MRP_TYPE_FLOAT:
        case MRP_TYPE_DOUBLE:
        case MRP_TYPE_BOOL:
        case MRP_TYPE_INT:
        case MRP_TYPE_UINT:
        case MRP_TYPE_SHORT:
        case MRP_TYPE_USHORT:
        case MRP_TYPE_SIZET:
        case MRP_TYPE_SSIZET:
            if (encode_basic(tlv, t->id, v) < 0)
                return -1;
            break;

        case MRP_TYPE_BLOB: /* XXX TODO implement blobs */
            return -1;

        case MRP_TYPE_ARRAY:
            return -1;

        default:
            /* an MRP_TYPE_STRUCT */
            if (encode_struct(tlv, elem, t, idmap) < 0)
                return -1;
            break;
        }
    }

    return 0;
}


static int encode_struct(mrp_tlv_t *tlv, void *data, mrp_native_type_t *t,
                         mrp_typemap_t *idmap)
{
    mrp_native_member_t *m;
    mrp_native_type_t   *mt;
    mrp_value_t         *v;
    uint32_t             idx;
    size_t               size, nelem;

    if (t == NULL)
        return -1;

    if (mrp_tlv_push_uint32(tlv, TAG_STRUCT, map_type(t->id, idmap)) < 0)
        return -1;

    for (idx = 0, m = t->members; idx < t->nmember; idx++, m++) {
        if (mrp_tlv_push_uint32(tlv, TAG_MEMBER, idx) < 0)
            return -1;

        if (m->any.layout == MRP_LAYOUT_INDIRECT)
            v = *(void **)(data + m->any.offs);
        else
            v = data + m->any.offs;

        switch (m->any.type) {
        case MRP_TYPE_INT8:
        case MRP_TYPE_UINT8:
        case MRP_TYPE_INT16:
        case MRP_TYPE_UINT16:
        case MRP_TYPE_INT32:
        case MRP_TYPE_UINT32:
        case MRP_TYPE_INT64:
        case MRP_TYPE_UINT64:
        case MRP_TYPE_FLOAT:
        case MRP_TYPE_DOUBLE:
        case MRP_TYPE_BOOL:
        case MRP_TYPE_STRING:
        case MRP_TYPE_INT:
        case MRP_TYPE_UINT:
        case MRP_TYPE_SHORT:
        case MRP_TYPE_USHORT:
        case MRP_TYPE_SIZET:
        case MRP_TYPE_SSIZET:
            if (encode_basic(tlv, m->any.type, v) < 0)
                return -1;
            break;

        case MRP_TYPE_BLOB: /* XXX TODO implement blobs */
            if (get_blob_size(data, t, &m->blob, &size) < 0)
                return -1;
            return -1;

        case MRP_TYPE_ARRAY:
            if (get_array_size(data, t, v->ptr, &m->array, &nelem, &size) < 0)
                return -1;
            if (encode_array(tlv, v->ptr, &m->array, nelem,
                             size, idmap) < 0)
                return -1;
            break;

        case MRP_TYPE_STRUCT:
            if ((mt = lookup_type(m->strct.data_type.id)) == NULL)
                return -1;
            if (encode_struct(tlv, v->ptr, mt, idmap) < 0)
                return -1;
            break;

        default:
            return -1;
        }
    }

    return 0;
}


int mrp_encode_native(void *data, uint32_t id, size_t reserve, void **bufp,
                      size_t *sizep, mrp_typemap_t *idmap)
{
    mrp_native_type_t *t = lookup_type(id);
    mrp_tlv_t          tlv;

    *bufp  = NULL;
    *sizep = 0;

    if (t == NULL)
        return -1;

    if (mrp_tlv_setup_write(&tlv, reserve + 4096) < 0)
        return -1;

    if (reserve > 0)
        if (mrp_tlv_reserve(&tlv, reserve, 1) == NULL)
            goto fail;

    if (encode_struct(&tlv, data, t, idmap) < 0)
        goto fail;

    mrp_tlv_trim(&tlv);
    mrp_tlv_steal(&tlv, bufp, sizep);

    return 0;

 fail:
    mrp_tlv_cleanup(&tlv);
    return -1;
}


static void *allocate_indirect(mrp_list_hook_t **chunks, mrp_value_t *v,
                               mrp_native_member_t *m, mrp_typemap_t *idmap)
{
    size_t size;

    switch (m->any.type) {
    case MRP_TYPE_INT8:
    case MRP_TYPE_UINT8:
        return (v->ptr = alloc_chunk(chunks, sizeof(int8_t)));
    case MRP_TYPE_INT16:
    case MRP_TYPE_UINT16:
        return (v->ptr = alloc_chunk(chunks, sizeof(int16_t)));
    case MRP_TYPE_INT32:
    case MRP_TYPE_UINT32:
        return (v->ptr = alloc_chunk(chunks, sizeof(int32_t)));
    case MRP_TYPE_INT64:
    case MRP_TYPE_UINT64:
        return (v->ptr = alloc_chunk(chunks, sizeof(int64_t)));
    case MRP_TYPE_FLOAT:
        return (v->ptr = alloc_chunk(chunks, sizeof(float)));
    case MRP_TYPE_DOUBLE:
        return (v->ptr = alloc_chunk(chunks, sizeof(double)));
    case MRP_TYPE_BOOL:
        return (v->ptr = alloc_chunk(chunks, sizeof(bool)));
    case MRP_TYPE_STRING:
        return v;                        /* will be allocated by TLV pull */
    case MRP_TYPE_BLOB:
        return v;                        /* will be allocated by decoder */
    case MRP_TYPE_ARRAY:
        return v;                        /* will be allocated by decoder */
    case MRP_TYPE_STRUCT:
        if ((size = type_size(mapped_type(m->strct.data_type.id, idmap))) == 0)
            return NULL;
        return (v->ptr = alloc_chunk(chunks, size));
    default:
        return NULL;
    }
}


static void *alloc_str_chunk(size_t size, void *chunksp)
{
    return alloc_chunk((mrp_list_hook_t **)chunksp, size);
}


static int decode_basic(mrp_tlv_t *tlv, mrp_list_hook_t **chunks,
                        mrp_type_t type, mrp_value_t *v)
{
    int32_t  i;
    uint32_t u;

    switch (type) {
    case MRP_TYPE_INT8:    return mrp_tlv_pull_int8  (tlv, TAG_NONE, &v->s8);
    case MRP_TYPE_UINT8:   return mrp_tlv_pull_uint8 (tlv, TAG_NONE, &v->u8);
    case MRP_TYPE_INT16:   return mrp_tlv_pull_int16 (tlv, TAG_NONE, &v->s16);
    case MRP_TYPE_UINT16:  return mrp_tlv_pull_uint16(tlv, TAG_NONE, &v->u16);
    case MRP_TYPE_INT32:   return mrp_tlv_pull_int32 (tlv, TAG_NONE, &v->s32);
    case MRP_TYPE_UINT32:  return mrp_tlv_pull_uint32(tlv, TAG_NONE, &v->u32);
    case MRP_TYPE_INT64:   return mrp_tlv_pull_int64 (tlv, TAG_NONE, &v->s64);
    case MRP_TYPE_UINT64:  return mrp_tlv_pull_uint64(tlv, TAG_NONE, &v->u64);
    case MRP_TYPE_FLOAT:   return mrp_tlv_pull_float (tlv, TAG_NONE, &v->flt);
    case MRP_TYPE_DOUBLE:  return mrp_tlv_pull_double(tlv, TAG_NONE, &v->dbl);
    case MRP_TYPE_BOOL:    return mrp_tlv_pull_bool  (tlv, TAG_NONE, &v->bln);
    case MRP_TYPE_STRING:
        return mrp_tlv_pull_string(tlv, TAG_NONE, &v->strp,
                                   -1, alloc_str_chunk, chunks);

    case MRP_TYPE_INT:
        if (mrp_tlv_pull_int32(tlv, TAG_NONE, &i) < 0)
            return -1;
        v->i = (int)i;
        return 0;

    case MRP_TYPE_UINT:
        if (mrp_tlv_pull_uint32(tlv, TAG_NONE, &u) < 0)
            return -1;
        v->ui = (unsigned int)u;
        return 0;

    case MRP_TYPE_SHORT:
        if (mrp_tlv_pull_int32(tlv, TAG_NONE, &i) < 0)
            return -1;
        v->si = (short)i;
        return 0;

    case MRP_TYPE_USHORT:
        if (mrp_tlv_pull_uint32(tlv, TAG_NONE, &u) < 0)
            return -1;
        v->usi = (unsigned short)u;
        return 0;

    case MRP_TYPE_SIZET:
        if (mrp_tlv_pull_uint32(tlv, TAG_NONE, &u) < 0)
            return -1;
        v->sz = (size_t)u;
        return 0;

    case MRP_TYPE_SSIZET:
        if (mrp_tlv_pull_int32(tlv, TAG_NONE, &i) < 0)
            return -1;
        v->ssz = (ssize_t)i;
        return 0;

    default:
        return -1;
    }
}


static int decode_array(mrp_tlv_t *tlv, mrp_list_hook_t **chunks,
                        void **arrp, mrp_native_array_t *m,
                        void *data, mrp_native_type_t *t,
                        mrp_typemap_t *idmap)
{
    mrp_native_type_t *mt;
    mrp_value_t       *v;
    void              *elem, *base;
    size_t             elem_size, i;
    uint32_t           id, nelem;
    int                n, guard;

    if (mrp_tlv_pull_uint32(tlv, TAG_ARRAY, &id) < 0)
        return -1;

    if ((id = mapped_type(id, idmap)) != m->elem.id)
        return -1;

    if ((elem_size = type_size(id)) == 0)
        return -1;

    if (mrp_tlv_pull_uint32(tlv, TAG_NELEM, &nelem) < 0)
        return -1;

    if ((mt = lookup_type(m->elem.id)) == NULL)
        return -1;

    switch (m->kind) {
    case MRP_ARRAY_SIZE_EXPLICIT:
        if ((n = get_explicit_array_size(data, t, m)) < 0)
            return -1;
        guard = 0;
        break;
    case MRP_ARRAY_SIZE_FIXED:
        n     = m->size.nelem;
        guard = 0;
        break;
    case MRP_ARRAY_SIZE_GUARDED:
        n     = nelem;
        guard = 1;
        break;
    default:
        return -1;
    }

    if (n != (int)nelem)
        return -1;

    switch (m->layout) {
    case MRP_LAYOUT_INLINED:
        base = (void *)arrp;
        break;
    case MRP_LAYOUT_INDIRECT:
    case MRP_LAYOUT_DEFAULT:
        if ((*arrp = alloc_chunk(chunks, (nelem + guard) * elem_size)) == NULL)
            return (nelem + guard) ? -1 : 0;
        base = *arrp;
        break;
    default:
        return -1;
    }

    for (i = 0, elem = base; i < nelem; i++, elem += elem_size) {
        v = elem;

        switch (mt->id) {
        case MRP_TYPE_INT8:
        case MRP_TYPE_UINT8:
        case MRP_TYPE_INT16:
        case MRP_TYPE_UINT16:
        case MRP_TYPE_INT32:
        case MRP_TYPE_UINT32:
        case MRP_TYPE_INT64:
        case MRP_TYPE_UINT64:
        case MRP_TYPE_FLOAT:
        case MRP_TYPE_DOUBLE:
        case MRP_TYPE_BOOL:
        case MRP_TYPE_STRING:
        case MRP_TYPE_INT:
        case MRP_TYPE_UINT:
        case MRP_TYPE_SHORT:
        case MRP_TYPE_USHORT:
        case MRP_TYPE_SIZET:
        case MRP_TYPE_SSIZET:
            if (decode_basic(tlv, chunks, mt->id, v) < 0)
                return -1;
            break;

        case MRP_TYPE_BLOB: /* XXX TODO implement blobs */
            return -1;

        case MRP_TYPE_ARRAY:
            return -1;

        default:
            /* an MRP_TYPE_STRUCT */
            if (decode_struct(tlv, chunks, &elem, &id, idmap) < 0)
                return -1;
        }
    }

    if (guard) {
        if (terminate_guarded_array(elem, m, mt) < 0)
            return -1;
    }

    return 0;
}


static int decode_struct(mrp_tlv_t *tlv, mrp_list_hook_t **chunks,
                         void **datap, uint32_t *idp, mrp_typemap_t *idmap)
{
    mrp_native_type_t   *t;
    mrp_native_member_t *m;
    mrp_value_t         *v;
    char                *str, **strp;
    size_t               max, i;
    uint32_t             idx, id;

    if (datap == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (mrp_tlv_pull_uint32(tlv, TAG_STRUCT, &id) < 0)
        return -1;
    else
        id = mapped_type(id, idmap);

    if (*idp) {
        if (*idp != id) {
            errno = EINVAL;
            return -1;
        }
    }
    else
        *idp = id;

    if ((t = lookup_type(id)) == NULL)
        return -1;

    if (*datap == NULL)
        if ((*datap = alloc_chunk(chunks, t->size)) == NULL)
            return -1;

    for (i = 0, m = t->members; i < t->nmember; i++, m++) {
        if (mrp_tlv_pull_uint32(tlv, TAG_MEMBER, &idx) < 0)
            return -1;

        v = *datap + m->any.offs;

        if (m->any.layout == MRP_LAYOUT_INDIRECT) {
            if ((v = allocate_indirect(chunks, v, m, idmap)) == NULL)
                return -1;
        }

        switch (m->any.type) {
        case MRP_TYPE_INT8:
        case MRP_TYPE_UINT8:
        case MRP_TYPE_INT16:
        case MRP_TYPE_UINT16:
        case MRP_TYPE_INT32:
        case MRP_TYPE_UINT32:
        case MRP_TYPE_INT64:
        case MRP_TYPE_UINT64:
        case MRP_TYPE_FLOAT:
        case MRP_TYPE_DOUBLE:
        case MRP_TYPE_BOOL:
        case MRP_TYPE_INT:
        case MRP_TYPE_UINT:
        case MRP_TYPE_SHORT:
        case MRP_TYPE_USHORT:
        case MRP_TYPE_SIZET:
        case MRP_TYPE_SSIZET:
            if (decode_basic(tlv, chunks, m->any.type, v) < 0)
                return -1;
            break;

        case MRP_TYPE_STRING:
            if (m->any.layout == MRP_LAYOUT_INLINED) {
                max  = m->str.size;
                str  = v->str;
                strp = &str;
            }
            else {
                max  = (size_t)-1;
                strp = &v->strp;
            }
            if (mrp_tlv_pull_string(tlv, TAG_NONE, strp, max,
                                    alloc_str_chunk, chunks) < 0)
                return -1;
            break;

        case MRP_TYPE_BLOB: /* XXX TODO implement blobs */
            return -1;

        case MRP_TYPE_ARRAY:
            if (decode_array(tlv, chunks, &v->ptr, &m->array,
                             *datap, t, idmap) < 0)
                return -1;
            break;

        case MRP_TYPE_STRUCT:
            id = m->strct.data_type.id;
            if (decode_struct(tlv, chunks, &v->ptr, &id, idmap) < 0)
                return -1;
            break;

        default:
            return -1;
        }
    }

    return 0;
}


int mrp_decode_native(void **bufp, size_t *sizep, void **datap, uint32_t *idp,
                      mrp_typemap_t *idmap)
{
    mrp_tlv_t        tlv;
    mrp_list_hook_t *chunks;
    void            *data;
    size_t           diff;

    chunks = NULL;
    data   = NULL;

    if (mrp_tlv_setup_read(&tlv, *bufp, *sizep) < 0)
        return -1;

    if (decode_struct(&tlv, &chunks, &data, idp, idmap) == 0) {
        diff = mrp_tlv_offset(&tlv);

        if (diff <= *sizep) {
            *bufp  += diff;
            *sizep -= diff;
            *datap  = data;

            return 0;
        }
    }

    free_chunks(chunks);

    return -1;
}


void mrp_free_native(void *data, uint32_t id)
{
    mrp_list_hook_t *chunks;

    MRP_UNUSED(id);

    if (data != NULL) {
        chunks = ((void *)data) - MRP_OFFSET(chunk_t, data);
        free_chunks(chunks);
    }
}


#define INDENT(_level, _fmt) "%*.*s"_fmt, _level * 4, _level * 4, ""

#define PRINT(_l, _p, _size, fmt, args...) do {                     \
        ssize_t _n;                                                 \
        _n = snprintf((_p), (_size), INDENT(_l, fmt), ## args);     \
        if (_n >= (ssize_t)(_size))                                 \
            return -1;                                              \
        (_p)    += _n;                                              \
        (_size) -= _n;                                              \
    } while (0)


static int print_basic(int level, char **bufp, size_t *sizep, int type,
                       const char *name, mrp_value_t *v)
{
#define NAME name ? name : "", name ? " = " : ""
    char    *p    = *bufp;
    size_t   size = *sizep;

    if (type >= MRP_TYPE_BLOB)
        return -1;

    switch (type) {
    case MRP_TYPE_INT8:
        PRINT(level, p, size, "%s%s%d\n", NAME, v->s8);
        break;
    case MRP_TYPE_UINT8:
        PRINT(level, p, size, "%s%s%u\n", NAME, v->u8);
        break;

    case MRP_TYPE_INT16:
        PRINT(level, p, size, "%s%s%d\n", NAME, v->s16);
        break;
    case MRP_TYPE_UINT16:
        PRINT(level, p, size, "%s%s%u\n", NAME, v->u16);
        break;

    case MRP_TYPE_INT32:
        PRINT(level, p, size, "%s%s%d\n", NAME, v->s32);
        break;
    case MRP_TYPE_UINT32:
        PRINT(level, p, size, "%s%s%u\n", NAME, v->u32);
        break;

    case MRP_TYPE_INT64:
        PRINT(level, p, size, "%s%s%lld\n", NAME, (long long)v->s64);
        break;
    case MRP_TYPE_UINT64:
        PRINT(level, p, size, "%s%s%llu\n", NAME,
              (unsigned long long)v->s64);
        break;

    case MRP_TYPE_FLOAT:
        PRINT(level, p, size, "%s%s%f\n", NAME, v->flt);
        break;
    case MRP_TYPE_DOUBLE:
        PRINT(level, p, size, "%s%s%f\n", NAME, v->dbl);
        break;

    case MRP_TYPE_BOOL:
        PRINT(level, p, size, "%s%s%s\n", NAME,
              v->bln ? "<true>" : "<false>");
        break;

    case MRP_TYPE_STRING:
        PRINT(level, p, size, "%s%s%s\n", NAME,
              v->str ? v->str : "<null>");
        break;

    case MRP_TYPE_INT:
        PRINT(level, p, size, "%s%s%d\n", NAME, v->i);
        break;
    case MRP_TYPE_UINT:
        PRINT(level, p, size, "%s%s%u\n", NAME, v->ui);
        break;

    case MRP_TYPE_SHORT:
        PRINT(level, p, size, "%s%s%hd\n", NAME, v->si);
        break;
    case MRP_TYPE_USHORT:
        PRINT(level, p, size, "%s%s%hu\n", NAME, v->usi);
        break;

    case MRP_TYPE_SIZET:
        PRINT(level, p, size, "%s%s%zu\n", NAME, v->sz);
        break;
    case MRP_TYPE_SSIZET:
        PRINT(level, p, size, "%s%s%zd\n", NAME, v->ssz);
        break;

    default:
        PRINT(level, p, size, "%s%s%s\n", NAME, "<unknown>");
    }

    *bufp  = p;
    *sizep = size;

    return 0;

#undef NAME
}


static int print_array(char **bufp, size_t *sizep, int level,
                       void *arrp, mrp_native_array_t *a, size_t nelem,
                       size_t elem_size)
{
    mrp_native_type_t *et;
    mrp_value_t       *v;
    void              *elem;
    size_t             i;
    char              *p;
    size_t             size;

    p    = *bufp;
    size = *sizep;

    if ((et = lookup_type(a->elem.id)) == NULL)
        return -1;

    PRINT(level, p, size, "%s = [%s", a->name, nelem == 0 ? "]" : "\n");
    level++;

    for (i = 0, elem = arrp; i < nelem; i++, elem += elem_size) {
        v = elem;

        switch (et->id) {
        case MRP_TYPE_STRING:
            v = *(void **)elem;
        case MRP_TYPE_INT8:
        case MRP_TYPE_UINT8:
        case MRP_TYPE_INT16:
        case MRP_TYPE_UINT16:
        case MRP_TYPE_INT32:
        case MRP_TYPE_UINT32:
        case MRP_TYPE_INT64:
        case MRP_TYPE_UINT64:
        case MRP_TYPE_FLOAT:
        case MRP_TYPE_DOUBLE:
        case MRP_TYPE_BOOL:
        case MRP_TYPE_INT:
        case MRP_TYPE_UINT:
        case MRP_TYPE_SHORT:
        case MRP_TYPE_USHORT:
        case MRP_TYPE_SIZET:
        case MRP_TYPE_SSIZET:
            if (print_basic(level, &p, &size, et->id, NULL, v) < 0)
                return -1;
            break;

        case MRP_TYPE_BLOB:
            PRINT(level, p, size, "<blob>\n");
            break;

        case MRP_TYPE_ARRAY:
            return -1;

        default:
            /* an MRP_TYPE_STRUCT */
            if (print_struct(&p, &size, level, elem, et) < 0)
                return -1;
            break;
        }
    }

    level--;
    PRINT(level, p, size, "%s\n", nelem == 0 ? "" : "]");

    *bufp  = p;
    *sizep = size;

    return 0;
}


static int print_struct(char **bufp, size_t *sizep, int level,
                        void *data, mrp_native_type_t *t)
{
    mrp_native_member_t *m;
    mrp_native_type_t   *mt;
    mrp_value_t         *v;
    uint32_t             idx;
    size_t               esize, nelem;
    char                *p;
    size_t               size;

    if (data == NULL) {
        **bufp = '\0';

        return 0;
    }

    if (t == NULL)
        return -1;

    p    = *bufp;
    size = *sizep;
    PRINT(level, p, size, "{\n");
    level++;

    for (idx = 0, m = t->members; idx < t->nmember; idx++, m++) {
        if (m->any.layout == MRP_LAYOUT_INDIRECT)
            v = *(void **)(data + m->any.offs);
        else
            v = data + m->any.offs;

        switch (m->any.type) {
        case MRP_TYPE_INT8:
        case MRP_TYPE_UINT8:
        case MRP_TYPE_INT16:
        case MRP_TYPE_UINT16:
        case MRP_TYPE_INT32:
        case MRP_TYPE_UINT32:
        case MRP_TYPE_INT64:
        case MRP_TYPE_UINT64:
        case MRP_TYPE_FLOAT:
        case MRP_TYPE_DOUBLE:
        case MRP_TYPE_BOOL:
        case MRP_TYPE_INT:
        case MRP_TYPE_UINT:
        case MRP_TYPE_SHORT:
        case MRP_TYPE_USHORT:
        case MRP_TYPE_SIZET:
        case MRP_TYPE_SSIZET:
        case MRP_TYPE_STRING:
            if (print_basic(level, &p, &size, m->any.type, m->any.name, v) < 0)
                return -1;
            break;

        case MRP_TYPE_BLOB: /* XXX TODO implement blobs */
            PRINT(level, p, size, "%s = <blob>\n", m->any.name);
            break;

        case MRP_TYPE_ARRAY:
            if (get_array_size(data, t, v->ptr, &m->array, &nelem, &esize) < 0)
                return -1;
            if (print_array(&p, &size, level, v->ptr, &m->array,
                            nelem, esize) < 0)
                return -1;
            break;

        case MRP_TYPE_STRUCT:
            if ((mt = lookup_type(m->strct.data_type.id)) == NULL)
                return -1;
            if (print_struct(&p, &size, level, v->ptr, mt) < 0)
                return -1;
            break;

        default:
            return -1;
        }
    }

    level--;
    PRINT(level, p, size, "}\n");

    *bufp  = p;
    *sizep = size;

    return 0;
}


ssize_t mrp_print_native(char *buf, size_t size, void *data, uint32_t id)
{
    mrp_native_type_t *t;
    char              *p;

    p = buf;

    if (id < MRP_TYPE_STRUCT || (t = lookup_type(id)) == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (print_struct(&p, &size, 0, data, t) == 0)
        return (ssize_t)(p - buf);
    else
        return -1;
}


static inline size_t chunk_size(size_t size)
{
    return MRP_OFFSET(chunk_t, data[size]);
}


static void *alloc_chunk(mrp_list_hook_t **chunks, size_t size)
{
    chunk_t *chunk;

    if (size == 0)
        return NULL;

    if (*chunks == NULL) {
        if ((*chunks = mrp_allocz(sizeof(*chunks))) == NULL)
            return NULL;
        else
            mrp_list_init(*chunks);
    }

    if ((chunk = mrp_allocz(chunk_size(size))) == NULL)
        return NULL;

    mrp_list_init(&chunk->hook);
    mrp_list_append(*chunks, &chunk->hook);

    return &chunk->data[0];
}


static void free_chunks(mrp_list_hook_t *chunks)
{
    mrp_list_hook_t *p, *n;

    if (chunks != NULL) {
        mrp_list_foreach(chunks, p, n) {
            mrp_list_delete(p);

            if (p != chunks)
                mrp_free(p);
        }

        mrp_free(chunks);
    }
}
