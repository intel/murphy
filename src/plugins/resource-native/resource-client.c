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

#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <ctype.h>
#include <libgen.h>
#include <errno.h>

#include <murphy/common.h>
#include <murphy/resource/protocol.h>

#define ARRAY_MAX      1024
#define RESOURCE_MAX   32
#define ATTRIBUTE_MAX  32

#define INVALID_ID      (~(uint32_t)0)
#define INVALID_INDEX   (~(uint32_t)0)
#define INVALID_SEQNO   (~(uint32_t)0)
#define INVALID_REQUEST (~(uint16_t)0)

#define GRANT           0
#define ADVICE          1


typedef struct {
    uint32_t        dim;
    const char     *elems[0];
} string_array_t;

typedef struct {
    const char     *name;
    char            type;       /* s:char *, i:int32_t, u:uint32_t, f:double */
    union {
        const char *string;
        int32_t     integer;
        uint32_t    unsignd;
        double      floating;
    } v;
} attribute_t;

typedef struct {
    uint32_t       dim;
    attribute_t    elems[0];
} attribute_array_t;

typedef struct {
    const char        *name;
    attribute_array_t *attrs;
} resource_def_t;

typedef struct {
    uint32_t          dim;
    resource_def_t    defs[0];
} resource_def_array_t;

typedef struct {
    const char           *name;
    mrp_mainloop_t       *ml;
    mrp_transport_t      *transp;
    mrp_sockaddr_t        saddr;
    socklen_t             alen;
    const char           *atype;
    uint32_t              seqno;
    bool                  prompt;
    bool                  msgdump;
    char *                class;
    char *                zone;
    char *                rsetd;
    uint32_t              rsetf;
    uint32_t              priority;
    resource_def_array_t *resources;
    string_array_t       *class_names;
    string_array_t       *zone_names;
    uint32_t              rset_id;
} client_t;

typedef struct {
    uint32_t              seqno;
    uint64_t              time;
} reqstamp_t;

#define HASH_BITS         8
#define HASH_MAX          (((uint32_t)1) << HASH_BITS)
#define HASH_MASK         (HASH_MAX - 1)
#define HASH_FUNC(s)      ((uint32_t)(s) & HASH_MASK)

static reqstamp_t         reqstamps[HASH_MAX];
static uint64_t           totaltime;
static uint32_t           reqcount;

static void print_prompt(client_t *, bool);


static uint64_t reqstamp_current_time(void)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) < 0)
        return 0ULL;

    return ((uint64_t)tv.tv_sec * 1000000ULL) + (uint64_t)tv.tv_usec;
}

static void reqstamp_start(uint32_t seqno)
{
    reqstamp_t *rs  = reqstamps + HASH_FUNC(seqno);
    uint64_t    now = reqstamp_current_time();

    if (!rs->seqno && !rs->time && now) {
        rs->seqno = seqno;
        rs->time  = now;
    }
}

static void reqstamp_intermediate(uint32_t seqno)
{
    reqstamp_t *rs   = reqstamps + HASH_FUNC(seqno);
    uint64_t    now  = reqstamp_current_time();

    if (rs->seqno == seqno && rs->time && now > rs->time) {
        printf("request %u was responded in %.2lf msec\n",
               seqno, (double)(now - rs->time) / 1000.0);
    }
}

static void reqstamp_end(uint32_t seqno)
{
    reqstamp_t *rs   = reqstamps + HASH_FUNC(seqno);
    uint64_t    now  = reqstamp_current_time();
    uint64_t    diff = 0;

    if (rs->seqno == seqno && rs->time) {
        if (now > rs->time) {
            diff = now - rs->time;
        }

        printf("request %u was processed in %.2lf msec\n",
               seqno, (double)diff / 1000.0);

        rs->seqno = 0;
        rs->time = 0ULL;

        totaltime += diff;
        reqcount++;
    }
}


static void str_array_free(string_array_t *arr)
{
    uint32_t i;

    if (arr) {
        for (i = 0;  i < arr->dim;  i++)
            mrp_free((void *)arr->elems[i]);

        mrp_free(arr);
    }
}

static string_array_t *str_array_dup(uint32_t dim, const char **arr)
{
    size_t size;
    uint32_t i;
    string_array_t *dup;

    MRP_ASSERT(dim < ARRAY_MAX && arr, "invalid argument");

    if (!dim && arr) {
        for (dim = 0;  arr[dim];  dim++)
            ;
    }

    size = sizeof(string_array_t) + (sizeof(const char *) * (dim + 1));

    if (!(dup = mrp_allocz(size))) {
        errno = ENOMEM;
        return NULL;
    }

    dup->dim = dim;

    for (i = 0;   i < dim;   i++) {
        if (arr[i]) {
            if (!(dup->elems[i] = mrp_strdup(arr[i]))) {
                errno = ENOMEM;
                /* probably no use for freing anything */
                return NULL;
            }
        }
    }

    return dup;
}


static int str_array_print(string_array_t *arr, const char *hdr,
                           const char *sep, const char *trail,
                           char *buf, int len)
{
    uint32_t i;
    int cnt;

    char *p, *e;

    if (!sep)
        sep = " ";

    e = (p = buf) + len;
    cnt = 0;

    if (hdr && p < e)
        p += snprintf(p, e-p, "%s", hdr);

    if (arr) {
        for (i = 0;  i < arr->dim && p < e;  i++) {
            p += snprintf(p, e-p, "%s'%s'", sep, arr->elems[i]);
            cnt++;
        }
    }

    if (!cnt && p < e)
        p += snprintf(p, e-p, "%s<none>", sep);

    if (trail && hdr && p < e)
        p += snprintf(p, e-p, "%s", trail);

    return p - buf;
}

