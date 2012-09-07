#ifndef __MURPHY_RESOLVER_TYPES_H__
#define __MURPHY_RESOLVER_TYPES_H__

#include <stdint.h>

#include <murphy/common/hashtbl.h>
#include <murphy/core/scripting.h>

#include <murphy-db/mqi.h>

typedef struct target_s target_t;        /* opaque type for resolver targets */
typedef struct fact_s   fact_t;          /* opaque type for tracked facts */

/*
 * a resolver target
 */
struct target_s {
    char            *name;               /* target name */
    uint32_t         stamp;              /* touch-stamp */
    char           **depends;            /* dependencies stated in the input */
    int              ndepend;            /* number of dependencies */
    int             *update_facts;       /* facts to check when updating */
    int             *update_targets;     /* targets to check when updating */
    uint32_t        *fact_stamps;        /* stamps of facts at last update */
    mrp_scriptlet_t *script;             /* update script if any, or NULL */
};


/*
 * a tracked fact
 */
struct fact_s {
    char         *name;                  /* fact name */
    mqi_handle_t  table;                 /* associated DB table */
    uint32_t      stamp;                 /* touch-stamp */
};


struct mrp_resolver_s {
    target_t          *targets;          /* targets defined in the ruleset */
    int                ntarget;          /* number of targets */
    fact_t            *facts;            /* facts tracked as dependencies */
    int                nfact;            /* number of tracked facts */
    target_t          *auto_update;      /* target to resolve on fact changes */
    uint32_t           stamp;            /* update stamp */
    mrp_context_tbl_t *ctbl;             /* context variable table */
};


#endif /* __MURPHY_RESOLVER_TYPES_H__ */
