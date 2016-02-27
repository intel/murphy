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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <murphy/common/macros.h>
#include <murphy/common/transport.h>
#include <murphy/common/wsck-transport.h>
#include <murphy/common/json.h>
#include <murphy/core/plugin.h>
#include <murphy/core/lua-bindings/murphy.h>

#include <murphy/resource/client-api.h>
#include <murphy/resource/config-api.h>
#include <murphy/resource/manager-api.h>
#include <murphy/resource/protocol.h>

#include "resource-wrt.h"
#include "config.h"

#ifdef MURPHY_DATADIR                             /* default content dir */
#    define DEFAULT_HTTPDIR MURPHY_DATADIR"/resource-wrt"
#else
#    define DEFAULT_HTTPDIR "/usr/share/murphy/resource-wrt"
#endif

#define DEFAULT_ADDRESS "wsck:127.0.0.1:4000/murphy"
#define ATTRIBUTE_MAX   MRP_ATTRIBUTE_MAX

/*
 * plugin argument indices
 */

enum {
    ARG_ADDRESS,                         /* transport address to use */
    ARG_HTTPDIR,                         /* content directory for HTTP */
    ARG_SSLCERT,                         /* path to SSL certificate */
    ARG_SSLPKEY,                         /* path to SSL private key */
    ARG_SSLCA                            /* path to SSL CA */
};


/*
 * WRT resource context
 */

typedef struct {
    mrp_context_t   *ctx;                /* murphy context */
    mrp_transport_t *lt;                 /* transport we listen on */
    const char      *addr;               /* address we listen on */
    mrp_list_hook_t  clients;            /* connected clients */
    int              id;                 /* next client id */
    const char      *httpdir;            /* WRT-resource agent directory */
    const char      *sslcert;            /* path to SSL certificate */
    const char      *sslpkey;            /* path to SSL private key */
    const char      *sslca;              /* path to SSL CA */

} wrt_data_t;


typedef struct {
    int                    id;           /* client id */
    int                    seq;          /* last request sequence number */
    mrp_context_t         *ctx;          /* murphy context */
    mrp_transport_t       *t;            /* client transport */
    mrp_resource_client_t *rsc;          /* resource client */
    mrp_list_hook_t        hook;         /* to list of clients */
    /*
     * Notes:
     *    The resource infra sends the first event for a resource set
     *    out-of-order, before it has acknowledged the creation of the
     *    set and let the client know the resource set id. We use the
     *    field below to suppress this event and force emitting an event
     *    for the resource set right after we have acked the creation request.
     */
    mrp_resource_set_t *rset;            /* resource set being created */
    int                 force_all;       /* flag for */
} wrt_client_t;


typedef struct {
    const char *name;
    bool        mand;
    bool        share;
    mrp_attr_t  attrs[ATTRIBUTE_MAX + 1];
    int         nattr;
} resdef_t;


typedef struct {
    const char *name;
    uint32_t    flag;
} flagdef_t;


typedef struct {
    int  err;
    char msg[256];
} errbuf_t;


static int send_message(wrt_client_t *c, mrp_json_t *msg);

static void ignore_invalid_request(wrt_client_t *c, mrp_json_t *req, ...)
{
    MRP_UNUSED(c);
    MRP_UNUSED(req);

    mrp_log_error("Ignoring invalid WRT resource request");
}


static void ignore_unknown_request(wrt_client_t *c, mrp_json_t *req,
                                   const char *type)
{
    MRP_UNUSED(c);
    MRP_UNUSED(req);

    mrp_log_error("Ignoring unknown WRT resource request '%s'", type);
}


static int error(errbuf_t *e, int code, const char *format, ...)
{
    va_list ap;

    errno = e->err = code;

    va_start(ap, format);
    vsnprintf(e->msg, sizeof(e->msg), format, ap);
    va_end(ap);

    return code;
}


static mrp_json_t *alloc_reply(const char *type, int seq)
{
    mrp_json_t *reply;

    reply = mrp_json_create(MRP_JSON_OBJECT);

    if (reply != NULL) {
        if (mrp_json_add_string (reply, "type", type) &&
            mrp_json_add_integer(reply, "seq" , seq))
            return reply;
        else
            mrp_json_unref(reply);
    }

    mrp_log_error("Failed to allocate WRT resource reply.");

    return NULL;
}