#if 0
static uint32_t str_array_index(string_array_t *arr, const char *member)
{
    uint32_t i;

    if (arr && member) {
        for (i = 0;  i < arr->dim;  i++) {
            if (!strcmp(member, arr->elems[i]))
                return i;
        }
    }

    return INVALID_INDEX;
}
#endif

static void attribute_array_free(attribute_array_t *arr)
{
    uint32_t i;
    attribute_t *attr;

    if (arr) {
        for (i = 0;   i < arr->dim;   i++) {
            attr = arr->elems + i;

            mrp_free((void *)attr->name);

            if (attr->type == 's')
                mrp_free((void *)attr->v.string);
        }
        mrp_free(arr);
    }
}

static attribute_array_t *attribute_array_dup(uint32_t dim, attribute_t *arr)
{
    size_t size;
    uint32_t i;
    attribute_t *sattr, *dattr;
    attribute_array_t *dup;
    int err = ENOMEM;

    MRP_ASSERT(dim < ARRAY_MAX && arr, "invalid argument");

    if (!dim && arr) {
        for (dim = 0;  arr[dim].name;  dim++)
            ;
    }

    size = sizeof(attribute_array_t) + (sizeof(attribute_t) * (dim + 1));

    if (!(dup = mrp_allocz(size))) {
        goto failed;
    }

    dup->dim = dim;

    for (i = 0;    i < dim;    i++) {
        sattr = arr + i;
        dattr = dup->elems + i;

        if (!(dattr->name = mrp_strdup(sattr->name))) {
            goto failed;
        }

        switch ((dattr->type = sattr->type)) {
        case 's':
            if (!(dattr->v.string = mrp_strdup(sattr->v.string))) {
                goto failed;
            }
            break;
        case 'i':
            dattr->v.integer = sattr->v.integer;
            break;
        case 'u':
            dattr->v.unsignd = sattr->v.unsignd;
            break;
        case 'f':
            dattr->v.floating = sattr->v.floating;
            break;
        default:
            errno = EINVAL;
            goto failed;
        }
    }

    return dup;

 failed:
    attribute_array_free(dup);
    errno = err;
    return NULL;
}


static int attribute_array_print(attribute_array_t *arr, const char *hdr,
                                 const char *sep, const char *trail,
                                 char *buf, int len)
{
    attribute_t *attr;
    uint32_t i;
    int cnt;

    char *p, *e;

    if (!sep)
        sep = " ";

    e = (p = buf) + len;
    cnt = 0;

    if (hdr && p < e)
        p += snprintf(p, e-p, "%s", hdr);

    if (arr) {
        for (i = 0;  i < arr->dim && p < e;  i++) {
            attr = arr->elems + i;

            p += snprintf(p, e-p, "%s%s:%c:", sep, attr->name, attr->type);

            if (p < e) {
                switch (attr->type) {
                case 's': p += snprintf(p,e-p, "'%s'", attr->v.string);  break;
                case 'i': p += snprintf(p,e-p, "%d",   attr->v.integer); break;
                case 'u': p += snprintf(p,e-p, "%u",   attr->v.unsignd); break;
                case 'f': p += snprintf(p,e-p, "%.2lf",attr->v.floating);break;
                default:  p += snprintf(p,e-p, "<unknown>");             break;
                }
            }

            cnt++;
        }
    }

    if (!cnt && hdr && p < e)
        p += snprintf(p, e-p, "%s<none>", sep);

    if (trail && hdr && p < e)
        p += snprintf(p, e-p, "%s", trail);

    return p - buf;
}

#if 0
static uint32_t attribute_array_index(attribute_array_t *arr,
                                      const char *member)
{
    uint32_t i;

    if (arr && member) {
        for (i = 0;  i < arr->dim;  i++) {
            if (!strcmp(member, arr->elems[i].name))
                return i;
        }
    }

    return INVALID_INDEX;
}
#endif

static void resource_def_array_free(resource_def_array_t *arr)
{
    uint32_t i;
    resource_def_t *def;

    if (arr) {
        for (i = 0;   i < arr->dim;   i++) {
            def = arr->defs + i;

            mrp_free((void *)def->name);
            attribute_array_free(def->attrs);
        }

        mrp_free(arr);
    }
}


static int resource_def_array_print(resource_def_array_t *arr,
                                    const char *rhdr,
                                    const char *rsep,
                                    const char *rtrail,
                                    const char *ahdr,
                                    const char *asep,
                                    const char *atrail,
                                    char *buf, int len)
{
    resource_def_t *def;
    uint32_t i;
    int cnt;

    char *p, *e;

    if (!rsep)
        rsep = " ";

    e = (p = buf) + len;
    cnt = 0;

    if (rhdr && p < e)
        p += snprintf(p, e-p, "%s", rhdr);

    if (arr) {
        for (i = 0;  i < arr->dim && p < e;  i++) {
            def = arr->defs + i;

            p += snprintf(p, e-p, "%s%s", rsep, def->name);

            if (p < e)
                p += attribute_array_print(def->attrs,ahdr,asep,atrail,p,e-p);

            cnt++;
        }
    }

    if (!cnt && rhdr && p < e)
        p += snprintf(p, e-p, "%s<none>", rsep);

    if (rtrail && p < e)
        p += snprintf(p, e-p, "%s", rtrail);

    return p - buf;
}




static bool fetch_seqno(mrp_msg_t *msg, void **pcursor, uint32_t *pseqno)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_SEQUENCE_NO || type != MRP_MSG_FIELD_UINT32)
    {
        *pseqno = INVALID_SEQNO;
        return false;
    }

    *pseqno = value.u32;
    return true;
}


