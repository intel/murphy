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

#include <murphy/common/mm.h>
#include <murphy/common/list.h>
#include <murphy/common/log.h>
#include <murphy/common/native-types.h>
#include <murphy/common/transport.h>

static int check_destroy(mrp_transport_t *t);
static int recv_data(mrp_transport_t *t, void *data, size_t size,
                     mrp_sockaddr_t *addr, socklen_t addrlen);
static inline int purge_destroyed(mrp_transport_t *t);


static MRP_LIST_HOOK(transports);
static mrp_sighandler_t *pipe_handler;


static int check_request_callbacks(mrp_transport_req_t *req)
{
    /* XXX TODO: hmm... this probably needs more thought/work */

    if (!req->open || !req->close)
        return FALSE;

    if (req->accept) {
        if (!req->sendmsg || !req->sendraw || !req->senddata)
            return FALSE;
    }
    else {
        if (!req->sendmsgto || !req->sendrawto || !req->senddatato)
            return FALSE;
    }

    if (( req->connect && !req->disconnect) ||
        (!req->connect &&  req->disconnect))
        return FALSE;

    return TRUE;
}


int mrp_transport_register(mrp_transport_descr_t *d)
{
    if (!check_request_callbacks(&d->req))
        return FALSE;

    if (d->size >= sizeof(mrp_transport_t)) {
        mrp_list_init(&d->hook);
        mrp_list_append(&transports, &d->hook);

        return TRUE;
    }
    else
        return FALSE;
}


void mrp_transport_unregister(mrp_transport_descr_t *d)
{
    mrp_list_delete(&d->hook);
}


static mrp_transport_descr_t *find_transport(const char *type)
{
    mrp_transport_descr_t *d;
    mrp_list_hook_t       *p, *n;

    mrp_list_foreach(&transports, p, n) {
        d = mrp_list_entry(p, typeof(*d), hook);
        if (!strcmp(d->type, type))
            return d;
    }

    return NULL;
}


static int check_event_callbacks(mrp_transport_evt_t *evt)
{
    /*
     * For connection-oriented transports we require a recv* callback
     * and a closed callback.
     *
     * For connectionless transports we only require a recvfrom* callback.
     * A recv* callback is optional, however the transport cannot be put
     * to connected mode (usually for doing sender-based filtering) if
     * recv* is omitted.
     */

    if (evt->connection != NULL) {
        if (evt->recvmsg == NULL || evt->closed == NULL)
            return FALSE;
    }
    else {
        if (evt->recvmsgfrom == NULL)
            return FALSE;
    }

    return TRUE;
}


static void sigpipe_handler(mrp_sighandler_t *h, int sig, void *user_data)
{
    MRP_UNUSED(h);
    MRP_UNUSED(user_data);

    mrp_debug("caught signal %d (%s)...", sig, strsignal(sig));
}


mrp_transport_t *mrp_transport_create(mrp_mainloop_t *ml, const char *type,
                                      mrp_transport_evt_t *evt, void *user_data,
                                      int flags)
{
    mrp_transport_descr_t *d;
    mrp_transport_t       *t;

    if (!pipe_handler)
        pipe_handler = mrp_add_sighandler(ml, SIGPIPE, sigpipe_handler, NULL);

    if (!check_event_callbacks(evt)) {
        errno = EINVAL;
        return NULL;
    }

    if ((d = find_transport(type)) != NULL) {
        if ((t = mrp_allocz(d->size)) != NULL) {
            t->descr     = d;
            t->ml        = ml;
            t->evt       = *evt;
            t->user_data = user_data;

            t->check_destroy = check_destroy;
            t->recv_data     = recv_data;
            t->flags         = flags & ~MRP_TRANSPORT_MODE_MASK;
            t->mode          = flags &  MRP_TRANSPORT_MODE_MASK;

            if (!t->descr->req.open(t)) {
                mrp_free(t);
                t = NULL;
            }
        }
    }
    else
        t = NULL;

    return t;
}