static void error_reply(wrt_client_t *c, const char *type, int seq, int code,
                        const char *fmt, ...)
{
    mrp_json_t *reply;
    char        errmsg[256];
    va_list     ap;

    reply = mrp_json_create(MRP_JSON_OBJECT);

    if (reply != NULL) {
        va_start(ap, fmt);
        vsnprintf(errmsg, sizeof(errmsg), fmt, ap);
        errmsg[sizeof(errmsg) - 1] = '\0';
        va_end(ap);

        if (mrp_json_add_string (reply, "type"   , type) &&
            mrp_json_add_integer(reply, "seq"    , seq ) &&
            mrp_json_add_integer(reply, "error"  , code) &&
            mrp_json_add_string (reply, "message", errmsg))
            send_message(c, reply);

        mrp_json_unref(reply);
    }
}


static void query_resources(wrt_client_t *c, mrp_json_t *req)
{
    const char  *type = RESWRT_QUERY_RESOURCES;
    mrp_json_t  *reply, *rarr, *r, *ao;
    const char **resources;
    int          seq, cnt;
    mrp_attr_t  *attrs, *a;
    mrp_attr_t   buf[ATTRIBUTE_MAX];
    uint32_t     id;
    bool         sync_release;

    if (!mrp_json_get_integer(req, "seq", &seq)) {
        ignore_invalid_request(c, req, "missing 'seq' field");
        return;
    }

    rarr = r = ao = NULL;
    resources = mrp_resource_definition_get_all_names(0, NULL);

    if (resources == NULL) {
        error_reply(c, type, seq, ENOMEM, "failed to query class names");
        return;
    }

    reply = alloc_reply(type, seq);

    if (reply == NULL)
        return;

    rarr = mrp_json_create(MRP_JSON_ARRAY);

    if (rarr == NULL)
        goto fail;

    for (id = 0; resources[id]; id++) {
        r = mrp_json_create(MRP_JSON_OBJECT);

        if (r == NULL)
            goto fail;

        if (!mrp_json_add_string (r, "name", resources[id]))
            goto fail;

        sync_release = mrp_resource_definition_get_sync_release(id);
        if (!mrp_json_add_boolean(r, "sync_release", sync_release))
            goto fail;

        attrs = mrp_resource_definition_read_all_attributes(id,
                                                            ATTRIBUTE_MAX, buf);

        ao = mrp_json_create(MRP_JSON_OBJECT);

        if (ao == NULL)
            goto fail;

        for (a = attrs, cnt = 0; a->name; a++, cnt++) {
            switch (a->type) {
            case mqi_string:
                if (!mrp_json_add_string(ao, a->name, a->value.string))
                    goto fail;

                break;
            case mqi_integer:
            case mqi_unsignd:
                if (!mrp_json_add_integer(ao, a->name, a->value.integer))
                    goto fail;
                break;
            case mqi_floating:
                if (!mrp_json_add_double(ao, a->name, a->value.floating))
                    goto fail;
                break;
            default:
                mrp_log_error("attribute '%s' of resource '%s' "
                              "has unknown type %d", a->name, resources[id],
                              a->type);
                break;
            }
        }

        if (cnt > 0)
            mrp_json_add(r, "attributes", ao);
        else
            mrp_json_unref(ao);

        ao = NULL;

        if (!mrp_json_array_append(rarr, r))
            goto fail;
        else
            r = NULL;
    }

    if (mrp_json_add_integer(reply, "status"   , 0)) {
        mrp_json_add        (reply, "resources", rarr);
        send_message(c, reply);
    }

    mrp_json_unref(reply);
    mrp_free(resources);
    return;

 fail:
    mrp_json_unref(reply);
    mrp_json_unref(rarr);
    mrp_json_unref(r);
    mrp_json_unref(ao);
    mrp_free(resources);
}


static void query_classes(wrt_client_t *c, mrp_json_t *req)
{
    const char  *type = RESWRT_QUERY_CLASSES;
    mrp_json_t  *reply;
    const char **classes;
    size_t       nclass;
    int          seq;

    if (!mrp_json_get_integer(req, "seq", &seq)) {
        ignore_invalid_request(c, req, "missing 'seq' field");
        return;
    }

    classes = mrp_application_class_get_all_names(0, NULL);

    if (classes == NULL) {
        error_reply(c, type, seq, ENOMEM, "failed to query class names");
        return;
    }

    reply = alloc_reply(type, seq);

    if (reply == NULL)
        return;

    nclass = 0;
    for (nclass = 0; classes[nclass] != NULL; nclass++)
        ;

    if (mrp_json_add_integer     (reply, "status" , 0) &&
        mrp_json_add_string_array(reply, "classes", classes, nclass))
        send_message(c, reply);

    mrp_json_unref(reply);
    mrp_free(classes);
}