static bool fetch_request(mrp_msg_t *msg, void **pcursor, uint16_t *preqtype)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_REQUEST_TYPE || type != MRP_MSG_FIELD_UINT16)
    {
        *preqtype = INVALID_REQUEST;
        return false;
    }

    *preqtype = value.u16;
    return true;
}

static bool fetch_status(mrp_msg_t *msg, void **pcursor, int *pstatus)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_REQUEST_STATUS || type != MRP_MSG_FIELD_SINT16)
    {
        *pstatus = EIO;
        return false;
    }

    *pstatus = value.s16;
    return true;
}

static bool fetch_resource_set_id(mrp_msg_t *msg, void **pcursor,uint32_t *pid)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_SET_ID || type != MRP_MSG_FIELD_UINT32)
    {
        *pid = INVALID_ID;
        return false;
    }

    *pid = value.u32;
    return true;
}

static bool fetch_resource_set_state(mrp_msg_t *msg, void **pcursor,
                                     mrp_resproto_state_t *pstate)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_STATE || type != MRP_MSG_FIELD_UINT16)
    {
        *pstate = 0;
        return false;
    }

    *pstate = value.u16;
    return true;
}


static bool fetch_resource_set_mask(mrp_msg_t *msg, void **pcursor,
                                    int mask_type, mrp_resproto_state_t *pmask)
{
    uint16_t expected_tag;
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    switch (mask_type) {
    case GRANT:    expected_tag = RESPROTO_RESOURCE_GRANT;     break;
    case ADVICE:   expected_tag = RESPROTO_RESOURCE_ADVICE;    break;
    default:       /* don't know what to fetch */              return false;
    }

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != expected_tag || type != MRP_MSG_FIELD_UINT32)
    {
        *pmask = 0;
        return false;
    }

    *pmask = value.u32;
    return true;
}

static bool fetch_resource_name(mrp_msg_t *msg, void **pcursor,
                                const char **pname)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != RESPROTO_RESOURCE_NAME || type != MRP_MSG_FIELD_STRING)
    {
        *pname = "<unknown>";
        return false;
    }

    *pname = value.str;
    return true;
}


static bool fetch_str_array(mrp_msg_t *msg, void **pcursor,
                            uint16_t expected_tag, string_array_t **parr)
{
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;

    if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
        tag != expected_tag || type != MRP_MSG_FIELD_ARRAY_OF(STRING))
    {
        *parr = str_array_dup(0, NULL);
        return false;
    }

    if (!(*parr = str_array_dup(size, (const char **)value.astr)))
        return false;

    return true;
}

static bool fetch_attribute_array(mrp_msg_t *msg, void **pcursor,
                                 size_t dim, attribute_t *arr)
{
    attribute_t *attr;
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;
    size_t i;

    i = 0;

    while (mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size)) {
        if (tag == RESPROTO_SECTION_END && type == MRP_MSG_FIELD_UINT8)
            break;

        if (tag  != RESPROTO_ATTRIBUTE_NAME ||
            type != MRP_MSG_FIELD_STRING    ||
            i    >= dim - 1)
        {
            return false;
        }

        attr = arr + i++;
        attr->name = value.str;

        if (!mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size) ||
            tag != RESPROTO_ATTRIBUTE_VALUE)
        {
            return false;
        }

        switch (type) {
        case MRP_MSG_FIELD_STRING:
            attr->type = 's';
            attr->v.string = value.str;
            break;
        case MRP_MSG_FIELD_SINT32:
            attr->type = 'i';
            attr->v.integer = value.s32;
            break;
        case MRP_MSG_FIELD_UINT32:
            attr->type = 'u';
            attr->v.unsignd = value.u32;
            break;
        case MRP_MSG_FIELD_DOUBLE:
            attr->type = 'f';
            attr->v.floating = value.dbl;
            break;
        default:
            return false;
        }
    }

    memset(arr + i, 0, sizeof(attribute_t));

    return true;
}


static void resource_query_response(client_t *client, uint32_t seqno,
                                    mrp_msg_t *msg, void **pcursor)
{
    int             status;
    uint32_t        dim, i;
    resource_def_t  rdef[RESOURCE_MAX];
    attribute_t     attrs[ATTRIBUTE_MAX + 1];
    resource_def_t *src, *dst;
    resource_def_array_t *arr;
    size_t          size;
    char            buf[4096];

    MRP_UNUSED(seqno);

    if (!fetch_status(msg, pcursor, &status))
        goto failed;

    if (status != 0)
        printf("Resource query failed (%u): %s\n", status, strerror(status));
    else {
        dim = 0;

        while (fetch_resource_name(msg, pcursor, &rdef[dim].name)) {
            if (!fetch_attribute_array(msg, pcursor, ATTRIBUTE_MAX+1, attrs))
                goto failed;

            if (!(rdef[dim].attrs = attribute_array_dup(0, attrs))) {
                mrp_log_error("failed to duplicate attributes");
                return;
            }

            dim++;
        }

        size = sizeof(resource_def_array_t) + sizeof(resource_def_t) * (dim+1);


        resource_def_array_free(client->resources);

        client->resources = arr = mrp_allocz(size);

        arr->dim = dim;

        for (i = 0;  i < dim;  i++) {
            src = rdef + i;
            dst = arr->defs + i;

            dst->name  = mrp_strdup(src->name);
            dst->attrs = src->attrs;
        }

        resource_def_array_print(client->resources,
                                 "Resource definitions:", "\n   ", "\n",
                                 NULL, "\n      ", NULL,
                                 buf, sizeof(buf));
        printf("\n%s", buf);

        client->prompt = true;
        print_prompt(client, true);
    }

    return;

 failed:
    mrp_log_error("malformed reply to recource query");
}

