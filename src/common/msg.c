/*
 * Copyright (c) 2012, Intel Corporation
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

#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <arpa/inet.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/list.h>
#include <murphy/common/msg.h>

#define NDIRECT_TYPE      256            /* directly indexed types */

static mrp_data_descr_t **direct_types;  /* directly indexed types */
static mrp_data_descr_t **other_types;   /* linearly searched types */
static int                nother_type;


static inline void destroy_field(mrp_msg_field_t *f)
{
    uint32_t i;

    if (f != NULL) {
        mrp_list_delete(&f->hook);

        switch (f->type) {
        case MRP_MSG_FIELD_STRING:
            mrp_free(f->str);
            break;

        case MRP_MSG_FIELD_BLOB:
            mrp_free(f->blb);
            break;

        default:
            if (f->type & MRP_MSG_FIELD_ARRAY) {
                if ((f->type & ~MRP_MSG_FIELD_ARRAY) == MRP_MSG_FIELD_STRING) {
                    for (i = 0; i < f->size[0]; i++) {
                        mrp_free(f->astr[i]);
                    }
                }

                mrp_free(f->aany);
            }
            break;
        }

        mrp_free(f);
    }
}


static inline mrp_msg_field_t *create_field(uint16_t tag, va_list *ap)
{
    mrp_msg_field_t *f;
    uint16_t         type, base;
    uint32_t         size;
    void            *blb;

    type = va_arg(*ap, uint32_t);

#define CREATE(_f, _tag, _type, _fldtype, _fld, _last, _errlbl) do {      \
                                                                          \
            (_f) = mrp_allocz(MRP_OFFSET(typeof(*_f), _last) +            \
                              sizeof(_f->_last));                         \
                                                                          \
            if ((_f) != NULL) {                                           \
                mrp_list_init(&(_f)->hook);                             \
                (_f)->tag  = _tag;                                        \
                (_f)->type = _type;                                       \
                (_f)->_fld = va_arg(*ap, _fldtype);                       \
            }                                                             \
            else {                                                        \
                goto _errlbl;                                             \
            }                                                             \
        } while (0)

#define CREATE_ARRAY(_f, _tag, _type, _fld, _fldtype, _errlbl) do {       \
            uint16_t _base;                                               \
            uint32_t _i;                                                  \
                                                                          \
            (_f) = mrp_allocz(MRP_OFFSET(typeof(*_f), size[1]));          \
                                                                          \
            if ((_f) != NULL) {                                           \
                mrp_list_init(&(_f)->hook);                               \
                (_f)->tag  = _tag;                                        \
                (_f)->type = _type | MRP_MSG_FIELD_ARRAY;                 \
                _base      = _type & ~MRP_MSG_FIELD_ARRAY;                \
                                                                          \
                _f->size[0] = va_arg(*ap, uint32_t);                      \
                _f->_fld    = mrp_allocz_array(typeof(*_f->_fld),         \
                                               _f->size[0]);              \
                                                                          \
                if (_f->_fld == NULL && _f->size[0] != 0)                 \
                    goto _errlbl;                                         \
                else                                                      \
                    memcpy(_f->_fld, va_arg(*ap, typeof(_f->_fld)),       \
                           _f->size[0] * sizeof(_f->_fld[0]));            \
                                                                          \
                if (_base == MRP_MSG_FIELD_STRING) {                      \
                    for (_i = 0; _i < _f->size[0]; _i++) {                \
                        _f->astr[_i] = mrp_strdup(_f->astr[_i]);          \
                        if (_f->astr[_i] == NULL)                         \
                            goto _errlbl;                                 \
                    }                                                     \
                }                                                         \
            }                                                             \
            else                                                          \
                goto _errlbl;                                             \
        } while (0)

    f = NULL;

    switch (type) {
    case MRP_MSG_FIELD_STRING:
        CREATE(f, tag, type, char *, str, str, fail);
        f->str = mrp_strdup(f->str);
        if (f->str == NULL)
            goto fail;
        break;
    case MRP_MSG_FIELD_BOOL:
        CREATE(f, tag, type, int, bln, bln, fail);
        break;
    case MRP_MSG_FIELD_UINT8:
        CREATE(f, tag, type, unsigned int, u8, u8, fail);
        break;
    case MRP_MSG_FIELD_SINT8:
        CREATE(f, tag, type, signed int, s8, s8, fail);
        break;
    case MRP_MSG_FIELD_UINT16:
        CREATE(f, tag, type, unsigned int, u16, u16, fail);
        break;
    case MRP_MSG_FIELD_SINT16:
        CREATE(f, tag, type, signed int, s16, s16, fail);
        break;
    case MRP_MSG_FIELD_UINT32:
        CREATE(f, tag, type, unsigned int, u32, u32, fail);
        break;
    case MRP_MSG_FIELD_SINT32:
        CREATE(f, tag, type, signed int, s32, s32, fail);
        break;
    case MRP_MSG_FIELD_UINT64:
        CREATE(f, tag, type, uint64_t, u64, u64, fail);
        break;
    case MRP_MSG_FIELD_SINT64:
        CREATE(f, tag, type, int64_t, s64, s64, fail);
        break;
    case MRP_MSG_FIELD_DOUBLE:
        CREATE(f, tag, type, double, dbl, dbl, fail);
        break;

    case MRP_MSG_FIELD_BLOB:
        size = va_arg(*ap, uint32_t);
        CREATE(f, tag, type, void *, blb, size[0], fail);

        blb        = f->blb;
        f->size[0] = size;
        f->blb     = mrp_allocz(size);

        if (f->blb != NULL) {
            memcpy(f->blb, blb, size);
            f->size[0] = size;
        }
        else
            goto fail;
        break;

    default:
        if (!(type & MRP_MSG_FIELD_ARRAY)) {
            errno = EINVAL;
            goto fail;
        }

        base = type & ~MRP_MSG_FIELD_ARRAY;

        switch (base) {
        case MRP_MSG_FIELD_STRING:
            CREATE_ARRAY(f, tag, base, astr, char *, fail);
            break;
        case MRP_MSG_FIELD_BOOL:
            CREATE_ARRAY(f, tag, base, abln, int, fail);
            break;
        case MRP_MSG_FIELD_UINT8:
            CREATE_ARRAY(f, tag, base, au8, unsigned int, fail);
            break;
        case MRP_MSG_FIELD_SINT8:
            CREATE_ARRAY(f, tag, base, as8, int, fail);
            break;
        case MRP_MSG_FIELD_UINT16:
            CREATE_ARRAY(f, tag, base, au16, unsigned int, fail);
            break;
        case MRP_MSG_FIELD_SINT16:
            CREATE_ARRAY(f, tag, base, as16, int, fail);
            break;
        case MRP_MSG_FIELD_UINT32:
            CREATE_ARRAY(f, tag, base, au32, unsigned int, fail);
            break;
        case MRP_MSG_FIELD_SINT32:
            CREATE_ARRAY(f, tag, base, as32, int, fail);
            break;
        case MRP_MSG_FIELD_UINT64:
            CREATE_ARRAY(f, tag, base, au64, unsigned long long, fail);
            break;
        case MRP_MSG_FIELD_SINT64:
            CREATE_ARRAY(f, tag, base, as64, long long, fail);
            break;
        case MRP_MSG_FIELD_DOUBLE:
            CREATE_ARRAY(f, tag, base, adbl, double, fail);
            break;
        default:
            errno = EINVAL;
            goto fail;
        }
        break;
    }

    return f;

 fail:
    destroy_field(f);
    return NULL;

#undef CREATE
#undef CREATE_ARRAY
}


static void msg_destroy(mrp_msg_t *msg)
{
    mrp_list_hook_t *p, *n;
    mrp_msg_field_t *f;

    if (msg != NULL) {
        mrp_list_foreach(&msg->fields, p, n) {
            f = mrp_list_entry(p, typeof(*f), hook);
            destroy_field(f);
        }

        mrp_free(msg);
    }
}


mrp_msg_t *mrp_msg_createv(uint16_t tag, va_list ap)
{
    mrp_msg_t       *msg;
    mrp_msg_field_t *f;
    va_list          aq;

    va_copy(aq, ap);
    if ((msg = mrp_allocz(sizeof(*msg))) != NULL) {
        mrp_list_init(&msg->fields);
        mrp_refcnt_init(&msg->refcnt);

        while (tag != MRP_MSG_FIELD_INVALID) {
            f = create_field(tag, &aq);

            if (f != NULL) {
                mrp_list_append(&msg->fields, &f->hook);
                msg->nfield++;
            }
            else {
                msg_destroy(msg);
                msg = NULL;
                goto out;
            }
            tag = va_arg(aq, uint32_t);
        }
    }
 out:
    va_end(aq);

    return msg;
}