static void query_zones(wrt_client_t *c, mrp_json_t *req)
{
    const char  *type = RESWRT_QUERY_ZONES;
    mrp_json_t  *reply;
    const char **zones;
    size_t       nzone;
    int          seq;

    if (!mrp_json_get_integer(req, "seq", &seq)) {
        ignore_invalid_request(c, req, "missing 'seq' field");
        return;
    }

    zones = mrp_zone_get_all_names(0, NULL);

    if (zones == NULL) {
        error_reply(c, type, seq, ENOMEM, "failed to query zone names");
        return;
    }

    reply = alloc_reply(type, seq);

    if (reply == NULL)
        mrp_log_error("Failed to allocate WRT resource reply.");

    nzone = 0;
    for (nzone = 0; zones[nzone] != NULL; nzone++)
        ;

    if (mrp_json_add_integer     (reply, "status", 0) &&
        mrp_json_add_string_array(reply, "zones" , zones, nzone))
        send_message(c, reply);

    mrp_json_unref(reply);
    mrp_free(zones);
}


static int parse_attributes(mrp_json_t *ja, mrp_attr_t *attrs, size_t max,
                            errbuf_t *e)
{
    mrp_json_iter_t  it;
    mrp_json_t      *v;
    const char      *k;
    mrp_attr_t      *attr;
    int              nattr, cnt;

    nattr = mrp_json_array_length(ja);

    if (nattr >= (int)max)
        return -error(e, EOVERFLOW, "too many attributes (%d > %d)", nattr,
                      max - 1);

    cnt  = 0;
    attr = attrs;
    mrp_json_foreach_member(ja, k, v, it) {
        attr->name = k;

        switch (mrp_json_get_type(v)) {
        case MRP_JSON_STRING:
            attr->type = mqi_string;
            attr->value.string = mrp_json_string_value(v);
            break;
        case MRP_JSON_INTEGER:
            attr->type = mqi_integer;
            attr->value.integer = mrp_json_integer_value(v);
            break;
        case MRP_JSON_DOUBLE:
            attr->type = mqi_floating;
            attr->value.floating = mrp_json_double_value(v);
            break;
        case MRP_JSON_BOOLEAN:
            attr->type = mqi_integer;
            attr->value.integer = mrp_json_boolean_value(v);
            break;
        default:
            return -error(e, EINVAL, "attribute '%s' with invalid type", k);
        }

        cnt++;
        attr++;
    }
    attr->name = NULL;

    return cnt;
}


static int append_attributes(mrp_json_t *o, mrp_attr_t *attrs, errbuf_t *e)
{
    mrp_json_t *a;
    mrp_attr_t *attr;

    if (attrs->name == NULL)
        return 0;

    a = mrp_json_create(MRP_JSON_OBJECT);

    if (a == NULL)
        return error(e, ENOMEM, "failed to create attributes object");

    for (attr = attrs; attr->name != NULL; attr++) {
        switch (attr->type) {
        case mqi_string:
            if (!mrp_json_add_string(a, attr->name, attr->value.string))
                goto fail;
            break;
        case mqi_integer:
            if (!mrp_json_add_integer(a, attr->name, attr->value.integer))
                goto fail;
            break;
        case mqi_unsignd:
            if (!mrp_json_add_integer(a, attr->name, attr->value.integer))
                goto fail;
            break;
        case mqi_floating:
            if (!mrp_json_add_double(a, attr->name, attr->value.floating))
                goto fail;
            break;
        default:
            goto fail;
        }
    }

    mrp_json_add(o, "attributes", a);
    return 0;

 fail:
    mrp_json_unref(a);

    return error(e, EINVAL, "failed to append attribtues");
}


static int parse_flags(mrp_json_t *arr, flagdef_t *defs, uint32_t *flagsp,
                       errbuf_t *e)
{
    const char *name;
    flagdef_t  *d;
    int         i, cnt;
    uint32_t    flags;

    flags = 0;
    cnt   = mrp_json_array_length(arr);

    for (i = 0; i < cnt; i++) {
        if (mrp_json_array_get_string(arr, i, &name)) {
            for (d = defs; d->name != NULL; d++)
                if (!strcmp(d->name, name))
                    break;

            if (d->name == NULL)
                return -error(e, EINVAL, "unknown flag '%s'", name);

            flags |= d->flag;
        }
        else
            return -error(e, EINVAL, "flags must be strings");
    }

    *flagsp = flags;
    return 0;
}