static void class_query_response(client_t *client, uint32_t seqno,
                                 mrp_msg_t *msg, void **pcursor)
{
    int status;
    string_array_t *arr;
    char buf[4096];

    MRP_UNUSED(seqno);

    if (!fetch_status(msg, pcursor, &status) || (status == 0 &&
        !fetch_str_array(msg, pcursor, RESPROTO_CLASS_NAME, &arr)))
    {
        mrp_log_error("ignoring malformed response to class query");
        return;
    }

    if (status) {
        mrp_log_error("class query failed with error code %u", status);
        return;
    }

    str_array_free(client->class_names);
    client->class_names = arr;

    str_array_print(arr, "Application class names:", "\n   ", "\n",
                    buf, sizeof(buf));

    printf("\n%s", buf);

    client->prompt = true;
    print_prompt(client, true);
}

static void zone_query_response(client_t *client, uint32_t seqno,
                                mrp_msg_t *msg, void **pcursor)
{
    int status;
    string_array_t *arr;
    char buf[4096];

    MRP_UNUSED(seqno);

    if (!fetch_status(msg, pcursor, &status) || (status == 0 &&
        !fetch_str_array(msg, pcursor, RESPROTO_ZONE_NAME, &arr)))
    {
        mrp_log_error("ignoring malformed response to zone query");
        return;
    }

    if (status) {
        mrp_log_error("zone query failed with error code %u", status);
        return;
    }

    str_array_free(client->zone_names);
    client->zone_names = arr;

    str_array_print(arr, "Zone names:", "\n   ", "\n",
                    buf, sizeof(buf));

    printf("\n%s", buf);

    client->prompt = true;
    print_prompt(client, true);
}

static void create_resource_set_response(client_t *client, uint32_t seqno,
                                         mrp_msg_t *msg, void **pcursor)
{
    int status;
    uint32_t rset_id;

    MRP_UNUSED(seqno);

    if (!fetch_status(msg, pcursor, &status) || (status == 0 &&
        !fetch_resource_set_id(msg, pcursor, &rset_id)))
    {
        mrp_log_error("ignoring malformed response to resource set creation");
        return;
    }

    if (status) {
        mrp_log_error("creation of resource set failed. error code %u",status);
        return;
    }

    client->rset_id = rset_id;

    printf("\nresource set %u created\n", rset_id);

    client->prompt = true;
    print_prompt(client, true);
}

static void acquire_resource_set_response(client_t *client, uint32_t seqno,
                                          bool acquire, mrp_msg_t *msg,
                                          void **pcursor)
{
    const char *op = acquire ? "acquisition" : "release";
    int status;
    uint32_t rset_id;

    if (!fetch_resource_set_id(msg, pcursor, &rset_id) ||
        !fetch_status(msg, pcursor, &status))
    {
        mrp_log_error("ignoring malformed response to resource set %s", op);
        return;
    }

    if (status) {
        printf("\n%s of resource set %u failed. request no %u "
               "error code %u", op, rset_id, seqno, status);
    }
    else {
        printf("\nSuccessful %s of resource set %u. request no %u\n",
               op, rset_id, seqno);
    }

    client->prompt = true;

    if (status)
        print_prompt(client, true);
}


static void resource_event(client_t *client, uint32_t seqno, mrp_msg_t *msg,
                           void **pcursor)
{
    uint32_t rset;
    uint32_t grant, advice;
    mrp_resproto_state_t state;
    const char *str_state;
    uint16_t tag;
    uint16_t type;
    mrp_msg_value_t value;
    size_t size;
    uint32_t resid;
    const char *resnam;
    attribute_t attrs[ATTRIBUTE_MAX + 1];
    attribute_array_t *list;
    char buf[4096];
    uint32_t mask;
    int cnt;

    printf("\nResource event (request no %u):\n", seqno);

    if (!fetch_resource_set_id(msg, pcursor, &rset) ||
        !fetch_resource_set_state(msg, pcursor, &state) ||
        !fetch_resource_set_mask(msg, pcursor, GRANT, &grant) ||
        !fetch_resource_set_mask(msg, pcursor, ADVICE, &advice))
        goto malformed;

    switch (state) {
    case RESPROTO_RELEASE:   str_state = "release";    break;
    case RESPROTO_ACQUIRE:   str_state = "acquire";    break;
    default:                 str_state = "<unknown>";  break;
    }

    printf("   resource-set ID  : %u\n"  , rset);
    printf("   state            : %s\n"  , str_state);
    printf("   grant mask       : 0x%x\n", grant);
    printf("   advice mask      : 0x%x\n", advice);
    printf("   resources        :");

    cnt = 0;

    while (mrp_msg_iterate(msg, pcursor, &tag, &type, &value, &size)) {
        if ((tag != RESPROTO_RESOURCE_ID || type != MRP_MSG_FIELD_UINT32) ||
            !fetch_resource_name(msg, pcursor, &resnam))
            goto malformed;

        resid = value.u32;
        mask  = (1UL <<  resid);

        if (!cnt++)
            printf("\n");

        printf("      %02u name       : %s\n", resid, resnam);
        printf("         mask       : 0x%x\n", mask);
        printf("         grant      : %s\n", (grant & mask)  ? "yes" : "no");
        printf("         advice     : %savailable\n",
               (advice & mask)  ? "" : "not ");

        if (!fetch_attribute_array(msg, pcursor, ATTRIBUTE_MAX + 1, attrs))
            goto malformed;

        if (!(list = attribute_array_dup(0, attrs))) {
            mrp_log_error("failed to duplicate attribute list");
            exit(ENOMEM);
        }

        attribute_array_print(list, "         attributes :", " ", "\n",
                              buf, sizeof(buf));
        printf("%s", buf);

        attribute_array_free(list);
    }

    if (!cnt)
        printf(" <none>\n");

    print_prompt(client, true);

    return;

 malformed:
    mrp_log_error("ignoring malformed resource event");
}


