#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy-db/mqi.h>

#include "resolver-types.h"
#include "resolver.h"
#include "target.h"
#include "fact.h"

static int subscribe_db_events(mrp_resolver_t *r);
static void unsubscribe_db_events(mrp_resolver_t *r);

int create_fact(mrp_resolver_t *r, char *fact)
{
    int     i;
    fact_t *f;

    subscribe_db_events(r);

    for (i = 0; i < r->nfact; i++) {
        if (!strcmp(r->facts[i].name, fact))
            return TRUE;
    }

    if (!mrp_reallocz(r->facts, r->nfact * sizeof(*r->facts),
                      (r->nfact + 1) * sizeof(*r->facts)))
        return FALSE;

    f = r->facts + r->nfact++;
    f->name  = mrp_strdup(fact);
    f->table = mqi_get_table_handle(f->name + 1);

    if (f->name != NULL)
        return TRUE;
    else
        return FALSE;
}


void destroy_facts(mrp_resolver_t *r)
{
    fact_t *f;
    int     i;

    unsubscribe_db_events(r);

    for (i = 0, f = r->facts; i < r->nfact; i++, f++)
        mrp_free(f->name);

    mrp_free(r->facts);
}


uint32_t fact_stamp(mrp_resolver_t *r, int id)
{
    fact_t   *fact = r->facts + id;
    uint32_t  stamp;

    if (fact->table != MQI_HANDLE_INVALID)
        stamp = mqi_get_table_stamp(fact->table);
    else
        stamp = 0; /* MQI_NO_STAMP */

    return stamp;
}


fact_t *lookup_fact(mrp_resolver_t *r, const char *name)
{
    fact_t *f;
    int     i;

    for (i = 0, f = r->facts; i < r->nfact; i++, f++)
        if (!strcmp(f->name, name))
            return f;

    return NULL;
}


static void update_fact_table(mrp_resolver_t *r, const char *name,
                              mqi_handle_t tbl)
{
    fact_t *f;
    int     i;

    for (i = 0, f = r->facts; i < r->nfact; i++, f++) {
        if (!strcmp(f->name + 1, name)) {
            f->table = tbl;
            return;
        }
    }
}


static void check_fact_tables(mrp_resolver_t *r)
{
    fact_t *f;
    int     i;

    for (i = 0, f = r->facts; i < r->nfact; i++, f++) {
        if (f->table != MQI_HANDLE_INVALID)
            mrp_debug("Fact table '%s' stamp: %u.",
                      f->name, mqi_get_table_stamp(f->table));
    }
}


static inline int open_db(void)
{
    static int opened = FALSE;
    if (!opened)
        opened = (mqi_open() == 0);

    return opened;
}


static void table_event(mqi_event_t *e, void *user_data)
{
    mrp_resolver_t *r = (mrp_resolver_t *)user_data;

    switch (e->event) {
    case mqi_table_created:
        mrp_debug("DB table created (%s, %u).",
                  e->table.table.name, e->table.table.handle);
        update_fact_table(r, e->table.table.name, e->table.table.handle);
        break;
    case mqi_table_dropped:
        mrp_debug("DB table dropped (%s, %u).",
                  e->table.table.name, e->table.table.handle);
        update_fact_table(r, e->table.table.name, MQI_HANDLE_INVALID);
        break;
    default:
        break;
    }
}


static void transaction_event(mqi_event_t *e, void *user_data)
{
    mrp_resolver_t *r = (mrp_resolver_t *)user_data;

    MRP_UNUSED(r);

    switch (e->event) {
    case mqi_transaction_end:
        mrp_debug("DB transaction ended.");
        check_fact_tables(r);
        schedule_target_autoupdate(r);
        break;
    case mqi_transaction_start:
        mrp_debug("DB transaction started.");
        break;
    default:
        break;
    }
}


static int subscribe_db_events(mrp_resolver_t *r)
{
    if (open_db()) {
        if (mqi_create_table_trigger(table_event, r) == 0) {
            if (mqi_create_transaction_trigger(transaction_event, r) == 0)
                return TRUE;
            else
                mqi_drop_table_trigger(table_event, r);
        }
    }

    return FALSE;
}


static void unsubscribe_db_events(mrp_resolver_t *r)
{
    mqi_drop_table_trigger(table_event, r);
    mqi_drop_transaction_trigger(transaction_event, r);
}


mqi_handle_t start_transaction(mrp_resolver_t *r)
{
    MRP_UNUSED(r);

    return mqi_begin_transaction();
}


int commit_transaction(mrp_resolver_t *r, mqi_handle_t tx)
{
    MRP_UNUSED(r);

    return mqi_commit_transaction(tx) != -1;
}


int rollback_transaction(mrp_resolver_t *r, mqi_handle_t tx)
{
    MRP_UNUSED(r);

    return mqi_rollback_transaction(tx) != -1;
}