static int parse_resource_definition(mrp_json_t *jr, resdef_t *d, errbuf_t *e)
{
#define OPTIONAL 0x1
#define SHARED   0x2
    static flagdef_t res_flags[] = {
        { "optional", 0x1 },
        { "shared"  , 0x2 },
        { NULL, 0 }
    };
    mrp_json_t *jf, *ja;
    uint32_t    flags;

    if (!mrp_json_get_string(jr, "name", &d->name))
        return -error(e, EINVAL, "missing resource name");

    if (mrp_json_get_array(jr, "flags", &jf)) {
        if (parse_flags(jf, res_flags, &flags, e) != 0)
            return e->err;
    }
    else {
        if (errno != ENOENT)
            return -error(e, EINVAL, "invalid resource flags");
        else
            flags = 0;
    }

    d->mand  = !(flags & OPTIONAL);
    d->share =   flags & SHARED;

    if (mrp_json_get_object(jr, "attributes", &ja)) {
        d->nattr = parse_attributes(ja, d->attrs, MRP_ARRAY_SIZE(d->attrs), e);

        if (d->nattr < 0)
            return -e->err;
    }
    else {
        if (errno != ENOENT)
            return -error(e, EINVAL, "invalid resource attributes");
        else {
            d->attrs[0].name = NULL;
            d->nattr = 0;
        }
    }

    return 0;
#undef OPTIONAL
#undef SHARED
}


static int block_resource_set_events(wrt_client_t *c, mrp_resource_set_t *rset)
{
    if (c->rset == NULL) {
        c->rset = rset;
        return TRUE;
    }
    else
        return FALSE;
}


static int allow_resource_set_events(wrt_client_t *c, mrp_resource_set_t *rset)
{
    if (c->rset == rset) {
        c->rset = NULL;
        return TRUE;
    }
    else
        return FALSE;
}


static int resource_set_events_blocked(wrt_client_t *c, mrp_resource_set_t *rs)
{
    return c->rset == rs;
}


static void emit_resource_set_event(wrt_client_t *c, uint32_t reqid,
                                    mrp_resource_set_t *rset, int force_all)
{
    const char     *type = RESWRT_EVENT;
    int             seq  = (int)reqid;
    mrp_json_t     *msg, *rarr, *r;
    int             rsid;
    const char     *state;
    int             grant, pending_release, pending_acquire, advice, all, mask;
    errbuf_t        e;
    mrp_resource_t *res;
    void           *it;
    const char     *name;
    mrp_attr_t      attrs[ATTRIBUTE_MAX + 1];

    mrp_debug("event for resource set %p of client %p", rset, c);

    if (resource_set_events_blocked(c, rset)) {
        mrp_debug("suppressing event for unacknowledged resource set");
        return;
    }

    pending_release = (int)mrp_get_resource_set_pending_release(rset);
    pending_acquire = (int)mrp_get_resource_set_pending_acquire(rset);

    if (pending_acquire && !pending_release) {
        mrp_debug("not emitting event for resource set that is pending acquisition");
        return;
    }
    else if (mrp_get_resource_set_state(rset) == mrp_resource_acquire)
        state = RESWRT_STATE_GRANTED;
    else
        state = RESWRT_STATE_RELEASE;

    rsid    = (int)mrp_get_resource_set_id(rset);
    grant   = (int)mrp_get_resource_set_grant(rset);
    advice  = (int)mrp_get_resource_set_advice(rset);

    msg = alloc_reply(type, seq);

    if (msg == NULL)
        return;

    rarr = r = NULL;

    if (mrp_json_add_integer(msg, "id"      , rsid   ) &&
        mrp_json_add_string (msg, "state"   , state  ) &&
        mrp_json_add_integer(msg, "grant"   , grant  ) &&
        mrp_json_add_integer(msg, "pending" , pending_release) &&
        mrp_json_add_integer(msg, "advice"  , advice)) {

        all = grant | advice;
        it  = NULL;

        while ((res = mrp_resource_set_iterate_resources(rset, &it)) != NULL) {
            mask = mrp_resource_get_mask(res);

            if (!(mask & all) && !force_all)
                continue;

            name = mrp_resource_get_name(res);

            if (!mrp_resource_read_all_attributes(res, ATTRIBUTE_MAX+1, attrs))
                goto fail;

            if (rarr == NULL) {
                rarr = mrp_json_create(MRP_JSON_ARRAY);
                if (rarr == NULL)
                    goto fail;
            }

            r = mrp_json_create(MRP_JSON_OBJECT);

            if (r == NULL)
                goto fail;

            if (!mrp_json_add_string (r, "name", name) ||
                (force_all && !mrp_json_add_integer(r, "mask", mask)))
                goto fail;

            if (append_attributes(r, attrs, &e) != 0)
                goto fail;

            if (!mrp_json_array_append(rarr, r))
                goto fail;
            else
                r = NULL;
        }

        if (rarr != NULL)
            mrp_json_add(msg, "resources", rarr);

        send_message(c, msg);
        mrp_json_unref(msg);

        return;
    }

 fail:
    mrp_json_unref(msg);
    mrp_json_unref(rarr);
    mrp_json_unref(r);
}