static void recvfrom_msg(mrp_transport_t *transp, mrp_msg_t *msg,
                         mrp_sockaddr_t *addr, socklen_t addrlen,
                         void *user_data)
{
    client_t *client = (client_t *)user_data;
    void     *cursor = NULL;
    uint32_t  seqno;
    uint16_t  request;

    MRP_UNUSED(transp);
    MRP_UNUSED(addr);
    MRP_UNUSED(addrlen);

    if (client->msgdump) {
        mrp_log_info("received a message");
        mrp_msg_dump(msg, stdout);
    }

    if (!fetch_seqno   (msg, &cursor, &seqno  ) ||
        !fetch_request (msg, &cursor, &request)   )
    {
        mrp_log_error("ignoring malformed message");
        return;
    }


    switch (request) {
    case RESPROTO_QUERY_RESOURCES:
        reqstamp_end(seqno);
        resource_query_response(client, seqno, msg, &cursor);
        break;
    case RESPROTO_QUERY_CLASSES:
        reqstamp_end(seqno);
        class_query_response(client, seqno, msg, &cursor);
        break;
    case RESPROTO_QUERY_ZONES:
        reqstamp_end(seqno);
        zone_query_response(client, seqno, msg, &cursor);
        break;
    case RESPROTO_CREATE_RESOURCE_SET:
        reqstamp_end(seqno);
        create_resource_set_response(client, seqno, msg, &cursor);
        break;
    case RESPROTO_ACQUIRE_RESOURCE_SET:
        reqstamp_intermediate(seqno);
        acquire_resource_set_response(client, seqno, true, msg, &cursor);
        break;
    case RESPROTO_RELEASE_RESOURCE_SET:
        reqstamp_intermediate(seqno);
        acquire_resource_set_response(client, seqno, false, msg, &cursor);
        break;
    case RESPROTO_RESOURCES_EVENT:
        reqstamp_end(seqno);
        resource_event(client, seqno, msg, &cursor);
        break;
    default:
        mrp_log_error("ignoring unsupported request type %u", request);
        break;
    }
}

static void recv_msg(mrp_transport_t *t, mrp_msg_t *msg, void *user_data)
{
    return recvfrom_msg(t, msg, NULL, 0, user_data);
}


void closed_evt(mrp_transport_t *transp, int error, void *user_data)
{
    MRP_UNUSED(transp);
    MRP_UNUSED(user_data);

    if (error) {
        mrp_log_error("Connection closed with error %d (%s)", error,
                      strerror(error));
        exit(EIO);
    }
    else {
        mrp_log_info("Peer has closed the connection");
        exit(0);
    }
}


static void init_transport(client_t *client, char *addr)
{
    static mrp_transport_evt_t evt = {
        { .recvmsg     = recv_msg },
        { .recvmsgfrom = recvfrom_msg },
        .closed        = closed_evt,
        .connection    = NULL
    };

    client->alen = mrp_transport_resolve(NULL, addr, &client->saddr,
                                         sizeof(client->saddr),&client->atype);
    if (client->alen <= 0) {
        mrp_log_error("Can't resolve transport address '%s'", addr);
        exit(EINVAL);
    }

    client->transp = mrp_transport_create(client->ml, client->atype,
                                          &evt, client, 0);

    if (!client->transp) {
        mrp_log_error("Failed to create transport");
        exit(EIO);
    }

    if (!mrp_transport_connect(client->transp, &client->saddr, client->alen)) {
        mrp_log_error("Failed to connect to '%s'", addr);
        exit(EIO);
    }
}



static mrp_msg_t *create_request(uint32_t seqno, mrp_resproto_request_t req)
{
    uint16_t   type  = req;
    mrp_msg_t *msg;

    msg = mrp_msg_create(RESPROTO_SEQUENCE_NO , MRP_MSG_FIELD_UINT32, seqno,
                         RESPROTO_REQUEST_TYPE, MRP_MSG_FIELD_UINT16, type ,
                         RESPROTO_MESSAGE_END                               );

    if (!msg) {
        mrp_log_error("Unable to create new message");
        exit(ENOMEM);
    }

    reqstamp_start(seqno);

    return msg;
}

static void send_message(client_t *client, mrp_msg_t *msg)
{
    if (!mrp_transport_send(client->transp, msg)) {
        mrp_log_error("Failed to send message");
        exit(EIO);
    }

    mrp_msg_unref(msg);
}

static void query_resources(client_t *client)
{
    mrp_msg_t *req;

    req = create_request(client->seqno++, RESPROTO_QUERY_RESOURCES);

    send_message(client, req);
}

static void query_classes(client_t *client)
{
    mrp_msg_t *req;

    req = create_request(client->seqno++, RESPROTO_QUERY_CLASSES);

    send_message(client, req);
}

static void query_zones(client_t *client)
{
    mrp_msg_t *req;

    req = create_request(client->seqno++, RESPROTO_QUERY_ZONES);

    send_message(client, req);
}

