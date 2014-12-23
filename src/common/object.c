/*
 * Copyright (c) 2014, Intel Corporation
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

#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/debug.h>
#include <murphy/common/object.h>

#define MAKE_ID(tidx, eidx) ((((tidx)+1) << 16) | ((eidx)+1))
#define TYPE_ID(tidx)       (((tidx)+1) << 16)
#define TYPE_IDX(id)        (((id) >> 16)-1)
#define EXT_IDX(id)         (((id) & 0xffff)-1)
#define HAS_EXT(ext, eidx)  ((eidx) < (ext)->nmember)

static mrp_extensible_t *types;          /* registered extensible types */
static uint32_t          ntype;          /* number of extensible types */


static mrp_extensible_t *find_type(const char *type)
{
    mrp_extensible_t *t;
    uint32_t          i;

    for (i = 0, t = types; i < ntype; i++, t++)
        if (!strcmp(t->type, type))
            return t;

    return NULL;
}


static inline mrp_extensible_t *lookup_type(uint32_t id)
{
    uint32_t idx = TYPE_IDX(id);

    if (idx >=  ntype) {
        errno = EINVAL;
        return NULL;
    }

    return types + idx;
}


static mrp_extension_t *find_extension(mrp_extensible_t *t, const char *name)
{
    mrp_extension_t *ext;
    uint32_t         i;

    if (t == NULL)
        return NULL;

    for (i = 0, ext = t->extensions; i < t->nextension; i++, ext++) {
        if (!strcmp(ext->name, name))
            return ext;
    }

    return NULL;
}


static inline mrp_extension_t *lookup_extension(mrp_extensible_t *t, uint32_t id)
{
    uint32_t         idx = EXT_IDX(id);
    mrp_extension_t *ext;

    if (t == NULL || !t->size)
        return NULL;

    if (idx >= t->nextension) {
        errno = ENOENT;
        return NULL;
    }

    ext = t->extensions + idx;

    if (ext->id != id) {
        mrp_log_error("corrupt extension table: %s[#%u], 0x%x != 0x%x",
                      t->type, idx, ext->id, id);
        errno = EINVAL;
        return NULL;
    }

    return ext;
}


static inline void free_extension(void *obj, mrp_extensible_t *t, uint32_t id)
{
    mrp_extension_t *e   = lookup_extension(t, id);
    uint32_t         idx = EXT_IDX(id);
    mrp_extended_t  *ext = obj + t->offs;

    if (e == NULL)
        return;

    if (!HAS_EXT(ext, idx))
        return;

    if (e->free != NULL)
        e->free(obj, idx, ext->members[idx]);

    ext->members[idx] = NULL;
}


uint32_t mrp_extensible_register(const char *type, size_t size, size_t offs)
{
    mrp_extensible_t *t;
    uint32_t          id;

    t = find_type(type);

    if (t != NULL) {
        id = TYPE_ID(t - types);

        if (t->size == size && t->offs == offs)
            return id;

        if (!t->size && !t->offs) {
            t->size = size;
            t->offs = offs;
            goto registered;
        }

        errno = EINVAL;
        mrp_log_error("type '%s' registered extensibe and incompatible", type);
        return MRP_EXTENSIBLE_NONE;
    }

    if (!mrp_reallocz(types, ntype, ntype + 1))
        return MRP_EXTENSIBLE_NONE;

    t = types + ntype;
    t->type = mrp_strdup(type);
    t->size = size;
    t->offs = offs;

    if (t->type == NULL) {
        mrp_reallocz(types, ntype + 1, ntype);
        errno = ENOMEM;
        return MRP_EXTENSIBLE_NONE;
    }

    ntype++;
    id = TYPE_ID(t - types);

    if (size && offs) {
    registered:
        mrp_debug("type '%s' registered for extensions", type);
    }
    else
        mrp_debug("type '%s' forward-declared for extensions", type);

    return id;
}


uint32_t _mrp_extensible_id(const char *type)
{
    mrp_extensible_t *t = find_type(type);

    if (t == NULL)
        return MRP_EXTENSIBLE_NONE;
    else
        return TYPE_ID(t - types);
}


int mrp_extensible_init(void *obj, uint32_t id)
{
    mrp_extensible_t *t = lookup_type(id);
    mrp_extended_t   *ext;

    if (t == NULL) {
        errno = EINVAL;
        return -1;
    }

    ext = obj + t->offs;
    ext->id = id;

    return 0;
}


void mrp_extensible_cleanup(void *obj, uint32_t id)
{
    mrp_extension_free_all(obj, id);
}


static inline int extensible_check(void *obj, uint32_t id)
{
    mrp_extensible_t *t = lookup_type(id);
    mrp_extended_t   *ext;

    if (t == NULL) {
    invalid:
        errno = EINVAL;
        return -1;
    }

    ext = obj + t->offs;

    if (ext->id != id)
        goto invalid;

    return 0;
}


