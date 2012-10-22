#include <errno.h>

#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>

#include "domain-control-types.h"
#include "table.h"
#include "proxy.h"


int init_proxies(pdp_t *pdp)
{
    mrp_list_init(&pdp->proxies);

    return TRUE;
}


void destroy_proxies(pdp_t *pdp)
{
    MRP_UNUSED(pdp);

    return;
}


pep_proxy_t *create_proxy(pdp_t *pdp)
{
    pep_proxy_t *proxy;

    proxy = mrp_allocz(sizeof(*proxy));

    if (proxy != NULL) {
        mrp_list_init(&proxy->hook);
        mrp_list_init(&proxy->watches);

        proxy->pdp = pdp;

        mrp_list_append(&pdp->proxies, &proxy->hook);
    }

    return proxy;
}


void destroy_proxy(pep_proxy_t *proxy)
{
    int i;

    if (proxy != NULL) {
        mrp_list_delete(&proxy->hook);

        for (i = 0; i < proxy->ntable; i++)
            destroy_proxy_table(proxy->tables + i);

        destroy_proxy_watches(proxy);

        mrp_free(proxy);
    }
}


int register_proxy(pep_proxy_t *proxy, char *name,
                   mrp_domctl_table_t *tables, int ntable,
                   mrp_domctl_watch_t *watches, int nwatch,
                   int *error, const char **errmsg)
{
    pep_table_t        *t;
    mrp_domctl_watch_t *w;
    int                 i;

    proxy->name   = mrp_strdup(name);
    proxy->tables = mrp_allocz_array(typeof(*proxy->tables) , ntable);
    proxy->ntable = ntable;

    if (proxy->name == NULL || (ntable && proxy->tables == NULL)) {
        *error  = ENOMEM;
        *errmsg = "failed to allocate proxy table";

        return FALSE;
    }

    for (i = 0, t = proxy->tables; i < ntable; i++, t++) {
        t->h           = MQI_HANDLE_INVALID;
        t->name        = mrp_strdup(tables[i].table);
        t->mql_columns = mrp_strdup(tables[i].mql_columns);
        t->mql_index   = mrp_strdup(tables[i].mql_index);

        if (t->name == NULL || t->mql_columns == NULL || t->mql_index == NULL) {
            mrp_log_error("Failed to allocate proxy table %s for %s.",
                          tables[i].table, name);
            *error  = ENOMEM;
            *errmsg = "failed to allocate proxy table";

            return FALSE;
        }

        if (create_proxy_table(t, error, errmsg))
            mrp_log_info("Client %s created table %s.", proxy->name,
                         tables[i].table);
        else {
            mrp_log_error("Client %s failed to create table %s (%d: %s).",
                          proxy->name, tables[i].table, *error, *errmsg);
            return FALSE;
        }
    }

    for (i = 0, w = watches; i < nwatch; i++, w++) {
        if (create_proxy_watch(proxy, i, w->table, w->mql_columns,
                               w->mql_where, w->max_rows, error, errmsg))
            mrp_log_info("Client %s subscribed for table %s.", proxy->name,
                         w->table);
        else
            mrp_log_error("Client %s failed to subscribe for table %s.",
                          proxy->name, w->table);
    }

    return TRUE;
}


int unregister_proxy(pep_proxy_t *proxy)
{
    destroy_proxy(proxy);

    return TRUE;
}