static void event_cb(uint32_t reqid, mrp_resource_set_t *rset, void *user_data)
{
    emit_resource_set_event((wrt_client_t *)user_data, reqid, rset, FALSE);
}


static void create_set(wrt_client_t *c, mrp_json_t *req)
{
    static flagdef_t set_flags[] = {
        { "autorelease", TRUE },
        { NULL, 0 }
    };
    const char *type = RESWRT_CREATE_SET;
    int         seq;
    mrp_json_t *reply;
    errbuf_t    e;
    mrp_json_t *jf, *jra, *jr;
    uint32_t    flags = 0, priority, rsid;
    bool        autorelease;
    bool        dontwait;
    const char *appclass, *zone;
    char        attr[1024], *p;
    resdef_t    r;
    int         i, j, cnt, n, l;
    mrp_resource_set_t *rset;

    if (!mrp_json_get_integer(req, "seq", &seq)) {
        ignore_invalid_request(c, req, "missing 'seq' field");
        return;
    }

    /* parse resource set flags */
    if (mrp_json_get_array(req, "flags", &jf)) {
        if (parse_flags(jf, set_flags, &flags, &e) != 0) {
            error_reply(c, type, seq, e.err, e.msg);
            return;
        }
    }

    autorelease = (flags != 0);
    dontwait    = false;
    mrp_debug("autorelease: %s", autorelease ? "true" : "false");

    /* dig out priority, class, and zone */
    if (mrp_json_get_integer(req, "priority", &priority))
        mrp_debug("priority: %u", priority);
    else {
        error_reply(c, type, seq, EINVAL, "missing or invalid 'priority'");
        return;
    }

    if (mrp_json_get_string(req, "class", &appclass))
        mrp_debug("class: '%s'", appclass);
    else {
        error_reply(c, type, seq, EINVAL, "missing or invalid 'class'");
        return;
    }

    if (mrp_json_get_string(req, "zone", &zone))
        mrp_debug("zone: '%s'", zone);
    else {
        error_reply(c, type, seq, EINVAL, "missing or invalid 'zone'");
        return;
    }

    /* dig out resources */
    if (!mrp_json_get_array(req, "resources", &jra) ||
        (cnt = mrp_json_array_length(jra)) <= 0) {
        error_reply(c, type, seq, EINVAL, "missing or invalid 'resources'");
        return;
    }

    /* create a new resource set */
    rset = mrp_resource_set_create(c->rsc, autorelease, dontwait,
                                   priority, event_cb, c);

    if (rset != NULL) {
        rsid = mrp_get_resource_set_id(rset);

        /* add resources to set */
        for (i = 0; i < cnt; i++) {
            if (mrp_json_array_get_object(jra, i, &jr)) {
                if (parse_resource_definition(jr, &r, &e) != 0) {
                    ignore_invalid_request(c, req, e.msg);
                    goto fail;
                }
                else {
                    mrp_debug("resource '%s': %s %s", r.name,
                              r.mand  ? "mandatory" : "optional",
                              r.share ? "shared"    : "exclusive");

                    for (j = 0; j < r.nattr; j++) {
                        p  = attr;
                        n  = sizeof(attr);
                        l  = snprintf(p, n, "'%s' = ", r.attrs[j].name);
                        p += l;
                        n -= l;

                        switch (r.attrs[j].type) {
                        case mqi_string:
                            l = snprintf(p, n, "'%s'", r.attrs[j].value.string);
                            p += l;
                            n -= l;
                            break;
                        case mqi_integer:
                            l = snprintf(p, n, "%d", r.attrs[j].value.integer);
                            p += l;
                            n -= l;
                            break;
                        case mqi_floating:
                            l = snprintf(p, n, "%f", r.attrs[j].value.floating);
                            p += l;
                            n -= l;
                            break;
                        default:
                            l = snprintf(p, n, "<unsupported type>");
                            p += l;
                            n -= l;
                        }

                        if (n > 0)
                            mrp_debug("    attribute %s", attr);
                    }

                    if (mrp_resource_set_add_resource(rset, r.name, r.share,
                                                      r.attrs, r.mand) < 0) {
                        error_reply(c, type, seq, EINVAL,
                                    "failed to add resource %s to set", r.name);
                        goto fail;
                    }
                }
            }
        }

        block_resource_set_events(c, rset);
        /* suppress events for this resource set (client does not know id) */
        c->rset = rset;

        /* add resource set to class/zone */
        if (mrp_application_class_add_resource_set(appclass, zone, rset, seq)) {
            error_reply(c, type, seq, EINVAL, "failed to add set to class");
            goto fail;
        }
        else {
            reply = alloc_reply(type, seq);

            if (reply != NULL) {
                if (mrp_json_add_integer(reply, "status",0) &&
                    mrp_json_add_integer(reply, "id", rsid)) {
                    send_message(c, reply);

                    allow_resource_set_events(c, rset);
                    emit_resource_set_event(c, seq, rset, TRUE);
                }
            }

            mrp_json_unref(reply);

            return;
        }
    }

 fail:
    mrp_resource_set_destroy(rset);
}