mrp_msg_t *mrp_msg_create(uint16_t tag, ...)
{
    mrp_msg_t *msg;
    va_list    ap;

    va_start(ap, tag);
    msg = mrp_msg_createv(tag, ap);
    va_end(ap);

    return msg;
}


mrp_msg_t *mrp_msg_ref(mrp_msg_t *msg)
{
    return mrp_ref_obj(msg, refcnt);
}


void mrp_msg_unref(mrp_msg_t *msg)
{
    if (mrp_unref_obj(msg, refcnt))
            msg_destroy(msg);
}


int mrp_msg_append(mrp_msg_t *msg, uint16_t tag, ...)
{
    mrp_msg_field_t *f;
    va_list          ap;

    va_start(ap, tag);
    f = create_field(tag, &ap);
    va_end(ap);

    if (f != NULL) {
        mrp_list_append(&msg->fields, &f->hook);
        msg->nfield++;
        return TRUE;
    }
    else
        return FALSE;
}


int mrp_msg_prepend(mrp_msg_t *msg, uint16_t tag, ...)
{
    mrp_msg_field_t *f;
    va_list          ap;

    va_start(ap, tag);
    f = create_field(tag, &ap);
    va_end(ap);

    if (f != NULL) {
        mrp_list_prepend(&msg->fields, &f->hook);
        msg->nfield++;
        return TRUE;
    }
    else
        return FALSE;
}


int mrp_msg_set(mrp_msg_t *msg, uint16_t tag, ...)
{
    mrp_msg_field_t *of, *nf;
    va_list          ap;

    of = mrp_msg_find(msg, tag);

    if (of != NULL) {
        va_start(ap, tag);
        nf = create_field(tag, &ap);
        va_end(ap);

        if (nf != NULL) {
            mrp_list_append(&of->hook, &nf->hook);
            destroy_field(of);

            return TRUE;
        }
    }

    return FALSE;
}


int mrp_msg_iterate(mrp_msg_t *msg, void **it, uint16_t *tagp, uint16_t *typep,
                    mrp_msg_value_t *valp, size_t *sizep)
{
    mrp_list_hook_t *p = *(mrp_list_hook_t **)it;
    mrp_msg_field_t *f;

    if (p == NULL)
        p = msg->fields.next;

    if (p == &msg->fields)
        return FALSE;

    f = mrp_list_entry(p, typeof(*f), hook);

    *tagp  = f->tag;
    *typep = f->type;

    switch (f->type) {
#define HANDLE_TYPE(type, member)                       \
        case MRP_MSG_FIELD_##type:                      \
            valp->member = f->member;                   \
            if (sizep != NULL)                          \
                *sizep = sizeof(typeof(f->member));     \
            break

        HANDLE_TYPE(BOOL  , bln);
        HANDLE_TYPE(UINT8 , u8);
        HANDLE_TYPE(SINT8 , s8);
        HANDLE_TYPE(UINT16, u16);
        HANDLE_TYPE(SINT16, s16);
        HANDLE_TYPE(UINT32, u32);
        HANDLE_TYPE(SINT32, s32);
        HANDLE_TYPE(UINT64, u64);
        HANDLE_TYPE(SINT64, s64);
        HANDLE_TYPE(DOUBLE, dbl);

    case MRP_MSG_FIELD_STRING:
        valp->str = f->str;
        if (sizep != NULL)
            *sizep = strlen(f->str);
        break;

    case MRP_MSG_FIELD_BLOB:
        valp->blb = f->blb;
        if (sizep != NULL)
            *sizep = (size_t)f->size[0];
        break;

    default:
        if (f->type & MRP_MSG_FIELD_ARRAY) {
            valp->aany = f->aany;
            if (sizep != NULL)
                *sizep = f->size[0];
        }
        else
            return FALSE;
#undef HANDLE_TYPE
    }

    *it = p->next;

    return TRUE;
}


mrp_msg_field_t *mrp_msg_find(mrp_msg_t *msg, uint16_t tag)
{
    mrp_msg_field_t *f;
    mrp_list_hook_t *p, *n;

    mrp_list_foreach(&msg->fields, p, n) {
        f = mrp_list_entry(p, typeof(*f), hook);
        if (f->tag == tag)
            return f;
    }

    return NULL;
}


int mrp_msg_get(mrp_msg_t *msg, ...)
{
#define HANDLE_TYPE(_type, _member)                       \
    case MRP_MSG_FIELD_##_type:                           \
        valp          = va_arg(ap, typeof(valp));         \
        valp->_member = f->_member;                       \
        break

#define HANDLE_ARRAY(_type, _member)                      \
    case MRP_MSG_FIELD_##_type:                           \
        cntp          = va_arg(ap, typeof(cntp));         \
        valp          = va_arg(ap, typeof(valp));         \
        *cntp         = f->size[0];                       \
        valp->_member = f->_member;                       \
        break


    mrp_msg_field_t *f;
    mrp_msg_value_t *valp;
    uint32_t        *cntp;
    mrp_list_hook_t *start, *p;
    uint16_t         tag, type;
    int              found;
    va_list          ap;

    va_start(ap, msg);

    /*
     * Okay... this might look a bit weird at first sight. This is
     * mostly because we don't use the standard list iterating macros
     * in the inner loop. There is a good reason for that: we want to
     * minimise the number of times we scan the message which is just
     * a linked list of fields. We do this by arranging the nested
     * loops below in such a way that if the order of fields to fetch
     * in the argument list matches the order of fields in the message
     * we end up running the outer and inner loops in a 'phase lock'.
     * So if the caller fetches the fields in the correct order we end
     * up scanning the message at most once but only up to the last
     * field to fetch.
     */

    start = msg->fields.next;
    found = FALSE;

    while ((tag = va_arg(ap, unsigned int)) != MRP_MSG_FIELD_INVALID) {
        type  = va_arg(ap, unsigned int);
        found = FALSE;

        for (p = start; p != start->prev; p = p->next) {
            if (p == &msg->fields)
                continue;

            f = mrp_list_entry(p, typeof(*f), hook);

            if (f->tag != tag)
                continue;

            if (f->type != type)
                goto out;

            switch (type) {
                HANDLE_TYPE(STRING, str);
                HANDLE_TYPE(BOOL  , bln);
                HANDLE_TYPE(UINT8 , u8 );
                HANDLE_TYPE(SINT8 , s8 );
                HANDLE_TYPE(UINT16, u16);
                HANDLE_TYPE(SINT16, s16);
                HANDLE_TYPE(UINT32, u32);
                HANDLE_TYPE(SINT32, s32);
                HANDLE_TYPE(UINT64, u64);
                HANDLE_TYPE(SINT64, s64);
                HANDLE_TYPE(DOUBLE, dbl);
            default:
                if (type & MRP_MSG_FIELD_ARRAY) {
                    switch (type & ~MRP_MSG_FIELD_ARRAY) {
                        HANDLE_ARRAY(STRING, astr);
                        HANDLE_ARRAY(BOOL  , abln);
                        HANDLE_ARRAY(UINT8 , au8 );
                        HANDLE_ARRAY(SINT8 , as8 );
                        HANDLE_ARRAY(UINT16, au16);
                        HANDLE_ARRAY(SINT16, as16);
                        HANDLE_ARRAY(UINT32, au32);
                        HANDLE_ARRAY(SINT32, as32);
                        HANDLE_ARRAY(UINT64, au64);
                        HANDLE_ARRAY(SINT64, as64);
                        HANDLE_ARRAY(DOUBLE, adbl);
                    default:
                        goto out;

                    }
                }
                else
                    goto out;
            }

            start = p->next;
            found = TRUE;
            break;
        }

        if (!found)
            break;
    }

 out:
    va_end(ap);

    return found;

#undef HANDLE_TYPE
#undef HANDLE_ARRAY

}