int mrp_extensible_check(void *obj, uint32_t id)
{
    return extensible_check(obj, id);
}


bool _mrp_extensible_of_type(void *obj, const char *type)
{
    uint32_t id = _mrp_extensible_id(type);

    if (id == MRP_EXTENSIBLE_NONE)
        return false;

    return mrp_extensible_check(obj, id) < 0 ? false : true;
}


uint32_t mrp_extension_register(const char *obj_type, const char *ext_type,
                                const char *ext_name, mrp_extfree_t free)
{
    mrp_extensible_t *t = find_type(obj_type);
    mrp_extension_t  *e = find_extension(t, ext_name);
    uint32_t          oidx, eidx, id;

    if (t == NULL) {
        mrp_extensible_declare(obj_type);
        t = find_type(obj_type);
    }

    if (t == NULL)
        return MRP_EXTENSION_NONE;

    if (e != NULL) {
        if ((!e->type && e->type) || (e->type && !ext_type))
            goto incompatible;
        if (e->type && ext_type && strcmp(e->type, ext_type))
            goto incompatible;
        if (e->free != free) {
        incompatible:
            mrp_log_error("type '%s' already has incompatible extension %s",
                          obj_type, ext_name);
            return MRP_EXTENSION_NONE;
        }
        return e->id;
    }

    if (!mrp_reallocz(t->extensions, t->nextension, t->nextension + 1))
        return MRP_EXTENSION_NONE;

    e = t->extensions + t->nextension;

    e->type = mrp_strdup(ext_type);
    e->name = mrp_strdup(ext_name);
    e->free = free;

    if (e->type == NULL || e->name == NULL) {
        mrp_free((void *)e->type);
        mrp_free((void *)e->name);
        mrp_reallocz(t->extensions, t->nextension + 1, t->nextension);

        return MRP_EXTENSION_NONE;
    }

    e->type_check = 1;

    oidx = t - types;
    eidx = t->nextension++;
    id   = e->id = MAKE_ID(oidx, eidx);

    mrp_debug("type %s extended by %s %s (0x%x)", obj_type,
              ext_type, ext_name, id);

    return id;
}


int mrp_extension_typecheck(uint32_t id, bool enable)
{
    mrp_extensible_t *t = lookup_type(id);
    mrp_extension_t  *e = lookup_extension(t, id);

    if (e == NULL)
        return -1;

    e->type_check = !!enable;

    return 0;
}


int _mrp_extension_set(void *obj, uint32_t id, const char *type, void *value)
{
    mrp_extensible_t *t   = lookup_type(id);
    mrp_extension_t  *e   = lookup_extension(t, id);
    uint32_t          idx = EXT_IDX(id);
    mrp_extended_t   *ext;

    if (e == NULL) {
        mrp_log_error("can't set unknown extension 0x%x for object %p", id, obj);
        return -1;
    }

    if (type != NULL && e->type_check && strcmp(e->type, type)) {
        mrp_log_error("%s.%s: extension type error, registered: %s, got: %s.",
                      t->type, e->name, e->type, type);
        errno = EINVAL;
        return -1;
    }

    ext = obj + t->offs;

    if (ext->nmember < idx + 1) {
        if (!mrp_reallocz(ext->members, ext->nmember, idx + 1))
            return -1;
        ext->nmember = idx + 1;
    }
    else {
        if (e->free && ext->members[idx])
            e->free(obj, id, ext->members[idx]);
    }
    ext->members[idx] = value;

    return 0;
}


void *_mrp_extension_get(void *obj, uint32_t id, const char *type)
{
    mrp_extensible_t *t   = lookup_type(id);
    mrp_extension_t  *e   = lookup_extension(t, id);
    uint32_t          idx = EXT_IDX(id);
    mrp_extended_t   *ext;

    if (e == NULL)
        return NULL;

    if (type != NULL && e->type_check && strcmp(e->type, type)) {
        mrp_log_error("%s.%s: extension type error, registered: %s, get: %s.",
                      t->type, e->name, e->type, type);
        errno = EINVAL;
        return NULL;
    }

    ext = obj + t->offs;

    if (!HAS_EXT(ext, idx))
        return NULL;

    return ext->members[idx];
}


void mrp_extension_free(void *obj, uint32_t id)
{
    free_extension(obj, lookup_type(id), id);
}


void mrp_extension_free_all(void *obj, uint32_t id)
{
    mrp_extensible_t *t = lookup_type(id);
    mrp_extended_t   *ext;
    uint32_t          i;

    if (t == NULL)
        return;

    ext = obj + t->offs;

    for (i = 0; i < t->nextension; i++) {
        if (t->extensions[i].free != NULL)
            t->extensions[i].free(obj, t->extensions[i].id, ext->members[i]);

        ext->members[i] = NULL;
    }
}