static void destroy_set(wrt_client_t *c, mrp_json_t *req)
{
    const char         *type = RESWRT_DESTROY_SET;
    int                 seq;
    mrp_json_t         *reply;
    mrp_resource_set_t *rset;
    uint32_t            rsid;

    if (!mrp_json_get_integer(req, "seq", &seq)) {
        ignore_invalid_request(c, req, "missing 'seq' field");
        return;
    }

    /* get resource set id */
    if (!mrp_json_get_integer(req, "id", &rsid)) {
        error_reply(c, type, seq, EINVAL, "missing id");
        return;
    }

    rset = mrp_resource_client_find_set(c->rsc, rsid);

    if (rset != NULL) {
        reply = alloc_reply(type, seq);

        if (reply != NULL) {
            if (mrp_json_add_integer(reply, "status", 0))
                send_message(c, reply);
        }

        mrp_resource_set_destroy(rset);
    }
    else
        error_reply(c, type, seq, ENOENT, "resource set %d not found", rsid);
}


static void acquire_set(wrt_client_t *c, mrp_json_t *req)
{
    const char         *type = RESWRT_ACQUIRE_SET;
    int                 seq;
    mrp_json_t         *reply;
    mrp_resource_set_t *rset;
    uint32_t            rsid;

    if (!mrp_json_get_integer(req, "seq", &seq)) {
        ignore_invalid_request(c, req, "missing 'seq' field");
        return;
    }

    /* get resource set id */
    if (!mrp_json_get_integer(req, "id", &rsid)) {
        error_reply(c, type, seq, EINVAL, "missing id");
        return;
    }

    rset = mrp_resource_client_find_set(c->rsc, rsid);

    if (rset != NULL) {
        reply = alloc_reply(type, seq);

        if (reply != NULL) {
            if (mrp_json_add_integer(reply, "status", 0))
                send_message(c, reply);
        }

        mrp_resource_set_acquire(rset, (uint32_t)seq);
    }
    else
        error_reply(c, type, seq, ENOENT, "resource set %d not found", rsid);
}


static void release_set(wrt_client_t *c, mrp_json_t *req)
{
    const char         *type = RESWRT_RELEASE_SET;
    int                 seq;
    mrp_json_t         *reply;
    mrp_resource_set_t *rset;
    uint32_t            rsid;

    if (!mrp_json_get_integer(req, "seq", &seq)) {
        ignore_invalid_request(c, req, "missing 'seq' field");
        return;
    }

    /* get resource set id */
    if (!mrp_json_get_integer(req, "id", &rsid)) {
        error_reply(c, type, seq, EINVAL, "missing id");
        return;
    }

    rset = mrp_resource_client_find_set(c->rsc, rsid);

    if (rset != NULL) {
        reply = alloc_reply(type, seq);

        if (reply != NULL) {
            if (mrp_json_add_integer(reply, "status", 0))
                send_message(c, reply);
        }

        mrp_resource_set_release(rset, (uint32_t)seq);
    }
    else
        error_reply(c, type, seq, ENOENT, "resource set %d not found", rsid);
}