int mrp_msg_iterate_get(mrp_msg_t *msg, void **it, ...)
{
#define HANDLE_TYPE(_type, _member)                       \
    case MRP_MSG_FIELD_##_type:                           \
        valp          = va_arg(ap, typeof(valp));         \
        valp->_member = f->_member;                       \
        break

#define HANDLE_ARRAY(_type, _member)                      \
    case MRP_MSG_FIELD_##_type:                           \
        cntp          = va_arg(ap, typeof(cntp));         \
        valp          = va_arg(ap, typeof(valp));         \
        *cntp         = f->size[0];                       \
        valp->_member = f->_member;                       \
        break

#define ANY_TYPE(_type, _member)                \
    case MRP_MSG_FIELD_##_type:                 \
        valp->_member = f->_member;             \
        break

    mrp_msg_field_t *f;
    mrp_msg_value_t *valp;
    uint32_t        *cntp;
    mrp_list_hook_t *start, *p;
    uint16_t         tag, type, *typep;
    int              found;
    va_list          ap;

    va_start(ap, it);

    /*
     * Okay... this might look a bit weird at first sight. This is
     * mostly because we don't use the standard list iterating macros
     * in the inner loop. There is a good reason for that: we want to
     * minimise the number of times we scan the message which is just
     * a linked list of fields. We do this by arranging the nested
     * loops below in such a way that if the order of fields to fetch
     * in the argument list matches the order of fields in the message
     * we end up running the outer and inner loops in a 'phase lock'.
     * So if the caller fetches the fields in the correct order we end
     * up scanning the message at most once but only up to the last
     * field to fetch.
     */

    start = (*it) ? (mrp_list_hook_t *)*it : msg->fields.next;
    found = FALSE;

    while ((tag = va_arg(ap, unsigned int)) != MRP_MSG_FIELD_INVALID) {
        type  = va_arg(ap, unsigned int);
        found = FALSE;

        if (type == MRP_MSG_FIELD_ANY) {
            typep = va_arg(ap, uint16_t *);
            valp  = va_arg(ap, mrp_msg_value_t *);
        }
        else {
            typep = NULL;
            valp  = NULL;
        }

        for (p = start; p != start->prev; p = p->next) {
            if (p == &msg->fields)
                continue;

            f = mrp_list_entry(p, typeof(*f), hook);

            if (f->tag != tag)
                continue;

            if (type == MRP_MSG_FIELD_ANY) {
                *typep = f->type;
                switch (f->type) {
                ANY_TYPE(STRING, str);
                ANY_TYPE(BOOL  , bln);
                ANY_TYPE(UINT8 , u8 );
                ANY_TYPE(SINT8 , s8 );
                ANY_TYPE(UINT16, u16);
                ANY_TYPE(SINT16, s16);
                ANY_TYPE(UINT32, u32);
                ANY_TYPE(SINT32, s32);
                ANY_TYPE(UINT64, u64);
                ANY_TYPE(SINT64, s64);
                ANY_TYPE(DOUBLE, dbl);
                default:
                    mrp_log_error("XXX TODO: currently cannot fetch array "
                                  "message fields with iterators.");
                }

                goto next;
            }

            if (f->type != type)
                goto out;

            switch (type) {
                HANDLE_TYPE(STRING, str);
                HANDLE_TYPE(BOOL  , bln);
                HANDLE_TYPE(UINT8 , u8 );
                HANDLE_TYPE(SINT8 , s8 );
                HANDLE_TYPE(UINT16, u16);
                HANDLE_TYPE(SINT16, s16);
                HANDLE_TYPE(UINT32, u32);
                HANDLE_TYPE(SINT32, s32);
                HANDLE_TYPE(UINT64, u64);
                HANDLE_TYPE(SINT64, s64);
                HANDLE_TYPE(DOUBLE, dbl);
            default:
                if (type & MRP_MSG_FIELD_ARRAY) {
                    switch (type & ~MRP_MSG_FIELD_ARRAY) {
                        HANDLE_ARRAY(STRING, astr);
                        HANDLE_ARRAY(BOOL  , abln);
                        HANDLE_ARRAY(UINT8 , au8 );
                        HANDLE_ARRAY(SINT8 , as8 );
                        HANDLE_ARRAY(UINT16, au16);
                        HANDLE_ARRAY(SINT16, as16);
                        HANDLE_ARRAY(UINT32, au32);
                        HANDLE_ARRAY(SINT32, as32);
                        HANDLE_ARRAY(UINT64, au64);
                        HANDLE_ARRAY(SINT64, as64);
                        HANDLE_ARRAY(DOUBLE, adbl);
                    default:
                        goto out;

                    }
                }
                else
                    goto out;
            }

        next:
            start = p->next;
            found = TRUE;
            break;
        }

        if (!found)
            break;
    }

 out:
    va_end(ap);

    if (found)
        *it = start;

    return found;

#undef HANDLE_TYPE
#undef HANDLE_ARRAY

}


static const char *field_type_name(uint16_t type)
{
#define BASIC(t, n) [MRP_MSG_FIELD_##t] = n
#define ARRAY(t, n) [MRP_MSG_FIELD_##t] = "array of "n"s"
    static const char *basic[] = {
        BASIC(STRING, "string" ),
        BASIC(BOOL  , "boolean"),
        BASIC(UINT8 , "uint8"  ),
        BASIC(SINT8 , "sint8"  ),
        BASIC(UINT16, "uint16" ),
        BASIC(SINT16, "sint16" ),
        BASIC(UINT32, "uint32" ),
        BASIC(SINT32, "sint32" ),
        BASIC(UINT64, "uint64" ),
        BASIC(SINT64, "sint64" ),
        BASIC(DOUBLE, "double" ),
        BASIC(BLOB  , "blob"   )
    };

    static const char *array[] = {
        ARRAY(STRING, "string" ),
        ARRAY(BOOL  , "boolean"),
        ARRAY(UINT8 , "uint8"  ),
        ARRAY(SINT8 , "sint8"  ),
        ARRAY(UINT16, "uint16" ),
        ARRAY(SINT16, "sint16" ),
        ARRAY(UINT32, "uint32" ),
        ARRAY(SINT32, "sint32" ),
        ARRAY(UINT64, "uint64" ),
        ARRAY(SINT64, "sint64" ),
        ARRAY(DOUBLE, "double" ),
        ARRAY(BLOB  , "blob"   )
    };
#undef BASIC
#undef ARRAY

    uint16_t base;

    if (MRP_MSG_FIELD_INVALID < type && type <= MRP_MSG_FIELD_MAX)
        return basic[type];
    else {
        if (type & MRP_MSG_FIELD_ARRAY) {
            base = type & ~MRP_MSG_FIELD_ARRAY;

            if (MRP_MSG_FIELD_INVALID < base && base <= MRP_MSG_FIELD_MAX)
                return array[base];
        }
    }

    return "unknown type";
}


int mrp_msg_dump(mrp_msg_t *msg, FILE *fp)
{
    mrp_msg_field_t *f;
    mrp_list_hook_t *p, *n;
    int              l;
    uint32_t         i;
    uint16_t         base;
    const char      *tname;

    if (msg == NULL)
        return fprintf(fp, "{\n    <no message>\n}\n");

    l = fprintf(fp, "{\n");
    mrp_list_foreach(&msg->fields, p, n) {
        f = mrp_list_entry(p, typeof(*f), hook);

        l += fprintf(fp, "    0x%x ", f->tag);

#define DUMP(_indent, _fmt, _typename, _val)                              \
        l += fprintf(fp, "%*.*s= <%s> "_fmt"\n", _indent, _indent, "",    \
                     _typename, _val)

        tname = field_type_name(f->type);
        switch (f->type) {
        case MRP_MSG_FIELD_STRING:
            DUMP(0, "'%s'", tname, f->str);
            break;
        case MRP_MSG_FIELD_BOOL:
            DUMP(0, "%s", tname, f->bln ? "true" : "false");
            break;
        case MRP_MSG_FIELD_UINT8:
            DUMP(0, "%u", tname, f->u8);
            break;
        case MRP_MSG_FIELD_SINT8:
            DUMP(0, "%d", tname, f->s8);
            break;
        case MRP_MSG_FIELD_UINT16:
            DUMP(0, "%u", tname, f->u16);
            break;
        case MRP_MSG_FIELD_SINT16:
            DUMP(0, "%d", tname, f->s16);
            break;
        case MRP_MSG_FIELD_UINT32:
            DUMP(0, "%u", tname, f->u32);
            break;
        case MRP_MSG_FIELD_SINT32:
            DUMP(0, "%d", tname, f->s32);
            break;
        case MRP_MSG_FIELD_UINT64:
            DUMP(0, "%Lu", tname, (long long unsigned)f->u64);
            break;
        case MRP_MSG_FIELD_SINT64:
            DUMP(0, "%Ld", tname, (long long signed)f->s64);
            break;
        case MRP_MSG_FIELD_DOUBLE:
            DUMP(0, "%f", tname, f->dbl);
            break;
        case MRP_MSG_FIELD_BLOB: {
            char     *p;
            uint32_t  i;

            fprintf(fp, "= <%s> <%u bytes, ", tname, f->size[0]);

            for (i = 0, p = f->blb; i < f->size[0]; i++, p++) {
                if (isprint(*p) && *p != '\n' && *p != '\t' && *p != '\r')
                    fprintf(fp, "%c", *p);
                else
                    fprintf(fp, ".");
            }
            fprintf(fp, ">\n");
        }
            break;

        default:
            if (f->type & MRP_MSG_FIELD_ARRAY) {
                base  = f->type & ~MRP_MSG_FIELD_ARRAY;
                tname = field_type_name(base);

                fprintf(fp, "\n");
                for (i = 0; i < f->size[0]; i++) {
                    switch (base) {
                    case MRP_MSG_FIELD_STRING:
                        DUMP(8, "'%s'", tname, f->astr[i]);
                        break;
                    case MRP_MSG_FIELD_BOOL:
                        DUMP(8, "%s", tname, f->abln[i] ? "true" : "false");
                        break;
                    case MRP_MSG_FIELD_UINT8:
                        DUMP(8, "%u", tname, f->au8[i]);
                        break;
                    case MRP_MSG_FIELD_SINT8:
                        DUMP(8, "%d", tname, f->as8[i]);
                        break;
                    case MRP_MSG_FIELD_UINT16:
                        DUMP(8, "%u", tname, f->au16[i]);
                        break;
                    case MRP_MSG_FIELD_SINT16:
                        DUMP(8, "%d", tname, f->as16[i]);
                        break;
                    case MRP_MSG_FIELD_UINT32:
                        DUMP(8, "%u", tname, f->au32[i]);
                        break;
                    case MRP_MSG_FIELD_SINT32:
                        DUMP(8, "%d", tname, f->as32[i]);
                        break;
                    case MRP_MSG_FIELD_UINT64:
                        DUMP(8, "%Lu", tname,
                             (unsigned long long)f->au64[i]);
                        break;
                    case MRP_MSG_FIELD_SINT64:
                        DUMP(8, "%Ld", tname,
                             (long long)f->as64[i]);
                        break;
                    case MRP_MSG_FIELD_DOUBLE:
                        DUMP(8, "%f", tname, f->adbl[i]);
                        break;
                    default:
                        fprintf(fp, "%*.*s= <%s>\n", 8, 8, "", tname);
                        break;
                    }
                }
            }
            else
                fprintf(fp, "= <%s>\n", tname);
        }
    }
    l += fprintf(fp, "}\n");

    return l;
#undef DUMP
}