mrp_transport_t *mrp_transport_create_from(mrp_mainloop_t *ml, const char *type,
                                           void *conn, mrp_transport_evt_t *evt,
                                           void *user_data, int flags,
                                           int state)
{
    mrp_transport_descr_t *d;
    mrp_transport_t       *t;

    if (!pipe_handler)
        pipe_handler = mrp_add_sighandler(ml, SIGPIPE, sigpipe_handler, NULL);

    if (!check_event_callbacks(evt)) {
        errno = EINVAL;
        return NULL;
    }

    if ((d = find_transport(type)) != NULL) {
        if ((t = mrp_allocz(d->size)) != NULL) {
            t->descr     = d;
            t->ml        = ml;
            t->evt       = *evt;
            t->user_data = user_data;

            t->check_destroy = check_destroy;
            t->recv_data     = recv_data;
            t->flags         = flags & ~MRP_TRANSPORT_MODE_MASK;
            t->mode          = flags &  MRP_TRANSPORT_MODE_MASK;

            t->connected = !!(state & MRP_TRANSPORT_CONNECTED);
            t->listened  = !!(state & MRP_TRANSPORT_LISTENED);

            if (t->connected && t->listened) {
                mrp_free(t);
                return NULL;
            }

            if (!t->descr->req.createfrom(t, conn)) {
                mrp_free(t);
                t = NULL;
            }
        }
    }
    else
        t = NULL;

    return t;
}