static void did_release_set(wrt_client_t *c, mrp_json_t *req)
{
    const char         *type = RESWRT_DID_RELEASE_SET;
    int                 seq;
    mrp_json_t         *reply;
    mrp_resource_set_t *rset;
    uint32_t            rsid;

    if (!mrp_json_get_integer(req, "seq", &seq)) {
        ignore_invalid_request(c, req, "missing 'seq' field");
        return;
    }

    /* get resource set id */
    if (!mrp_json_get_integer(req, "id", &rsid)) {
        error_reply(c, type, seq, EINVAL, "missing id");
        return;
    }

    rset = mrp_resource_client_find_set(c->rsc, rsid);

    if (rset != NULL) {
        reply = alloc_reply(type, seq);

        if (reply != NULL) {
            if (mrp_json_add_integer(reply, "status", 0))
                send_message(c, reply);
        }

        mrp_resource_set_did_release(rset, (uint32_t)seq);
    }
    else
        error_reply(c, type, seq, ENOENT, "resource set %d not found", rsid);
}


static wrt_client_t *create_client(wrt_data_t *data, mrp_transport_t *lt)
{
    wrt_client_t *c;
    char          name[64];

    c = mrp_allocz(sizeof(*c));

    if (c != NULL) {
        mrp_list_init(&c->hook);

        c->t = mrp_transport_accept(lt, c, MRP_TRANSPORT_REUSEADDR);

        if (c->t != NULL) {
            snprintf(name, sizeof(name), "wrt-client%d", data->id++);
            c->rsc = mrp_resource_client_create(name, c);

            if (c->rsc != NULL) {
                mrp_list_append(&data->clients, &c->hook);

                return c;
            }

            mrp_transport_destroy(c->t);
        }

        mrp_free(c);
    }

    return NULL;
}


static void destroy_client(wrt_client_t *c)
{
    if (c != NULL) {
        mrp_list_delete(&c->hook);

        mrp_transport_disconnect(c->t);
        mrp_transport_destroy(c->t);
        mrp_resource_client_destroy(c->rsc);

        mrp_free(c);
    }
}


static void connection_evt(mrp_transport_t *lt, void *user_data)
{
    wrt_data_t   *data = (wrt_data_t *)user_data;
    wrt_client_t *c;

    c = create_client(data, lt);

    if (c != NULL)
        mrp_log_info("Accepted WRT resource client connection.");
    else
        mrp_log_error("Failed to accept WRT resource client connection.");
}


static void closed_evt(mrp_transport_t *t, int error, void *user_data)
{
    wrt_client_t *c = (wrt_client_t *)user_data;

    MRP_UNUSED(t);

    if (error != 0)
        mrp_log_error("WRT resource connection closed with error %d (%s).",
                      error, strerror(error));
    else
        mrp_log_info("WRT resource connection closed.");

    destroy_client(c);
}


static int send_message(wrt_client_t *c, mrp_json_t *msg)
{
    const char *s;

    s = mrp_json_object_to_string(msg);

    mrp_log_info("sending WRT resource message:");
    mrp_log_info("  %s", s);

    return mrp_transport_sendcustom(c->t, msg);
}


static void recv_evt(mrp_transport_t *t, void *data, void *user_data)
{
    wrt_client_t *c   = (wrt_client_t *)user_data;
    mrp_json_t   *req = (mrp_json_t *)data;
    const char   *type;
    int           seq;
    const char   *s;

    MRP_UNUSED(t);

    s = mrp_json_object_to_string((mrp_json_t *)data);

    mrp_log_info("recived WRT resource message:");
    mrp_log_info("  %s", s);

    if (!mrp_json_get_string (req, "type", &type) ||
        !mrp_json_get_integer(req, "seq" , &seq))
        ignore_invalid_request(c, req);
    else {
        if (seq < c->seq) {
            mrp_log_info("ignoring out-of-date request");
            return;
        }
        else
            c->seq = seq;

        if (!strcmp(type, RESWRT_QUERY_RESOURCES))
            query_resources(c, req);
        else if (!strcmp(type, RESWRT_QUERY_CLASSES))
            query_classes(c, req);
        else if (!strcmp(type, RESWRT_QUERY_ZONES))
            query_zones(c, req);
        else if (!strcmp(type, RESWRT_CREATE_SET))
            create_set(c, req);
        else if (!strcmp(type, RESWRT_DESTROY_SET))
            destroy_set(c, req);
        else if (!strcmp(type, RESWRT_ACQUIRE_SET))
            acquire_set(c, req);
        else if (!strcmp(type, RESWRT_RELEASE_SET))
            release_set(c, req);
        else if (!strcmp(type, RESWRT_DID_RELEASE_SET))
            did_release_set(c, req);
        else
            ignore_unknown_request(c, req, type);
    }
}