#define MSG_MIN_CHUNK 32

ssize_t mrp_msg_default_encode(mrp_msg_t *msg, void **bufp)
{
    mrp_msg_field_t *f;
    mrp_list_hook_t *p, *n;
    mrp_msgbuf_t     mb;
    uint32_t         len, asize, i;
    uint16_t         type;
    size_t           size;

    size = msg->nfield * (2 * sizeof(uint16_t) + sizeof(uint64_t));

    if (mrp_msgbuf_write(&mb, size)) {
        MRP_MSGBUF_PUSH(&mb, htobe16(MRP_MSG_TAG_DEFAULT), 1, nomem);
        MRP_MSGBUF_PUSH(&mb, htobe16(msg->nfield), 1, nomem);

        mrp_list_foreach(&msg->fields, p, n) {
            f = mrp_list_entry(p, typeof(*f), hook);

            MRP_MSGBUF_PUSH(&mb, htobe16(f->tag) , 1, nomem);
            MRP_MSGBUF_PUSH(&mb, htobe16(f->type), 1, nomem);

            switch (f->type) {
            case MRP_MSG_FIELD_STRING:
                len = strlen(f->str) + 1;
                MRP_MSGBUF_PUSH(&mb, htobe32(len), 1, nomem);
                MRP_MSGBUF_PUSH_DATA(&mb, f->str, len, 1, nomem);
                break;

            case MRP_MSG_FIELD_BOOL:
                MRP_MSGBUF_PUSH(&mb, htobe32(f->bln ? TRUE : FALSE), 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT8:
                MRP_MSGBUF_PUSH(&mb, f->u8, 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT8:
                MRP_MSGBUF_PUSH(&mb, f->s8, 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT16:
                MRP_MSGBUF_PUSH(&mb, htobe16(f->u16), 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT16:
                MRP_MSGBUF_PUSH(&mb, htobe16(f->s16), 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT32:
                MRP_MSGBUF_PUSH(&mb, htobe32(f->u32), 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT32:
                MRP_MSGBUF_PUSH(&mb, htobe32(f->s32), 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT64:
                MRP_MSGBUF_PUSH(&mb, htobe64(f->u64), 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT64:
                MRP_MSGBUF_PUSH(&mb, htobe64(f->s64), 1, nomem);
                break;

            case MRP_MSG_FIELD_DOUBLE:
                MRP_MSGBUF_PUSH(&mb, f->dbl, 1, nomem);
                break;

            case MRP_MSG_FIELD_BLOB:
                len   = f->size[0];
                MRP_MSGBUF_PUSH(&mb, htobe32(len), 1, nomem);
                MRP_MSGBUF_PUSH_DATA(&mb, f->blb, len, 1, nomem);
                break;

            default:
                if (f->type & MRP_MSG_FIELD_ARRAY) {
                    type  = f->type & ~(MRP_MSG_FIELD_ARRAY);
                    asize = f->size[0];
                    MRP_MSGBUF_PUSH(&mb, htobe32(asize), 1, nomem);

                    for (i = 0; i < asize; i++) {
                        switch (type) {
                        case MRP_MSG_FIELD_STRING:
                            len = strlen(f->astr[i]) + 1;
                            MRP_MSGBUF_PUSH(&mb, htobe32(len), 1, nomem);
                            MRP_MSGBUF_PUSH_DATA(&mb, f->astr[i], len,
                                                 1, nomem);
                            break;

                        case MRP_MSG_FIELD_BOOL:
                            MRP_MSGBUF_PUSH(&mb, htobe32(f->abln[i]?TRUE:FALSE),
                                            1, nomem);
                            break;

                        case MRP_MSG_FIELD_UINT8:
                            MRP_MSGBUF_PUSH(&mb, f->au8[i], 1, nomem);
                            break;

                        case MRP_MSG_FIELD_SINT8:
                            MRP_MSGBUF_PUSH(&mb, f->as8[i], 1, nomem);
                            break;

                        case MRP_MSG_FIELD_UINT16:
                            MRP_MSGBUF_PUSH(&mb, htobe16(f->au16[i]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_SINT16:
                            MRP_MSGBUF_PUSH(&mb, htobe16(f->as16[i]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_UINT32:
                            MRP_MSGBUF_PUSH(&mb, htobe32(f->au32[i]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_SINT32:
                            MRP_MSGBUF_PUSH(&mb, htobe32(f->as32[i]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_UINT64:
                            MRP_MSGBUF_PUSH(&mb, htobe64(f->au64[i]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_SINT64:
                            MRP_MSGBUF_PUSH(&mb, htobe64(f->as64[i]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_DOUBLE:
                            MRP_MSGBUF_PUSH(&mb, f->adbl[i], 1, nomem);
                            break;

                        default:
                            goto invalid_type;
                        }
                    }
                }
                else {
                invalid_type:
                    errno = EINVAL;
                    mrp_msgbuf_cancel(&mb);
                nomem:
                    *bufp = NULL;
                    return -1;
                }
            }
        }
    }

    *bufp = mb.buf;
    return mb.p - mb.buf;
}


mrp_msg_t *mrp_msg_default_decode(void *buf, size_t size)
{
    mrp_msg_t       *msg;
    mrp_msgbuf_t     mb;
    mrp_msg_value_t  v;
    void            *value;
    uint16_t         nfield, tag, type, base;
    uint32_t         len, n, i, j;

    msg = mrp_msg_create_empty();

    if (msg == NULL)
        return NULL;

    mrp_msgbuf_read(&mb, buf, size);

    nfield = be16toh(MRP_MSGBUF_PULL(&mb, typeof(nfield), 1, nodata));

    for (i = 0; i < nfield; i++) {
        tag  = be16toh(MRP_MSGBUF_PULL(&mb, typeof(tag) , 1, nodata));
        type = be16toh(MRP_MSGBUF_PULL(&mb, typeof(type), 1, nodata));

        switch (type) {
        case MRP_MSG_FIELD_STRING:
            len = be32toh(MRP_MSGBUF_PULL(&mb, typeof(len), 1, nodata));
            if (len > 0)
                value = MRP_MSGBUF_PULL_DATA(&mb, len, 1, nodata);
            else
                value = "";
            if (!mrp_msg_append(msg, tag, type, value))
                goto fail;
            break;

        case MRP_MSG_FIELD_BOOL:
            v.bln = be32toh(MRP_MSGBUF_PULL(&mb, uint32_t, 1, nodata));
            if (!mrp_msg_append(msg, tag, type, v.bln))
                goto fail;
            break;

        case MRP_MSG_FIELD_UINT8:
            v.u8 = MRP_MSGBUF_PULL(&mb, typeof(v.u8), 1, nodata);
            if (!mrp_msg_append(msg, tag, type, v.u8))
                goto fail;
            break;

        case MRP_MSG_FIELD_SINT8:
            v.s8 = MRP_MSGBUF_PULL(&mb, typeof(v.s8), 1, nodata);
            if (!mrp_msg_append(msg, tag, type, v.s8))
                goto fail;
            break;

        case MRP_MSG_FIELD_UINT16:
            v.u16 = be16toh(MRP_MSGBUF_PULL(&mb, typeof(v.u16), 1, nodata));
            if (!mrp_msg_append(msg, tag, type, v.u16))
                goto fail;
            break;

        case MRP_MSG_FIELD_SINT16:
            v.s16 = be16toh(MRP_MSGBUF_PULL(&mb, typeof(v.s16), 1, nodata));
            if (!mrp_msg_append(msg, tag, type, v.s16))
                goto fail;
            break;

        case MRP_MSG_FIELD_UINT32:
            v.u32 = be32toh(MRP_MSGBUF_PULL(&mb, typeof(v.u32), 1, nodata));
            if (!mrp_msg_append(msg, tag, type, v.u32))
                goto fail;
            break;

        case MRP_MSG_FIELD_SINT32:
            v.s32 = be32toh(MRP_MSGBUF_PULL(&mb, typeof(v.s32), 1, nodata));
            if (!mrp_msg_append(msg, tag, type, v.s32))
                goto fail;
            break;

        case MRP_MSG_FIELD_UINT64:
            v.u64 = be64toh(MRP_MSGBUF_PULL(&mb, typeof(v.u64), 1, nodata));
            if (!mrp_msg_append(msg, tag, type, v.u64))
                goto fail;
            break;

        case MRP_MSG_FIELD_SINT64:
            v.s64 = be64toh(MRP_MSGBUF_PULL(&mb, typeof(v.s64), 1, nodata));
            if (!mrp_msg_append(msg, tag, type, v.s64))
                goto fail;
            break;

        case MRP_MSG_FIELD_DOUBLE:
            v.dbl = MRP_MSGBUF_PULL(&mb, typeof(v.dbl), 1, nodata);
            if (!mrp_msg_append(msg, tag, type, v.dbl))
                goto fail;
            break;

        case MRP_MSG_FIELD_BLOB:
            len   = be32toh(MRP_MSGBUF_PULL(&mb, typeof(len), 1, nodata));
            value = MRP_MSGBUF_PULL_DATA(&mb, len, 1, nodata);
            if (!mrp_msg_append(msg, tag, type, len, value))
                goto fail;
            break;

        default:
            if (!(type & MRP_MSG_FIELD_ARRAY)) {
                errno = EINVAL;
                goto fail;
            }

            base  = type & ~MRP_MSG_FIELD_ARRAY;
            n     = be32toh(MRP_MSGBUF_PULL(&mb, typeof(n), 1, nodata));
            {
                char    *astr[n];
                bool     abln[n];
                uint8_t  au8 [n];
                int8_t   as8 [n];
                uint16_t au16[n];
                int16_t  as16[n];
                uint32_t au32[n];
                int32_t  as32[n];
                uint64_t au64[n];
                int64_t  as64[n];
                double   adbl[n];

                for (j = 0; j < n; j++) {

                    switch (base) {
                    case MRP_MSG_FIELD_STRING:
                        len = be32toh(MRP_MSGBUF_PULL(&mb, typeof(len),
                                                      1, nodata));
                        if (len > 0)
                            astr[j] = MRP_MSGBUF_PULL_DATA(&mb, len, 1, nodata);
                        else
                            astr[j] = "";
                        break;

                    case MRP_MSG_FIELD_BOOL:
                        abln[j] = be32toh(MRP_MSGBUF_PULL(&mb, uint32_t, 1,
                                                          nodata));
                        break;

                    case MRP_MSG_FIELD_UINT8:
                        au8[j] = MRP_MSGBUF_PULL(&mb, typeof(v.u8), 1, nodata);
                        break;

                    case MRP_MSG_FIELD_SINT8:
                        as8[j] = MRP_MSGBUF_PULL(&mb, typeof(v.s8), 1, nodata);
                        break;

                    case MRP_MSG_FIELD_UINT16:
                        au16[j] = be16toh(MRP_MSGBUF_PULL(&mb, typeof(v.u16),
                                                          1, nodata));
                        break;

                    case MRP_MSG_FIELD_SINT16:
                        as16[j] = be16toh(MRP_MSGBUF_PULL(&mb, typeof(v.s16),
                                                          1, nodata));
                        break;

                    case MRP_MSG_FIELD_UINT32:
                        au32[j] = be32toh(MRP_MSGBUF_PULL(&mb, typeof(v.u32),
                                                          1, nodata));
                        break;

                    case MRP_MSG_FIELD_SINT32:
                        as32[j] = be32toh(MRP_MSGBUF_PULL(&mb, typeof(v.s32),
                                                          1, nodata));
                        break;

                    case MRP_MSG_FIELD_UINT64:
                        au64[j] = be64toh(MRP_MSGBUF_PULL(&mb, typeof(v.u64),
                                                          1, nodata));
                        break;

                    case MRP_MSG_FIELD_SINT64:
                        as64[j] = be64toh(MRP_MSGBUF_PULL(&mb, typeof(v.s64),
                                                          1, nodata));
                        break;

                    case MRP_MSG_FIELD_DOUBLE:
                        adbl[j] = MRP_MSGBUF_PULL(&mb, typeof(v.dbl),
                                                  1, nodata);
                    break;

                    default:
                        errno = EINVAL;
                        goto fail;
                    }
                }

#define HANDLE_TYPE(_type, _var)                                          \
                case _type:                                               \
                    if (!mrp_msg_append(msg, tag,                         \
                                        MRP_MSG_FIELD_ARRAY |_type,       \
                                        n, _var))                         \
                        goto fail;                                        \
                    break

                switch (base) {
                    HANDLE_TYPE(MRP_MSG_FIELD_STRING, astr);
                    HANDLE_TYPE(MRP_MSG_FIELD_BOOL  , abln);
                    HANDLE_TYPE(MRP_MSG_FIELD_UINT8 , au8 );
                    HANDLE_TYPE(MRP_MSG_FIELD_SINT8 , as8 );
                    HANDLE_TYPE(MRP_MSG_FIELD_UINT16, au16);
                    HANDLE_TYPE(MRP_MSG_FIELD_SINT16, as16);
                    HANDLE_TYPE(MRP_MSG_FIELD_UINT32, au32);
                    HANDLE_TYPE(MRP_MSG_FIELD_SINT32, as32);
                    HANDLE_TYPE(MRP_MSG_FIELD_UINT64, au64);
                    HANDLE_TYPE(MRP_MSG_FIELD_SINT64, as64);
                    HANDLE_TYPE(MRP_MSG_FIELD_DOUBLE, adbl);
                default:
                    errno = EINVAL;
                    goto fail;
                }
#undef HANDLE_TYPE
            }
        }
    }

    return msg;


 fail:
 nodata:
    mrp_msg_unref(msg);
    return NULL;
}


static int guarded_array_size(void *data, mrp_data_member_t *array)
{
#define MAX_ITEMS (32 * 1024)
    uint16_t  base;
    void     *value, *guard;
    size_t    size;
    int       cnt;

    if (array->type & MRP_MSG_FIELD_ARRAY) {
        base = array->type & ~MRP_MSG_FIELD_ARRAY;

        switch (base) {
        case MRP_MSG_FIELD_STRING: size = sizeof(array->str);  break;
        case MRP_MSG_FIELD_BOOL:   size = sizeof(array->bln);  break;
        case MRP_MSG_FIELD_UINT8:  size = sizeof(array->u8);   break;
        case MRP_MSG_FIELD_SINT8:  size = sizeof(array->s8);   break;
        case MRP_MSG_FIELD_UINT16: size = sizeof(array->u16);  break;
        case MRP_MSG_FIELD_SINT16: size = sizeof(array->s16);  break;
        case MRP_MSG_FIELD_UINT32: size = sizeof(array->u32);  break;
        case MRP_MSG_FIELD_SINT32: size = sizeof(array->s32);  break;
        case MRP_MSG_FIELD_UINT64: size = sizeof(array->u64);  break;
        case MRP_MSG_FIELD_SINT64: size = sizeof(array->s64);  break;
        case MRP_MSG_FIELD_DOUBLE: size = sizeof(array->dbl);  break;
        default:                                               return -1;
        }

        guard = &array->str;
        value = *(void **)(data + array->offs);
        for (cnt = 0; cnt < MAX_ITEMS; cnt++, value += size) {
            if (!memcmp(value, guard, size))
                return cnt + 1;
        }
    }

    return -1;
#undef MAX_ITEMS
}


static int counted_array_size(void *data, mrp_data_member_t *cnt)
{
    void *val = data + cnt->offs;

    switch (cnt->type) {
    case MRP_MSG_FIELD_UINT8:  return (int)*(uint8_t  *)val;
    case MRP_MSG_FIELD_SINT8:  return (int)*( int8_t  *)val;
    case MRP_MSG_FIELD_UINT16: return (int)*(uint16_t *)val;
    case MRP_MSG_FIELD_SINT16: return (int)*( int16_t *)val;
    case MRP_MSG_FIELD_UINT32: return (int)*(uint32_t *)val;
    case MRP_MSG_FIELD_SINT32: return (int)*( int32_t *)val;
    }

    return -1;
}


static int get_array_size(void *data, mrp_data_descr_t *type, int idx)
{
    mrp_data_member_t *arr;

    if (0 < idx && idx < type->nfield) {
        arr = type->fields + idx;

        if (arr->type & MRP_MSG_FIELD_ARRAY) {
            if (arr->guard)
                return guarded_array_size(data, arr);
            else {
                if ((int)arr->u32 < type->nfield)
                    return counted_array_size(data, type->fields + arr->u32);
            }
        }
    }

    return -1;
}


int mrp_data_get_array_size(void *data, mrp_data_descr_t *type, int idx)
{
    return get_array_size(data, type, idx);
}


static int get_blob_size(void *data, mrp_data_descr_t *type, int idx)
{
    mrp_data_member_t *blb, *cnt;
    void              *val;

    if (0 < idx && idx < type->nfield) {
        blb = type->fields + idx;

        if ((int)blb->u32 < type->nfield) {
            cnt = type->fields + blb->u32;
            val = data + cnt->offs;

            switch (cnt->type) {
            case MRP_MSG_FIELD_UINT8:  return (int)*(uint8_t  *)val;
            case MRP_MSG_FIELD_SINT8:  return (int)*( int8_t  *)val;
            case MRP_MSG_FIELD_UINT16: return (int)*(uint16_t *)val;
            case MRP_MSG_FIELD_SINT16: return (int)*( int16_t *)val;
            case MRP_MSG_FIELD_UINT32: return (int)*(uint32_t *)val;
            case MRP_MSG_FIELD_SINT32: return (int)*( int32_t *)val;
            }
        }
    }

    return -1;
}


int mrp_data_get_blob_size(void *data, mrp_data_descr_t *type, int idx)
{
    return get_blob_size(data, type, idx);
}


static int check_and_init_array_descr(mrp_data_descr_t *type, int idx)
{
    mrp_data_member_t *array, *cnt, *m;
    int                i;

    array = type->fields + idx;

    if (!array->guard) {
        cnt = NULL;

        for (i = 0, m = type->fields; i < type->nfield; i++, m++) {
            if (m->offs == array->u32) {
                cnt = m;
                break;
            }
        }

        if (cnt == NULL || cnt >= array)
            return FALSE;

        if (cnt->type < MRP_MSG_FIELD_UINT8 || cnt->type > MRP_MSG_FIELD_SINT32)
            return FALSE;

        array->u32 = i;

        return TRUE;
    }
    else {
        return TRUE;
    }
}


int mrp_msg_register_type(mrp_data_descr_t *type)
{
    mrp_data_member_t *f;
    int                idx, i;

    if (direct_types == NULL) {
        direct_types = mrp_allocz_array(typeof(*direct_types), NDIRECT_TYPE);

        if (direct_types == NULL)
            return FALSE;
    }

    if (type->tag == MRP_MSG_TAG_DEFAULT) {
        errno = EINVAL;
        return FALSE;
    }

    mrp_list_init(&type->allocated);

    /* enumerate fields, check arrays, collect extra allocations */
    for (i = 0, f = type->fields; i < type->nfield; i++, f++) {
        f->tag = (uint16_t)i + 1;

        if (f->type & MRP_MSG_FIELD_ARRAY) {
            if (!check_and_init_array_descr(type, i))
                return FALSE;

            mrp_list_append(&type->allocated, &f->hook);
        }
        else {
            switch (f->type) {
            case MRP_MSG_FIELD_STRING:
            case MRP_MSG_FIELD_BLOB:
                mrp_list_append(&type->allocated, &f->hook);
            }
        }
    }

    if (type->tag <= NDIRECT_TYPE) {
        idx = type->tag - 1;

        if (direct_types[idx] == NULL)
            direct_types[idx] = type;
        else
            return FALSE;
    }
    else {
        if (mrp_reallocz(other_types, nother_type, nother_type + 1) != NULL)
            other_types[nother_type++] = type;
        else
            return FALSE;
    }

    return TRUE;
}


mrp_data_descr_t *mrp_msg_find_type(uint16_t tag)
{
    int i;

    if (MRP_UNLIKELY(tag == MRP_MSG_TAG_DEFAULT))
        return NULL;

    if (tag <= NDIRECT_TYPE)
        return direct_types[tag - 1];
    else {
        for (i = 0; i < nother_type; i++) {
            if (other_types[i] != NULL && other_types[i]->tag == tag)
                return other_types[i];
        }
    }

    return NULL;
}


static __attribute__((destructor)) void cleanup_types(void)
{
    mrp_free(direct_types);
    mrp_free(other_types);
    nother_type = 0;
}


size_t mrp_data_encode(void **bufp, void *data, mrp_data_descr_t *descr,
                       size_t reserve)
{
    mrp_data_member_t *fields, *f;
    int                nfield;
    uint16_t           type;
    mrp_msgbuf_t       mb;
    mrp_msg_value_t   *v;
    uint32_t           len, asize, blblen, j;
    int                i, cnt;
    size_t             size;

    fields = descr->fields;
    nfield = descr->nfield;
    size   = reserve + nfield * (2 * sizeof(uint16_t) + sizeof(uint64_t));

    if (mrp_msgbuf_write(&mb, size)) {
        if (reserve)
            mrp_msgbuf_reserve(&mb, reserve, 1);

        for (i = 0, f = fields; i < nfield; i++, f++) {
            MRP_MSGBUF_PUSH(&mb, htobe16(f->tag) , 1, nomem);

            v = (mrp_msg_value_t *)(data + f->offs);

            switch (f->type) {
            case MRP_MSG_FIELD_STRING:
                len = strlen(v->str) + 1;
                MRP_MSGBUF_PUSH(&mb, htobe32(len), 1, nomem);
                MRP_MSGBUF_PUSH_DATA(&mb, v->str, len, 1, nomem);
                break;

            case MRP_MSG_FIELD_BOOL:
                MRP_MSGBUF_PUSH(&mb, htobe32(v->bln ? TRUE : FALSE), 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT8:
                MRP_MSGBUF_PUSH(&mb, v->u8, 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT8:
                MRP_MSGBUF_PUSH(&mb, v->s8, 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT16:
                MRP_MSGBUF_PUSH(&mb, htobe16(v->u16), 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT16:
                MRP_MSGBUF_PUSH(&mb, htobe16(v->s16), 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT32:
                MRP_MSGBUF_PUSH(&mb, htobe32(v->u32), 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT32:
                MRP_MSGBUF_PUSH(&mb, htobe32(v->s32), 1, nomem);
                break;

            case MRP_MSG_FIELD_UINT64:
                MRP_MSGBUF_PUSH(&mb, htobe64(v->u64), 1, nomem);
                break;

            case MRP_MSG_FIELD_SINT64:
                MRP_MSGBUF_PUSH(&mb, htobe64(v->s64), 1, nomem);
                break;

            case MRP_MSG_FIELD_DOUBLE:
                MRP_MSGBUF_PUSH(&mb, v->dbl, 1, nomem);
                break;

            case MRP_MSG_FIELD_BLOB:
                blblen = (uint32_t)get_blob_size(data, descr, i);

                if (blblen == (uint32_t)-1)
                    goto invalid_type;

                MRP_MSGBUF_PUSH(&mb, htobe32(v->u32), 1, nomem);
                MRP_MSGBUF_PUSH_DATA(&mb, v->blb, blblen, 1, nomem);
                break;

            default:
                if (f->type & MRP_MSG_FIELD_ARRAY) {
                    type  = f->type & ~(MRP_MSG_FIELD_ARRAY);
                    cnt   = get_array_size(data, descr, i);

                    if (cnt < 0)
                        goto invalid_type;

                    asize = (uint32_t)cnt;
                    MRP_MSGBUF_PUSH(&mb, htobe32(asize), 1, nomem);

                    for (j = 0; j < asize; j++) {
                        switch (type) {
                        case MRP_MSG_FIELD_STRING:
                            len = strlen(v->astr[j]) + 1;
                            MRP_MSGBUF_PUSH(&mb, htobe32(len), 1, nomem);
                            MRP_MSGBUF_PUSH_DATA(&mb, v->astr[j], len,
                                                 1, nomem);
                            break;

                        case MRP_MSG_FIELD_BOOL:
                            MRP_MSGBUF_PUSH(&mb, htobe32(v->abln[j]?TRUE:FALSE),
                                            1, nomem);
                            break;

                        case MRP_MSG_FIELD_UINT8:
                            MRP_MSGBUF_PUSH(&mb, v->au8[j], 1, nomem);
                            break;

                        case MRP_MSG_FIELD_SINT8:
                            MRP_MSGBUF_PUSH(&mb, v->as8[j], 1, nomem);
                            break;

                        case MRP_MSG_FIELD_UINT16:
                            MRP_MSGBUF_PUSH(&mb, htobe16(v->au16[j]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_SINT16:
                            MRP_MSGBUF_PUSH(&mb, htobe16(v->as16[j]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_UINT32:
                            MRP_MSGBUF_PUSH(&mb, htobe32(v->au32[j]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_SINT32:
                            MRP_MSGBUF_PUSH(&mb, htobe32(v->as32[j]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_UINT64:
                            MRP_MSGBUF_PUSH(&mb, htobe64(v->au64[j]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_SINT64:
                            MRP_MSGBUF_PUSH(&mb, htobe64(v->as64[j]), 1, nomem);
                            break;

                        case MRP_MSG_FIELD_DOUBLE:
                            MRP_MSGBUF_PUSH(&mb, v->adbl[j], 1, nomem);
                            break;

                        default:
                            goto invalid_type;
                        }
                    }
                }
                else {
                invalid_type:
                    errno = EINVAL;
                    mrp_msgbuf_cancel(&mb);
                nomem:
                    *bufp = NULL;
                    return 0;
                }
            }
        }
    }

    *bufp = mb.buf;
    return (size_t)(mb.p - mb.buf);
}


static mrp_data_member_t *member_type(mrp_data_member_t *fields, int nfield,
                                      uint16_t tag)
{
    mrp_data_member_t *f;
    int                i;

    for (i = 0, f = fields; i < nfield; i++, f++)
        if (f->tag == tag)
            return f;

    return NULL;
}


void *mrp_data_decode(void **bufp, size_t *sizep, mrp_data_descr_t *descr)
{
    void              *data;
    mrp_data_member_t *fields, *f;
    int                nfield;
    mrp_msgbuf_t       mb;
    uint16_t           tag, base;
    mrp_msg_value_t   *v;
    void              *value;
    uint32_t           len, n, j, size;
    int                i;

    fields = descr->fields;
    nfield = descr->nfield;
    data   = mrp_allocz(descr->size);

    if (MRP_UNLIKELY(data == NULL))
        return NULL;

    mrp_msgbuf_read(&mb, *bufp, *sizep);

    for (i = 0; i < nfield; i++) {
        tag = be16toh(MRP_MSGBUF_PULL(&mb, typeof(tag) , 1, nodata));
        f   = member_type(fields, nfield, tag);

        if (MRP_UNLIKELY(f == NULL))
            goto unknown_field;

        v = (mrp_msg_value_t *)(data + f->offs);

        switch (f->type) {
        case MRP_MSG_FIELD_STRING:
            len = be32toh(MRP_MSGBUF_PULL(&mb, typeof(len), 1, nodata));
            if (len > 0)
                value  = MRP_MSGBUF_PULL_DATA(&mb, len, 1, nodata);
            else
                value = "";
            v->str = mrp_strdup((char *)value);
            if (v->str == NULL)
                goto nomem;
            break;

        case MRP_MSG_FIELD_BOOL:
            v->bln = be32toh(MRP_MSGBUF_PULL(&mb, uint32_t, 1, nodata));
            break;

        case MRP_MSG_FIELD_UINT8:
            v->u8 = MRP_MSGBUF_PULL(&mb, typeof(v->u8), 1, nodata);
            break;

        case MRP_MSG_FIELD_SINT8:
            v->s8 = MRP_MSGBUF_PULL(&mb, typeof(v->s8), 1, nodata);
            break;

        case MRP_MSG_FIELD_UINT16:
            v->u16 = be16toh(MRP_MSGBUF_PULL(&mb, typeof(v->u16), 1, nodata));
            break;

        case MRP_MSG_FIELD_SINT16:
            v->s16 = be16toh(MRP_MSGBUF_PULL(&mb, typeof(v->s16), 1, nodata));
            break;

        case MRP_MSG_FIELD_UINT32:
            v->u32 = be32toh(MRP_MSGBUF_PULL(&mb, typeof(v->u32), 1, nodata));
            break;

        case MRP_MSG_FIELD_SINT32:
            v->s32 = be32toh(MRP_MSGBUF_PULL(&mb, typeof(v->s32), 1, nodata));
            break;

        case MRP_MSG_FIELD_UINT64:
            v->u64 = be64toh(MRP_MSGBUF_PULL(&mb, typeof(v->u64), 1, nodata));
            break;

        case MRP_MSG_FIELD_SINT64:
            v->s64 = be64toh(MRP_MSGBUF_PULL(&mb, typeof(v->s64), 1, nodata));
            break;

        case MRP_MSG_FIELD_DOUBLE:
            v->dbl = MRP_MSGBUF_PULL(&mb, typeof(v->dbl), 1, nodata);
            break;

        case MRP_MSG_FIELD_BLOB:
            len    = be32toh(MRP_MSGBUF_PULL(&mb, typeof(len), 1, nodata));
            value  = MRP_MSGBUF_PULL_DATA(&mb, len, 1, nodata);
            v->blb = mrp_datadup(value, len);
            if (v->blb == NULL)
                goto nomem;
            break;

        default:
            if (!(f->type & MRP_MSG_FIELD_ARRAY)) {
            unknown_field:
                errno = EINVAL;
                goto fail;
            }

            base  = f->type & ~MRP_MSG_FIELD_ARRAY;
            n     = be32toh(MRP_MSGBUF_PULL(&mb, typeof(n), 1, nodata));

            if (!f->guard && get_array_size(data, descr, i) != (int)n) {
                errno = EINVAL;
                goto fail;
            }

            size = n;

            switch (base) {
            case MRP_MSG_FIELD_STRING: size *= sizeof(*v->astr); break;
            case MRP_MSG_FIELD_BOOL:   size *= sizeof(*v->abln); break;
            case MRP_MSG_FIELD_UINT8:  size *= sizeof(*v->au8);  break;
            case MRP_MSG_FIELD_SINT8:  size *= sizeof(*v->as8);  break;
            case MRP_MSG_FIELD_UINT16: size *= sizeof(*v->au16); break;
            case MRP_MSG_FIELD_SINT16: size *= sizeof(*v->as16); break;
            case MRP_MSG_FIELD_UINT32: size *= sizeof(*v->au32); break;
            case MRP_MSG_FIELD_SINT32: size *= sizeof(*v->as32); break;
            case MRP_MSG_FIELD_UINT64: size *= sizeof(*v->au64); break;
            case MRP_MSG_FIELD_SINT64: size *= sizeof(*v->as64); break;
            case MRP_MSG_FIELD_DOUBLE: size *= sizeof(*v->adbl); break;
            default:
                errno = EINVAL;
                goto fail;
            }

            v->aany = mrp_allocz(size);
            if (v->aany == NULL)
                goto nomem;

            for (j = 0; j < n; j++) {
                switch (base) {
                case MRP_MSG_FIELD_STRING:
                    len = be32toh(MRP_MSGBUF_PULL(&mb, typeof(len),
                                                  1, nodata));
                    if (len > 0)
                        value = MRP_MSGBUF_PULL_DATA(&mb, len, 1, nodata);
                    else
                        value = "";

                    v->astr[j] = mrp_strdup(value);
                    if (v->astr[j] == NULL)
                        goto nomem;
                    break;

                    case MRP_MSG_FIELD_BOOL:
                        v->abln[j] = be32toh(MRP_MSGBUF_PULL(&mb, uint32_t,
                                                             1, nodata));
                        break;

                    case MRP_MSG_FIELD_UINT8:
                        v->au8[j] = MRP_MSGBUF_PULL(&mb, typeof(v->u8),
                                                    1, nodata);
                        break;

                    case MRP_MSG_FIELD_SINT8:
                        v->as8[j] = MRP_MSGBUF_PULL(&mb, typeof(v->s8),
                                                    1, nodata);
                        break;

                    case MRP_MSG_FIELD_UINT16:
                        v->au16[j] = be16toh(MRP_MSGBUF_PULL(&mb,
                                                             typeof(v->u16),
                                                             1, nodata));
                        break;

                    case MRP_MSG_FIELD_SINT16:
                        v->as16[j] = be16toh(MRP_MSGBUF_PULL(&mb,
                                                             typeof(v->s16),
                                                             1, nodata));
                        break;

                    case MRP_MSG_FIELD_UINT32:
                        v->au32[j] = be32toh(MRP_MSGBUF_PULL(&mb,
                                                             typeof(v->u32),
                                                             1, nodata));
                        break;

                    case MRP_MSG_FIELD_SINT32:
                        v->as32[j] = be32toh(MRP_MSGBUF_PULL(&mb,
                                                             typeof(v->s32),
                                                             1, nodata));
                        break;

                    case MRP_MSG_FIELD_UINT64:
                        v->au64[j] = be64toh(MRP_MSGBUF_PULL(&mb,
                                                             typeof(v->u64),
                                                             1, nodata));
                        break;

                    case MRP_MSG_FIELD_SINT64:
                        v->as64[j] = be64toh(MRP_MSGBUF_PULL(&mb,
                                                             typeof(v->s64),
                                                             1, nodata));
                        break;

                    case MRP_MSG_FIELD_DOUBLE:
                        v->adbl[j] = MRP_MSGBUF_PULL(&mb, typeof(v->dbl),
                                                     1, nodata);
                        break;

                    default:
                        errno = EINVAL;
                        goto fail;
                }
            }
        }
    }

    *bufp   = mb.buf;
    *sizep -= mb.p - mb.buf;
    return data;

 nodata:
 nomem:
 fail:
    if (data != NULL) {
        for (i = 0, f = fields; i < nfield; i++, f++) {
            switch (f->type) {
            case MRP_MSG_FIELD_STRING:
            case MRP_MSG_FIELD_BLOB:
                mrp_free(*(void **)(data + f->offs));
            }
        }

        mrp_free(data);
    }

    return NULL;
}


int mrp_data_dump(void *data, mrp_data_descr_t *descr, FILE *fp)
{
#define DUMP(_indent, _fmt, _typename, _val)                              \
        l += fprintf(fp, "%*.*s= <%s> "_fmt"\n", _indent, _indent, "",    \
                     _typename, _val)

    mrp_data_member_t *dm;
    mrp_msg_value_t   *v;
    uint16_t           base;
    int                i, j, l, cnt;
    const char        *tname;


    l = fprintf(fp, "{\n");
    for (i = 0, dm = descr->fields; i < descr->nfield; i++, dm++) {
        l     += fprintf(fp, "    @%d ", dm->offs);
        v      = (mrp_msg_value_t *)(data + dm->offs);
        tname  = field_type_name(dm->type);

        switch (dm->type) {
        case MRP_MSG_FIELD_STRING:
            DUMP(0, "'%s'", tname, v->str);
            break;
        case MRP_MSG_FIELD_BOOL:
            DUMP(0, "%s", tname, v->bln ? "true" : "false");
            break;
        case MRP_MSG_FIELD_UINT8:
            DUMP(0, "%u", tname, v->u8);
            break;
        case MRP_MSG_FIELD_SINT8:
            DUMP(0, "%d", tname, v->s8);
            break;
        case MRP_MSG_FIELD_UINT16:
            DUMP(0, "%u", tname, v->u16);
            break;
        case MRP_MSG_FIELD_SINT16:
            DUMP(0, "%d", tname, v->s16);
            break;
        case MRP_MSG_FIELD_UINT32:
            DUMP(0, "%u", tname, v->u32);
            break;
        case MRP_MSG_FIELD_SINT32:
            DUMP(0, "%d", tname, v->s32);
            break;
        case MRP_MSG_FIELD_UINT64:
            DUMP(0, "%Lu", tname, (long long unsigned)v->u64);
            break;
        case MRP_MSG_FIELD_SINT64:
            DUMP(0, "%Ld", tname, (long long signed)v->s64);
            break;
        case MRP_MSG_FIELD_DOUBLE:
            DUMP(0, "%f", tname, v->dbl);
            break;
        default:
            if (dm->type & MRP_MSG_FIELD_ARRAY) {
                base  = dm->type & ~MRP_MSG_FIELD_ARRAY;
                cnt   = get_array_size(data, descr, i);

                if (cnt < 0) {
                    fprintf(fp, "= <%s> ???\n", tname);
                    continue;
                }

                fprintf(fp, "= <%s> (%d)\n", tname, cnt);
                tname = field_type_name(base);

                for (j = 0; j < cnt; j++) {
                    switch (base) {
                    case MRP_MSG_FIELD_STRING:
                        DUMP(8, "'%s'", tname, v->astr[j]);
                        break;
                    case MRP_MSG_FIELD_BOOL:
                        DUMP(8, "%s", tname, v->abln[j] ? "true" : "false");
                        break;
                    case MRP_MSG_FIELD_UINT8:
                        DUMP(8, "%u", tname, v->au8[j]);
                        break;
                    case MRP_MSG_FIELD_SINT8:
                        DUMP(8, "%d", tname, v->as8[j]);
                        break;
                    case MRP_MSG_FIELD_UINT16:
                        DUMP(8, "%u", tname, v->au16[j]);
                        break;
                    case MRP_MSG_FIELD_SINT16:
                        DUMP(8, "%d", tname, v->as16[j]);
                        break;
                    case MRP_MSG_FIELD_UINT32:
                        DUMP(8, "%u", tname, v->au32[j]);
                        break;
                    case MRP_MSG_FIELD_SINT32:
                        DUMP(8, "%d", tname, v->as32[j]);
                        break;
                    case MRP_MSG_FIELD_UINT64:
                        DUMP(8, "%Lu", tname, (long long unsigned)v->au64[j]);
                        break;
                    case MRP_MSG_FIELD_SINT64:
                        DUMP(8, "%Ld", tname, (long long signed)v->as64[j]);
                        break;
                    case MRP_MSG_FIELD_DOUBLE:
                        DUMP(8, "%f", tname, v->adbl[j]);
                        break;
                    default:
                        fprintf(fp, "%*.*s<%s>\n", 8, 8, "", tname);
                        break;
                    }
                }
            }
        }
    }
    l += fprintf(fp, "}\n");

    return l;
}


int mrp_data_free(void *data, uint16_t tag)
{
    mrp_data_descr_t  *type;
    mrp_list_hook_t   *p, *n;
    mrp_data_member_t *f;
    void              *ptr;
    int                i, idx, cnt;

    if (data == NULL)
        return TRUE;

    type = mrp_msg_find_type(tag);

    if (type != NULL) {
        mrp_list_foreach(&type->allocated, p, n) {
            f   = mrp_list_entry(p, typeof(*f), hook);
            ptr = *(void **)(data + f->offs);

            if (f->type == (MRP_MSG_FIELD_ARRAY | MRP_MSG_FIELD_STRING)) {
                idx = f - type->fields;
                cnt = get_array_size(data, type, idx);

                for (i = 0; i < cnt; i++)
                    mrp_free(((char **)ptr)[i]);
            }

            mrp_free(ptr);
        }

        mrp_free(data);

        return TRUE;
    }
    else
        return FALSE;
}


void *mrp_msgbuf_write(mrp_msgbuf_t *mb, size_t size)
{
    mrp_clear(mb);

    mb->buf = mrp_allocz(size);

    if (mb->buf != NULL) {
        mb->size = size;
        mb->p    = mb->buf;
        mb->l    = size;

        return mb->p;
    }
    else
        return NULL;
}


void mrp_msgbuf_read(mrp_msgbuf_t *mb, void *buf, size_t size)
{
    mb->buf  = mb->p = buf;
    mb->size = mb->l = size;
}


void mrp_msgbuf_cancel(mrp_msgbuf_t *mb)
{
    mrp_free(mb->buf);
    mb->buf = mb->p = NULL;
}


void *mrp_msgbuf_ensure(mrp_msgbuf_t *mb, size_t size)
{
    int diff;

    if (MRP_UNLIKELY(size > mb->l)) {
        diff = size - mb->l;

        if (diff < MSG_MIN_CHUNK)
            diff = MSG_MIN_CHUNK;

        mb->p -= (ptrdiff_t)mb->buf;

        if (mrp_realloc(mb->buf, mb->size + diff)) {
            memset(mb->buf + mb->size, 0, diff);
            mb->size += diff;
            mb->p    += (ptrdiff_t)mb->buf;
            mb->l    += diff;
        }
        else
            mrp_msgbuf_cancel(mb);
    }

    return mb->p;
}


void *mrp_msgbuf_reserve(mrp_msgbuf_t *mb, size_t size, size_t align)
{
    void      *reserved;
    ptrdiff_t  offs, pad;
    size_t     len;

    len  = size;
    offs = mb->p - mb->buf;

    if (offs % align != 0) {
        pad  = align - (offs % align);
        len += pad;
    }
    else
        pad = 0;

    if (mrp_msgbuf_ensure(mb, len)) {
        if (pad != 0)
            memset(mb->p, 0, pad);

        reserved = mb->p + pad;

        mb->p += len;
        mb->l -= len;
    }
    else
        reserved = NULL;

    return reserved;
}


void *mrp_msgbuf_pull(mrp_msgbuf_t *mb, size_t size, size_t align)
{
    void      *pulled;
    ptrdiff_t  offs, pad;
    size_t     len;

    len  = size;
    offs = mb->p - mb->buf;

    if (offs % align != 0) {
        pad  = align - (offs % align);
        len += pad;
    }
    else
        pad = 0;

    if (mb->l >= len) {
        pulled = mb->p + pad;

        mb->p += len;
        mb->l -= len;
    }
    else
        pulled = NULL;

    return pulled;
}
