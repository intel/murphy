#include <murphy/common/log.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>

#include "decision-types.h"
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
                   mrp_pep_table_t *tables, int ntable,
                   mrp_pep_table_t *watches, int nwatch,
                   int *error, const char **errmsg)
{
    int i;

    proxy->name   = mrp_strdup(name);
    proxy->tables = mrp_allocz_array(typeof(*proxy->tables) , ntable);
    proxy->ntable = ntable;

    if (proxy->name == NULL || (ntable && proxy->tables == NULL))
        return FALSE;

    for (i = 0; i < ntable; i++) {
        if (create_proxy_table(proxy->tables + i, tables + i, error, errmsg))
            mrp_log_info("Client %s created table %s.", proxy->name,
                         tables[i].name);
        else {
            mrp_log_error("Client %s failed to create table %s (%d: %s).",
                          proxy->name, tables[i].name, *error, *errmsg);
            return FALSE;
        }
    }

    for (i = 0; i < nwatch; i++) {
        if (create_proxy_watch(proxy, i, watches + i, error, errmsg))
            mrp_log_info("Client %s subscribed for table %s.", proxy->name,
                         watches[i].name);
        else
            mrp_log_error("Client %s failed to subscribe for table %s.",
                          proxy->name, watches[i].name);
    }

    return TRUE;
}


int unregister_proxy(pep_proxy_t *proxy)
{
    destroy_proxy(proxy);

    return TRUE;
}