static int transport_create(wrt_data_t *data)
{
    static mrp_transport_evt_t evt = {
        { .recvcustom     = recv_evt },
        { .recvcustomfrom = NULL     },
        .connection       = connection_evt,
        .closed           = closed_evt,
    };

    mrp_mainloop_t *ml   = data->ctx->ml;
    const char     *root = data->httpdir;
    const char     *cert = data->sslcert;
    const char     *pkey = data->sslpkey;
    const char     *ca   = data->sslca;

    mrp_sockaddr_t  addr;
    socklen_t       len;
    const char     *type, *opt, *val;
    int             flags;

    len = mrp_transport_resolve(NULL, data->addr, &addr, sizeof(addr), &type);

    if (len > 0) {
        flags    = MRP_TRANSPORT_REUSEADDR | MRP_TRANSPORT_MODE_CUSTOM;
        data->lt = mrp_transport_create(ml, type, &evt, data, flags);

        if (data->lt != NULL) {
            if (cert || pkey || ca) {
                mrp_transport_setopt(data->lt, MRP_WSCK_OPT_SSL_CERT, cert);
                mrp_transport_setopt(data->lt, MRP_WSCK_OPT_SSL_PKEY, pkey);
                mrp_transport_setopt(data->lt, MRP_WSCK_OPT_SSL_CA  , ca);
            }

            if (mrp_transport_bind(data->lt, &addr, len) &&
                mrp_transport_listen(data->lt, 0)) {
                mrp_log_info("Listening on transport '%s'...", data->addr);

                opt = MRP_WSCK_OPT_SENDMODE;
                val = MRP_WSCK_SENDMODE_TEXT;
                mrp_transport_setopt(data->lt, opt, val);
                mrp_transport_setopt(data->lt, MRP_WSCK_OPT_HTTPDIR, root);

                return TRUE;
            }

            mrp_transport_destroy(data->lt);
            data->lt = NULL;
        }
    }
    else
        mrp_log_error("Failed to resolve transport address '%s'.", data->addr);

    return FALSE;
}


static void transport_destroy(wrt_data_t *data)
{
    mrp_transport_destroy(data->lt);
}


static int plugin_init(mrp_plugin_t *plugin)
{
    wrt_data_t *data;

    data = mrp_allocz(sizeof(*data));

    if (data != NULL) {
        mrp_list_init(&data->clients);

        data->id      = 1;
        data->ctx     = plugin->ctx;
        data->addr    = plugin->args[ARG_ADDRESS].str;
        data->httpdir = plugin->args[ARG_HTTPDIR].str;
        data->sslcert = plugin->args[ARG_SSLCERT].str;
        data->sslpkey = plugin->args[ARG_SSLPKEY].str;
        data->sslca   = plugin->args[ARG_SSLCA].str;

        if (!transport_create(data))
            goto fail;

        return TRUE;
    }


 fail:
    if (data != NULL) {
        transport_destroy(data);

        mrp_free(data);
    }

    return FALSE;
}


static void plugin_exit(mrp_plugin_t *plugin)
{
    wrt_data_t *data = (wrt_data_t *)plugin->data;

    transport_destroy(data);

    mrp_free(data);
}


#define PLUGIN_DESCRIPTION "Murphy resource Web runtime bridge plugin."
#define PLUGIN_HELP        "Murphy resource protocol for web-runtimes."
#define PLUGIN_AUTHORS     "Krisztian Litkey <kli@iki.fi>"
#define PLUGIN_VERSION     MRP_VERSION_INT(0, 0, 1)

static mrp_plugin_arg_t plugin_args[] = {
    MRP_PLUGIN_ARGIDX(ARG_ADDRESS, STRING, "address", DEFAULT_ADDRESS),
    MRP_PLUGIN_ARGIDX(ARG_HTTPDIR, STRING, "httpdir", DEFAULT_HTTPDIR),
    MRP_PLUGIN_ARGIDX(ARG_SSLCERT, STRING, "sslcert", NULL),
    MRP_PLUGIN_ARGIDX(ARG_SSLPKEY, STRING, "sslpkey", NULL),
    MRP_PLUGIN_ARGIDX(ARG_SSLCA  , STRING, "sslca"  , NULL)

};

MURPHY_REGISTER_PLUGIN("resource-wrt",
                       PLUGIN_VERSION, PLUGIN_DESCRIPTION, PLUGIN_AUTHORS,
                       PLUGIN_HELP, MRP_SINGLETON, plugin_init, plugin_exit,
                       plugin_args, MRP_ARRAY_SIZE(plugin_args),
                       NULL, 0,
                       NULL, 0,
                       NULL);
