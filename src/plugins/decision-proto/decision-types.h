#ifndef __MURPHY_DECISION_TYPES_H__
#define __MURPHY_DECISION_TYPES_H__

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
 * a policy enforcement point (on the client side)
 */

struct mrp_pep_s {
    char                 *name;          /* enforcment point name */
    mrp_mainloop_t       *ml;            /* main loop */
    mrp_transport_t      *t;             /* transport towards murphy */
    int                   connected;     /* transport is up */
    mrp_pep_table_t      *owned;         /* owned tables */
    int                   nowned;        /* number of owned tables */
    mrp_pep_table_t      *watched;       /* watched tables */
    int                   nwatched;      /* number of watched tables */
    mrp_pep_connect_cb_t  connect_cb;    /* connection state change callback */
    mrp_pep_data_cb_t     data_cb;       /* watched data change callback */
    void                 *user_data;     /* opqaue user data for callbacks */
    int                   busy;          /* non-zero if a callback is active */
    int                   destroyed : 1; /* non-zero if destroy pending */
    uint32_t              seqno;         /* request sequence number */
    mrp_list_hook_t       pending;       /* queue of outstanding requests */
};


/*
 * a table associated with or tracked by an enforcement point
 */

struct pep_table_s {
    char              *name;             /* table name */
    mrp_list_hook_t    hook;             /* to list of tables */
    mqi_handle_t       h;                /* MDB table handle */
    mqi_column_def_t  *columns;          /* column definitions */
    mqi_column_desc_t *coldesc;          /* column descriptors */
    int                ncolumn;          /* number of columns */
    int                idx_col;          /* column index of index column */
    mrp_list_hook_t    watches;          /* watches for this table */
    uint32_t           notify_stamp;     /* current table stamp */
    mrp_pep_value_t   *notify_data;      /* notification data */
    int                notify_nrow;      /* number of rows to notify */
    int                notify_fail : 1;  /* notification failure */
    int                notify_all : 1;   /* notify all watches */
};


/*
 * a table watch
 */

struct pep_watch_s {
    pep_table_t     *table;              /* table being watched */
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
    pdp_t             *pdp;              /* decision point context */
    mrp_transport_t   *t;                /* associated transport */
    mrp_list_hook_t    hook;             /* to list of all enforcement points */
    pep_table_t       *tables;           /* tables owned by this */
    int                ntable;           /* number of tables */
    mrp_list_hook_t    watches;          /* tables watched by this */
    mrp_msg_t         *notify_msg;       /* notification being built */
    int                notify_ntable;    /* number of changed tables */
    int                notify_ncolumn;   /* total columns in notification */
    int                notify_fail : 1;  /* notification failure */
    int                notify_all : 1;   /* notify all watches */
};


/*
 * policy decision point context
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






#if 0

/*
 * common table data for tracking and proxying
 */

typedef struct table_s         table_t;
typedef struct tracked_table_s tracked_table_t;
typedef struct proxied_table_s proxied_table_t;

struct table_s {
    char              *name;             /* table name */
    mqi_handle_t       h;                /* table handle */
    mqi_column_def_t  *columns;          /* column definitions */
    mqi_column_desc_t *coldesc;          /* column descriptors */
    int                ncolumn;          /* number of columns */
};


/*
 * a tracked table
 */

struct tracked_table_s {
    table_t         *t;                  /* actual table data */
    mrp_list_hook_t  watches;            /* watches for this table */
    mrp_pep_value_t *notify_data;        /* collected data for notification */
    int              notify_nrow;        /* number of rows in notification */
    int              notify_failed:1;    /* notification failure */
    int              notify_all:1;       /* notify all watches */
};


/*
 * a proxied table
 */

struct proxied_table_s {
    table_t *t;                          /* actual table data */
    int      id;                         /* id for enforcement point */
    int      idx_col;                    /* column index of index column */
}


#endif

#endif /* __MURPHY_DECISION_TYPES_H__ */
