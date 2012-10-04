#include <murphy/common/mm.h>
#include <murphy/common/log.h>

#include "decision-types.h"
#include "message.h"
#include "notify.h"


static void prepare_proxy_notification(pep_proxy_t *proxy)
{
    proxy->notify_ntable  = 0;
    proxy->notify_ncolumn = 0;
    proxy->notify_fail    = FALSE;
}


static int prepare_table_notification(pep_table_t *t)
{
    mrp_pep_value_t *data;
    int              nrow, size, n;

    nrow = mqi_get_table_size(t->h);

    mrp_debug("size of table %s: %d rows", t->name, nrow);

    if (nrow <= 0) {
        t->notify_fail = FALSE;

        return TRUE;
    }

    t->notify_stamp = mqi_get_table_stamp(t->h);

    size = t->ncolumn * sizeof(*data);
    data = mrp_allocz(nrow * size);

    if (data != NULL) {
        n = mqi_select(t->h, NULL, t->coldesc, data, size, nrow);

        mrp_debug("select from %s: %d rows", t->name, n);

        if (n <= nrow) {
            t->notify_data = data;
            t->notify_nrow = n;
            t->notify_fail = FALSE;

            return TRUE;
        }

        mrp_free(data);
    }

    t->notify_fail = TRUE;

    return FALSE;
}


static void free_table_notification(pep_table_t *t)
{
    mrp_free(t->notify_data);

    t->notify_data = NULL;
    t->notify_nrow = 0;
    t->notify_all  = FALSE;
}


static int collect_watch_notification(pep_watch_t *w)
{
    pep_proxy_t *proxy = w->proxy;
    pep_table_t *t     = w->table;

    if (!proxy->notify_fail && !t->notify_fail) {
        mrp_debug("updating %s watch for %s", t->name, proxy->name);

        if (proxy->notify_all || t->notify_all || t->notify_stamp != w->stamp) {
            if (proxy->notify_msg == NULL)
                proxy->notify_msg = create_notify_message();

            if (proxy->notify_msg != NULL) {
                if (update_notify_message(proxy->notify_msg, w->id,
                                          t->columns, t->ncolumn,
                                          t->notify_data, t->notify_nrow)) {
                    proxy->notify_ntable++;
                    proxy->notify_ncolumn += (t->notify_nrow * t->ncolumn);
                success:
                    w->stamp = t->notify_stamp;

                    return TRUE;
                }
            }
        }
        else
            goto success;
    }

    proxy->notify_fail = TRUE;

    return FALSE;
}


static int send_proxy_notification(pep_proxy_t *proxy)
{
    uint16_t nchange, ntotal;

    if (proxy->notify_msg == NULL)
        return TRUE;

    if (!proxy->notify_fail) {
        mrp_debug("notifying client %s", proxy->name);

        nchange = proxy->notify_ntable;
        ntotal  = proxy->notify_ncolumn;

        mrp_msg_set(proxy->notify_msg, MRP_PEPMSG_UINT16(NCHANGE, nchange));
        mrp_msg_set(proxy->notify_msg, MRP_PEPMSG_UINT16(NTOTAL , ntotal ));


        /*
          mrp_log_info("Notification message for client %s:", proxy->name);
          mrp_msg_dump(proxy->notify_msg, stdout);
        */


        mrp_transport_send(proxy->t, proxy->notify_msg);
    }
    else
        mrp_log_error("Failed to generate/send notification to %s.",
                      proxy->name);

    mrp_msg_unref(proxy->notify_msg);

    proxy->notify_msg     = NULL;
    proxy->notify_ntable  = 0;
    proxy->notify_ncolumn = 0;
    proxy->notify_fail    = FALSE;
    proxy->notify_all     = FALSE;

    return TRUE;
}


void notify_table_changes(pdp_t *pdp)
{
    mrp_list_hook_t *p, *n, *wp, *wn;
    pep_proxy_t     *proxy;
    pep_table_t     *t;
    pep_watch_t     *w;

    mrp_debug("notifying clients about table changes");

    mrp_list_foreach(&pdp->proxies, p, n) {
        proxy = mrp_list_entry(p, typeof(*proxy), hook);
        prepare_proxy_notification(proxy);
    }

    mrp_list_foreach(&pdp->tables, p, n) {
        t = mrp_list_entry(p, typeof(*t), hook);
        prepare_table_notification(t);

        mrp_list_foreach(&t->watches, wp, wn) {
            w = mrp_list_entry(wp, typeof(*w), tbl_hook);
            collect_watch_notification(w);
        }

        free_table_notification(t);
    }

    mrp_list_foreach(&pdp->proxies, p, n) {
        proxy = mrp_list_entry(p, typeof(*proxy), hook);
        send_proxy_notification(proxy);
    }
}