int mrp_transport_setopt(mrp_transport_t *t, const char *opt, const void *val)
{
    if (t != NULL) {
        if (t->descr->req.setopt != NULL)
            return t->descr->req.setopt(t, opt, val);
        else {
            if (t->mode == MRP_TRANSPORT_MODE_NATIVE) {
                if (!strcmp(opt, MRP_TRANSPORT_OPT_TYPEMAP)) {
                    t->map = (void *)val;
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}


static inline int type_matches(const char *type, const char *addr)
{
    while (*type == *addr)
        type++, addr++;

    return (*type == '\0' && *addr == ':');
}


socklen_t mrp_transport_resolve(mrp_transport_t *t, const char *str,
                                mrp_sockaddr_t *addr, socklen_t size,
                                const char **typep)
{
    mrp_transport_descr_t *d;
    mrp_list_hook_t       *p, *n;
    socklen_t              l;

    if (t != NULL)
        return t->descr->resolve(str, addr, size, typep);
    else {
        mrp_list_foreach(&transports, p, n) {
            d = mrp_list_entry(p, typeof(*d), hook);
            l = d->resolve(str, addr, size, typep);

            if (l > 0)
                return l;
        }
    }

    return 0;
}


int mrp_transport_bind(mrp_transport_t *t, mrp_sockaddr_t *addr,
                       socklen_t addrlen)
{
    if (t != NULL) {
        if (t->descr->req.bind != NULL)
            return t->descr->req.bind(t, addr, addrlen);
        else
            return TRUE;                  /* assume no binding is needed */
    }
    else
        return FALSE;
}


int mrp_transport_listen(mrp_transport_t *t, int backlog)
{
    int result;

    if (t != NULL) {
        if (t->descr->req.listen != NULL) {
            MRP_TRANSPORT_BUSY(t, {
                    result = t->descr->req.listen(t, backlog);
                });

            purge_destroyed(t);

            return result;
        }
    }

    return FALSE;
}


mrp_transport_t *mrp_transport_accept(mrp_transport_t *lt,
                                      void *user_data, int flags)
{
    mrp_transport_t *t;

    if ((t = mrp_allocz(lt->descr->size)) != NULL) {
        bool failed  = FALSE;
        t->descr     = lt->descr;
        t->ml        = lt->ml;
        t->evt       = lt->evt;
        t->user_data = user_data;

        t->check_destroy = check_destroy;
        t->recv_data     = recv_data;
        t->flags         = (lt->flags & MRP_TRANSPORT_INHERIT) | flags;
        t->flags         = t->flags & ~MRP_TRANSPORT_MODE_MASK;
        t->mode          = lt->mode;
        t->map           = lt->map;

        MRP_TRANSPORT_BUSY(t, {
                if (!t->descr->req.accept(t, lt)) {
                    failed = TRUE;
                }
                else {
                    t->connected = TRUE;
                }
            });

        if (failed) {
            mrp_free(t);
            t = NULL;
        }
    }

    return t;
}


static inline int purge_destroyed(mrp_transport_t *t)
{
    if (t->destroyed && !t->busy) {
        mrp_debug("destroying transport %p...", t);
        mrp_free(t);
        return TRUE;
    }
    else
        return FALSE;
}


void mrp_transport_destroy(mrp_transport_t *t)
{
    if (t != NULL) {
        t->destroyed = TRUE;

        MRP_TRANSPORT_BUSY(t, {
                t->descr->req.disconnect(t);
                t->descr->req.close(t);
            });

        purge_destroyed(t);
    }
}


static int check_destroy(mrp_transport_t *t)
{
    return purge_destroyed(t);
}


int mrp_transport_connect(mrp_transport_t *t, mrp_sockaddr_t *addr,
                          socklen_t addrlen)
{
    int result;

    if (!t->connected) {

        /* make sure we can deliver reception noifications */
        if (t->evt.recvmsg == NULL) {
            errno = EINVAL;
            return FALSE;
        }

        MRP_TRANSPORT_BUSY(t, {
                if (t->descr->req.connect(t, addr, addrlen))  {
                    t->connected = TRUE;
                    result       = TRUE;
                }
                else
                    result = FALSE;
            });

        purge_destroyed(t);
    }
    else {
        errno  = EISCONN;
        result = FALSE;
    }

    return result;
}


int mrp_transport_disconnect(mrp_transport_t *t)
{
    int result;

    if (t != NULL && t->connected) {
        MRP_TRANSPORT_BUSY(t, {
                if (t->descr->req.disconnect(t)) {
                    t->connected = FALSE;
                    result       = TRUE;
                }
                else
                    result = TRUE;
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_send(mrp_transport_t *t, mrp_msg_t *msg)
{
    int result;

    if (t->connected && t->descr->req.sendmsg) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.sendmsg(t, msg);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_sendto(mrp_transport_t *t, mrp_msg_t *msg,
                         mrp_sockaddr_t *addr, socklen_t addrlen)
{
    int result;

    if (t->descr->req.sendmsgto) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.sendmsgto(t, msg, addr, addrlen);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_sendraw(mrp_transport_t *t, void *data, size_t size)
{
    int result;

    if (t->connected &&
        t->mode == MRP_TRANSPORT_MODE_RAW && t->descr->req.sendraw) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.sendraw(t, data, size);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_sendrawto(mrp_transport_t *t, void *data, size_t size,
                            mrp_sockaddr_t *addr, socklen_t addrlen)
{
    int result;

    if (t->mode == MRP_TRANSPORT_MODE_RAW && t->descr->req.sendrawto) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.sendrawto(t, data, size, addr, addrlen);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_senddata(mrp_transport_t *t, void *data, uint16_t tag)
{
    int result;

    if (t->connected &&
        t->mode == MRP_TRANSPORT_MODE_DATA && t->descr->req.senddata) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.senddata(t, data, tag);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_senddatato(mrp_transport_t *t, void *data, uint16_t tag,
                             mrp_sockaddr_t *addr, socklen_t addrlen)
{
    int result;

    if (t->mode == MRP_TRANSPORT_MODE_DATA && t->descr->req.senddatato) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.senddatato(t, data, tag, addr, addrlen);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_sendcustom(mrp_transport_t *t, void *data)
{
    int result;

    if (t->mode == MRP_TRANSPORT_MODE_CUSTOM && t->descr->req.sendcustom) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.sendcustom(t, data);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_sendcustomto(mrp_transport_t *t, void *data,
                               mrp_sockaddr_t *addr, socklen_t addrlen)
{
    int result;

    if (t->mode == MRP_TRANSPORT_MODE_CUSTOM && t->descr->req.sendcustomto) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.sendcustomto(t, data, addr, addrlen);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_sendnative(mrp_transport_t *t, void *data, uint32_t type_id)
{
    int result;

    if (t->mode == MRP_TRANSPORT_MODE_NATIVE && t->descr->req.sendnative) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.sendnative(t, data, type_id);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_sendnativeto(mrp_transport_t *t, void *data, uint32_t type_id,
                               mrp_sockaddr_t *addr, socklen_t addrlen)
{
    int result;

    if (t->mode == MRP_TRANSPORT_MODE_NATIVE && t->descr->req.sendnativeto) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.sendnativeto(t, data, type_id,
                                                    addr, addrlen);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_sendjson(mrp_transport_t *t, mrp_json_t *msg)
{
    int result;

    if (t->mode == MRP_TRANSPORT_MODE_JSON && t->descr->req.sendjson) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.sendjson(t, msg);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


int mrp_transport_sendjsonto(mrp_transport_t *t, mrp_json_t *msg,
                             mrp_sockaddr_t *addr, socklen_t addrlen)
{
    int result;

    if (t->mode == MRP_TRANSPORT_MODE_JSON && t->descr->req.sendjsonto) {
        MRP_TRANSPORT_BUSY(t, {
                result = t->descr->req.sendjsonto(t, msg, addr, addrlen);
            });

        purge_destroyed(t);
    }
    else
        result = FALSE;

    return result;
}


static int recv_data(mrp_transport_t *t, void *data, size_t size,
                     mrp_sockaddr_t *addr, socklen_t addrlen)
{
    mrp_data_descr_t *type;
    uint16_t          tag;
    mrp_msg_t        *msg;
    uint32_t          type_id;
    void             *decoded;

    switch (t->mode) {
    case MRP_TRANSPORT_MODE_DATA:
        tag   = be16toh(*(uint16_t *)data);
        data += sizeof(tag);
        size -= sizeof(tag);
        type  = mrp_msg_find_type(tag);

        if (type != NULL) {
            decoded = mrp_data_decode(&data, &size, type);

            if (decoded != NULL && size == 0) {
                if (t->connected && t->evt.recvdata) {
                    MRP_TRANSPORT_BUSY(t, {
                            t->evt.recvdata(t, decoded, tag, t->user_data);
                        });
                }
                else if (t->evt.recvdatafrom) {
                    MRP_TRANSPORT_BUSY(t, {
                            t->evt.recvdatafrom(t, decoded, tag, addr, addrlen,
                                                t->user_data);
                        });
                }
                else
                    mrp_free(decoded);         /* no callback, discard */

                return 0;
            }
            else {
                if (decoded != NULL) {
                    mrp_free(decoded);
                    return -EMSGSIZE;
                }
                else
                    return -errno;
            }
        }
        else
            return -ENOPROTOOPT;
        break;

    case MRP_TRANSPORT_MODE_RAW:
        if (t->connected) {
            MRP_TRANSPORT_BUSY(t, {
                    t->evt.recvraw(t, data, size, t->user_data);
                });
        }
        else {
            MRP_TRANSPORT_BUSY(t, {
                    t->evt.recvrawfrom(t, data, size, addr, addrlen,
                                       t->user_data);
                });
        }
        return 0;

    case MRP_TRANSPORT_MODE_MSG:
        tag   = be16toh(*(uint16_t *)data);
        data += sizeof(tag);
        size -= sizeof(tag);

        if (tag != MRP_MSG_TAG_DEFAULT ||
            (msg = mrp_msg_default_decode(data, size)) == NULL) {
            return -EPROTO;
        }
        else {
            if (t->connected) {
                MRP_TRANSPORT_BUSY(t, {
                        t->evt.recvmsg(t, msg, t->user_data);
                    });
            }
            else {
                MRP_TRANSPORT_BUSY(t, {
                        t->evt.recvmsgfrom(t, msg, addr, addrlen,
                                           t->user_data);
                    });
            }

            mrp_msg_unref(msg);

            return 0;
        }
        break;

    case MRP_TRANSPORT_MODE_CUSTOM:
        if (t->connected) {
            if (t->evt.recvcustom) {
                MRP_TRANSPORT_BUSY(t, {
                        t->evt.recvcustom(t, data, t->user_data);
                    });

                return 0;
            }
        }
        else {
            if (t->evt.recvcustomfrom) {
                MRP_TRANSPORT_BUSY(t, {
                        t->evt.recvcustomfrom(t, data, addr, addrlen,
                                              t->user_data);
                    });

                return 0;
            }
        }
        return -EPROTOTYPE;

    case MRP_TRANSPORT_MODE_NATIVE:
        type_id = 0;
        if (mrp_decode_native(&data, &size, &decoded, &type_id, t->map) < 0)
            return -EPROTO;

        if (decoded == NULL || size != 0) {
            mrp_free_native(decoded, type_id);
            return -EPROTO;
        }

        if (t->connected) {
            MRP_TRANSPORT_BUSY(t, {
                    t->evt.recvnative(t, decoded, type_id, t->user_data);
                });
        }
        else {
            MRP_TRANSPORT_BUSY(t, {
                    t->evt.recvnativefrom(t, decoded, type_id, addr, addrlen,
                                          t->user_data);
                });
        }
        return 0;

    case MRP_TRANSPORT_MODE_JSON:
        if (t->connected) {
            if (t->evt.recvjson) {
                MRP_TRANSPORT_BUSY(t, {
                        t->evt.recvjson(t, data, t->user_data);
                    });
            }
        }
        else {
            if (t->evt.recvjsonfrom) {
                MRP_TRANSPORT_BUSY(t, {
                        t->evt.recvjsonfrom(t, data, addr, addrlen,
                                            t->user_data);
                    });
            }
        }
        return 0;

    default:
        return -EPROTOTYPE;
    }
}