static char *parse_attribute(mrp_msg_t *msg, char *str, char *sep)
{
#define PUSH_ATTRIBUTE_NAME(m, n) \
    mrp_msg_append(m, MRP_MSG_TAG_STRING(RESPROTO_ATTRIBUTE_NAME, n))

#define PUSH_ATTRIBUTE_VALUE(m, t, v) \
    mrp_msg_append(m, MRP_MSG_TAG_##t(RESPROTO_ATTRIBUTE_VALUE, v))


    char *p, *e, c;
    char *name;
    char  type;
    char *valstr;
    uint32_t unsignd;
    int32_t integer;
    double floating;


    *sep = '\0';

    if (!(p = str))
        return NULL;

    name = p;
    while ((c = *p++)) {
        if (c == ':') {
            *(p-1) = '\0';
            break;
        }
        if (!isalnum(c) && c != '_' && c != '-') {
            mrp_log_error("invalid attribute name: '%s'", name);
            return NULL;
        }
    }

    if (!c || !(type = *p++) || (*p++ != ':')) {
        mrp_log_error("invalid or missing resource type");
        return NULL;
    }

    if (*p == '\"') {
        valstr = ++p;
        while ((c = *p++) != '\"') {
            if (!c) {
                mrp_log_error("bad quoted value '%s'", valstr-1);
                return NULL;
            }
        }
        *(p-1) = '\0';
        if ((c = *p)) {
            if (c == '/' || c == ',')
                p++;
            else {
                mrp_log_error("invalid separator '%s'", p);
                return NULL;
            }
        }
    }
    else {
        valstr = p;
        while ((c = *p++)) {
            if (c == '/' || c == ',') {
                *(p-1) = '\0';
                break;
            }
            if (c < 0x20) {
                mrp_log_error("invalid attribute value '%s'", valstr);
                return NULL;
            }
        }
    }

    *sep = c;

    if (!PUSH_ATTRIBUTE_NAME(msg, name))
        goto error;

    if (type == 's') {
        if (!PUSH_ATTRIBUTE_VALUE(msg, STRING, valstr))
            goto error;
    }
    else if (type == 'i') {
        integer = strtol(valstr, &e, 10);

        if (*e || e == valstr || !PUSH_ATTRIBUTE_VALUE(msg, SINT32, integer))
            goto error;
    }
    else if (type == 'u') {
        unsignd = strtoul(valstr, &e, 10);

        if (*e || e == valstr || !PUSH_ATTRIBUTE_VALUE(msg, UINT32, unsignd))
            goto error;
    }
    else if (type == 'f') {
        floating = strtod(valstr, &e);

        if (*e || e == valstr || !PUSH_ATTRIBUTE_VALUE(msg, DOUBLE, floating))
            goto error;
    }


    return (p && *p) ? p : NULL;

 error:
    mrp_log_error("failed to build resource-set creation request");
    return NULL;

#undef PUSH_ATTRIBUTE_VALUE
#undef PUSH_ATTRIBUTE_NAME
}

bool parse_flags(char *str, uint32_t *pflags)
{
    typedef struct { char *str; uint32_t flags; } flagdef_t;

    static flagdef_t flagdefs[] = {
        { "M" ,    RESPROTO_RESFLAG_MANDATORY |            0            },
        { "O" ,               0               |            0            },
        { "S" ,    RESPROTO_RESFLAG_MANDATORY | RESPROTO_RESFLAG_SHARED },
        { "E" ,    RESPROTO_RESFLAG_MANDATORY |            0            },
        { "MS",    RESPROTO_RESFLAG_MANDATORY | RESPROTO_RESFLAG_SHARED },
        { "ME",    RESPROTO_RESFLAG_MANDATORY |            0            },
        { "OS",               0               | RESPROTO_RESFLAG_SHARED },
        { "OE",               0               |            0            },
        { "SM",    RESPROTO_RESFLAG_MANDATORY | RESPROTO_RESFLAG_SHARED },
        { "SO",               0               | RESPROTO_RESFLAG_SHARED },
        { "EM",    RESPROTO_RESFLAG_MANDATORY |            0            },
        { "EO",               0               |            0            },
        { NULL,               0               |            0            }
    };

    flagdef_t *fd;
    bool success;

    *pflags = RESPROTO_RESFLAG_MANDATORY;

    if (!str)
        success = true;
    else {
        for (success = false, fd = flagdefs;    fd->str;    fd++) {
            if (!strcasecmp(str, fd->str)) {
                success = true;
                *pflags = fd->flags;
                break;
            }
        }
    }

    return success;
}

static char *parse_resource(mrp_msg_t *msg, char *str, char *sep)
{
#define PUSH(msg, tag, typ, val) \
    mrp_msg_append(msg, MRP_MSG_TAG_##typ(RESPROTO_##tag, val))

    uint32_t flags;
    char *name, *flgstr;
    char *p;
    char  c;

    *sep = '\0';

    if (!(p = str))
        return NULL;

    name = p;
    flgstr = NULL;

    while ((c = *p++)) {
        if (c == ':') {
            *(p-1) = '\0';
            flgstr = name;
            name = p;
        }
        else if (c == '/' || c == ',') {
            *(p-1) = '\0';
            break;
        }
        else if (!isalnum(c) && c != '_' && c != '-') {
            mrp_log_error("invalid resource name: '%s'", name);
            return NULL;
        }
    }

    if (!parse_flags(flgstr, &flags)) {
        mrp_log_error("invalid flag string '%s'", flgstr ? flgstr : "");
        return NULL;
    }

    if (!PUSH(msg, RESOURCE_NAME , STRING, name ) ||
        !PUSH(msg, RESOURCE_FLAGS, UINT32, flags)   )
        goto failed;

    if (!c)
        p--;
    else {
        while ((*sep = c) == '/')
            p = parse_attribute(msg, p, &c);
    }

    if (!PUSH(msg, SECTION_END, UINT8, 0))
        goto failed;

    return (p && *p) ? p : NULL;

 failed:
    mrp_log_error("failed to build resource-set creation request");
    *sep = '\0';
    return NULL;

#undef PUSH
}

static void create_resource_set(client_t   *client,
                                const char *class,
                                const char *zone,
                                const char *def,
                                uint32_t    flags,
                                uint32_t    priority)
{
#define PUSH(msg, tag, typ, val) \
    mrp_msg_append(msg, MRP_MSG_TAG_##typ(RESPROTO_##tag, val))

    char  *buf;
    mrp_msg_t *req;
    char *p;
    char c;

    /* 'def' => {m|o}{s|e}:resource_name/attr_name:{s|i|u|f}:["]value["] */

    if (!client || !class || !zone || !def)
        return;

    req = create_request(client->seqno++, RESPROTO_CREATE_RESOURCE_SET);

    if (!PUSH(req, RESOURCE_FLAGS   , UINT32, flags   ) ||
        !PUSH(req, RESOURCE_PRIORITY, UINT32, priority) ||
        !PUSH(req, CLASS_NAME       , STRING, class   ) ||
        !PUSH(req, ZONE_NAME        , STRING, zone    )   )
    {
        mrp_msg_unref(req);
    }
    else {
        p = buf = mrp_strdup(def);
        c = ',';

        while (c == ',')
            p = parse_resource(req, p, &c);

        if (client->msgdump)
            mrp_msg_dump(req, stdout);

        send_message(client, req);

        mrp_free(buf);
    }

#undef PUSH
}

static uint32_t acquire_resource_set(client_t *client, bool acquire)
{
#define PUSH(msg, tag, typ, val) \
    mrp_msg_append(msg, MRP_MSG_TAG_##typ(RESPROTO_##tag, val))

    uint16_t   tag;
    uint32_t   reqno;
    mrp_msg_t *req;

    if (!client || client->rset_id == INVALID_ID)
        return 0;

    if (acquire)
        tag = RESPROTO_ACQUIRE_RESOURCE_SET;
    else
        tag = RESPROTO_RELEASE_RESOURCE_SET;

    req = create_request((reqno = client->seqno++), tag);

    if (!PUSH(req, RESOURCE_SET_ID, UINT32, client->rset_id))
        mrp_msg_unref(req);
    else {
        if (client->msgdump)
            mrp_msg_dump(req, stdout);

        send_message(client, req);
    }

    return reqno;

#undef PUSH
}

static void print_prompt(client_t *client, bool startwith_lf)
{
    if (client && client->prompt) {
        printf("%s%s>", startwith_lf ? "\n":"", client->name);
        fflush(stdout);
    }
}

static void print_command_help(void)
{
    printf("\nAvailable commands:\n");
    printf("   help\t\tprints this help\n");
    printf("   quit\t\texits\n");
    printf("   resources\tprints the resource definitions\n");
    printf("   classes\tprints the application classes\n");
    printf("   zones\tprints the zones\n");
    printf("   acquire\tacquires the resource-set specified by command "
           "line options\n");
    printf("   release\treleases the resource-set specified by command "
           "line options\n");
}

static void parse_line(client_t *client, char *buf, int len)
{
    char *p, *e;

    if (len <= 0)
        print_prompt(client, false);
    else {
        for (p = buf;  isblank(*p);  p++) ;
        for (e = buf+len;  e > buf && isblank(*(e-1));  e--) ;

        *e = '\0';

        if (!strcmp(p, "help")) {
            print_command_help();
            print_prompt(client, true);
        }
        else if (!strcmp(p, "quit") || !strcmp(p, "exit")) {
            printf("\n");
            mrp_mainloop_quit(client->ml, 0);
        }
        else if (!strcmp(p, "resources")) {
            client->prompt = false;
            printf("   querying resource definitions\n");
            query_resources(client);
        }
        else if (!strcmp(p, "classes")) {
            client->prompt = false;
            printf("   querying application classes\n");
            query_classes(client);
        }
        else if (!strcmp(p, "zones")) {
            client->prompt = false;
            printf("   querying zones\n");
            query_zones(client);
        }
        else if (!strcmp(p, "acquire")) {
            if (client->rset_id == INVALID_ID) {
                printf("   there is no resource set\n");
                print_prompt(client, true);
            }
            else {
                client->prompt = false;
                printf("   acquiring resource set %u. request no %u\n",
                       client->rset_id, acquire_resource_set(client, true));
            }
        }
        else if (!strcmp(p, "release")) {
            if (client->rset_id == INVALID_ID) {
                printf("   there is no resource set\n");
                print_prompt(client, true);
            }
            else {
                client->prompt = false;
                printf("   releasing resource set %u. request no %u\n",
                       client->rset_id, acquire_resource_set(client, false));
            }
        }
        else {
            printf("   unsupported command\n");
            print_prompt(client, true);
        }
    }
}

static void console_input(mrp_io_watch_t *w, int fd, mrp_io_event_t events,
                          void *user_data)
{
    static char  buf[512];
    static char *bufend = buf + (sizeof(buf) - 1);
    static char *writep = buf;

    client_t *client = (client_t *)user_data;
    int       len;
    char     *eol;

    MRP_UNUSED(w);
    MRP_UNUSED(events);

    MRP_ASSERT(client, "invalid argument");
    MRP_ASSERT(fd == 0, "confused with data structures");

    while ((len = read(fd, writep, bufend-writep)) < 0) {
        if (errno != EINTR) {
            mrp_log_error("read error %d: %s", errno, strerror(errno));
            return;
        }
    }

    *(writep += len) = '\0';

    while ((eol = strchr(buf, '\n'))) {
        *eol++ = '\0';

        parse_line(client, buf, (eol-buf)-1);

        if ((len = writep - eol) <= 0) {
            writep = buf;
            break;
        }
        else {
            memmove(buf, eol, len);
            writep = buf + len;
        }
    }
}

static void sighandler(mrp_sighandler_t *h, int signum, void *user_data)
{
    mrp_mainloop_t *ml     = mrp_get_sighandler_mainloop(h);
    client_t       *client = (client_t *)user_data;

    MRP_UNUSED(h);

    MRP_ASSERT(client, "invalid argument");

    switch (signum) {

    case SIGHUP:
    case SIGTERM:
    case SIGINT:
        if (ml)
            mrp_mainloop_quit(ml, 0);
        break;

    default:
        break;
    }
}

static void usage(client_t *client, int exit_code)
{
    printf("Usage: "
           "%s [-h] [-v] [-r] [-a] [-w] [-p pri] [class zone resources]\n"
           "\nwhere\n"
           "\t-h\t\tprints this help\n"
           "\t-v\t\tverbose mode (dumps the transport messages)\n"
           "\t-a\t\tautoacquire mode\n"
           "\t-w\t\tdont wait for resources if they were not available\n"
           "\t-r\t\tautorelease mode\n"
           "\t-p priority\t\tresource set priority (priority is 0-7)\n"
           "\tclass\t\tapplication class of the resource set\n"
           "\tzone\t\tzone wher the resource set lives\n"
           "\tresources\tcomma separated list of resources. Each resource is\n"
           "\t\t\tspecified as flags:name[/attribute[/ ... ]]\n"
           "\t\t\tflags\t\tspecified as {m|o}{s|e} where\n"
           "\t\t\t\t\t'm' stands for mandatory,\n"
           "\t\t\t\t\t'o' for optional,\n"
           "\t\t\t\t\t's' for shared and\n"
           "\t\t\t\t\t'e' for exclusive.\n"
           "\t\t\tresource\tis the name of the resource composed of\n"
           "\t\t\t\t\ta series of letters, digits, '_' and\n"
           "\t\t\t\t\t'-' characters\n"
           "\t\t\tattribute\tis defined as attr-name:type:[\"]value[\"]\n"
           "\t\t\t\t\ttypes can be\n"
           "\t\t\t\t\t's' - string\n"
           "\t\t\t\t\t'i' - signed integer\n"
           "\t\t\t\t\t'u' - unsigned integer\n"
           "\t\t\t\t\t'f' - floating\n"
           "\nExample:\n\n%s player driver "
           "ms:audio_playback/role:s:\"video\",me:video_playback\n"
           "\n", client->name, client->name);

    exit(exit_code);
}

static void parse_arguments(client_t *client, int argc, char **argv)
{
    unsigned long pri;
    char *e;
    int opt;

    while ((opt = getopt(argc, argv, "hvrawp:")) != -1) {
        switch (opt) {
        case 'h':
            usage(client, 0);
        case 'v':
            client->msgdump = true;
            break;
        case 'a':
            client->rsetf |= RESPROTO_RSETFLAG_AUTOACQUIRE;
            break;
        case 'r':
            client->rsetf |= RESPROTO_RSETFLAG_AUTORELEASE;
            break;
        case 'w':
            client->rsetf |= RESPROTO_RSETFLAG_DONTWAIT;
            break;
        case 'p':
            pri = strtoul(optarg, &e, 10);
            if (e == optarg || *e || pri > 7)
                usage(client, EINVAL);
            else
                client->priority = pri;
            break;
        default:
            usage(client, EINVAL);
        }
    }

    if (optind + 3 == argc) {
        client->class = argv[optind + 0];
        client->zone  = argv[optind + 1];
        client->rsetd = argv[optind + 2];
    }
    else if (optind < argc) {
        usage(client, EINVAL);
    }
}


int main(int argc, char **argv)
{
    client_t *client = mrp_allocz(sizeof(client_t));
    char     *addr = RESPROTO_DEFAULT_ADDRESS;

    mrp_log_set_mask(MRP_LOG_UPTO(MRP_LOG_DEBUG));
    mrp_log_set_target(MRP_LOG_TO_STDOUT);

    client->name    = mrp_strdup(basename(argv[0]));
    client->ml      = mrp_mainloop_create();
    client->seqno   = 1;
    client->prompt  = false;
    client->rset_id = INVALID_ID;

    if (!client->ml || !client->name)
        exit(1);

    parse_arguments(client, argc, argv);

    mrp_add_sighandler(client->ml, SIGHUP , sighandler, client);
    mrp_add_sighandler(client->ml, SIGTERM, sighandler, client);
    mrp_add_sighandler(client->ml, SIGINT , sighandler, client);

    init_transport(client, addr);


    if (!client->class || !client->zone || !client->rsetd)
        print_prompt(client, false);
    else {
        create_resource_set(client, client->class, client->zone,
                            client->rsetd, client->rsetf, client->priority);
    }

    mrp_add_io_watch(client->ml, 0, MRP_IO_EVENT_IN, console_input, client);

    mrp_mainloop_run(client->ml);

    if (reqcount > 0)
        printf("%u requests, avarage request processing time %.2lfmsec\n",
               reqcount, (double)(totaltime / (uint64_t)reqcount) / 1000.0);

    printf("exiting now ...\n");

    mrp_transport_destroy(client->transp);

    mrp_mainloop_destroy(client->ml);
    mrp_free((void *)client->name);
    resource_def_array_free(client->resources);
    str_array_free(client->class_names);
    str_array_free(client->zone_names);
    mrp_free(client);
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
