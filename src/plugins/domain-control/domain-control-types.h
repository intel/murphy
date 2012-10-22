#ifndef __MURPHY_DOMAIN_CONTROL_TYPES_H__
#define __MURPHY_DOMAIN_CONTROL_TYPES_H__

#include <murphy/common/list.h>
#include <murphy/common/mainloop.h>
#include <murphy/common/transport.h>
#include <murphy/common/hashtbl.h>
#include <murphy/core/context.h>

#include "client.h"

typedef struct pep_proxy_s pep_proxy_t;
typedef struct pep_table_s pep_table_t;
typedef struct pep_watch_s pep_watch_t;
typedef struct pdp_s       pdp_t;


/*
 * a domain controller (on the client side)
 */

struct mrp_domctl_s {
    char                    *name;       /* enforcment point name */
    mrp_mainloop_t          *ml;         /* main loop */
    mrp_transport_t         *t;          /* transport towards murphy */
    int                      connected;  /* transport is up */
    mrp_domctl_table_t      *tables;     /* owned tables */
    int                      ntable;     /* number of owned tables */
    mrp_domctl_watch_t      *watches;    /* watched tables */
    int                      nwatch;     /* number of watched tables */
    mrp_domctl_connect_cb_t  connect_cb; /* connection state change callback */
    mrp_domctl_watch_cb_t    watch_cb;   /* watched table change callback */
    void                    *user_data;  /* opqaue user data for callbacks */
    int                      busy;       /* non-zero if a callback is active */
    int                      destroyed:1;/* non-zero if destroy pending */
    uint32_t                 seqno;      /* request sequence number */
    mrp_list_hook_t          pending;    /* queue of outstanding requests */
};


/*
 * a table associated with or tracked by an enforcement point
 */

struct pep_table_s {
    char               *name;            /* table name */
    char               *mql_columns;     /* column definition clause */
    char               *mql_index;       /* index column list */
    mrp_list_hook_t     hook;            /* to list of tables */
    mqi_handle_t        h;               /* table handle */
    mqi_column_def_t   *columns;         /* column definitions */
    mqi_column_desc_t  *coldesc;         /* column descriptors */
    int                 ncolumn;         /* number of columns */
    int                 idx_col;         /* column index of index column */
    mrp_list_hook_t     watches;         /* watches for this table */
    int                 notify_all : 1;  /* notify all watches */
};


/*
 * a table watch
 */

struct pep_watch_s {
    pep_table_t     *table;              /* table being watched */
    char            *mql_columns;        /* column list to select */
    char            *mql_where;          /* where clause for select */
    int              max_rows;           /* max number of rows to select */
    pep_proxy_t     *proxy;              /* enforcement point */
    int              id;                 /* table id within proxy */
    uint32_t         stamp;              /* last notified update stamp */
    mrp_list_hook_t  tbl_hook;           /* hook to table watch list */
    mrp_list_hook_t  pep_hook;           /* hook to proxy watch list */
};


/*
 * a policy enforcement point (on the server side)
 */

struct pep_proxy_s {
    char              *name;             /* enforcement point name */
    pdp_t             *pdp;              /* domain controller context */
    mrp_transport_t   *t;                /* associated transport */
    mrp_list_hook_t    hook;             /* to list of all enforcement points */
    pep_table_t       *tables;           /* tables owned by this */
    int                ntable;           /* number of tables */
    mrp_list_hook_t    watches;          /* tables watched by this */
    int                notify_update;    /* whether needs notification */
    mrp_msg_t         *notify_msg;       /* notification being built */
    int                notify_ntable;    /* number of changed tables */
    int                notify_ncolumn;   /* total columns in notification */
    int                notify_fail : 1;  /* notification failure */
    int                notify_all : 1;   /* notify all watches */
};


/*
 * policy domain controller context
 */

struct pdp_s {
    mrp_context_t   *ctx;                /* murphy context */
    const char      *address;            /* external transport address */
    mrp_transport_t *ext;                /* external transport */
    mrp_list_hook_t  proxies;            /* list of enforcement points */
    mrp_list_hook_t  tables;             /* list of tables we track */
    mrp_htbl_t      *watched;            /* tracked tables by name */
    mrp_deferred_t  *notify;             /* deferred notification */
    int              notify_scheduled;   /* is notification scheduled? */
};



#endif /* __MURPHY_DOMAIN_CONTROL_TYPES_H__ */
