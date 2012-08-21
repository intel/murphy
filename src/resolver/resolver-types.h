#ifndef __MURPHY_RESOLVER_TYPES_H__
#define __MURPHY_RESOLVER_TYPES_H__

#include <stdint.h>

#include <murphy/resolver/script.h>

typedef struct target_s target_t;        /* opaque type for resolver targets */
typedef uint32_t        tstamp_t;        /* DB 'touch-stamp' for facts */
typedef struct fact_s   fact_t;          /* opaque type for tracked facts */

struct target_s {
    char         *name;                  /* target name */
    tstamp_t      stamp;                 /* touch-stamp */
    char        **depends;               /* dependencies stated in the input */
    int           ndepend;               /* number of dependencies */
    int          *update_facts;          /* facts to check when updating */
    int          *update_targets;        /* targets to check when updating */
    mrp_script_t *script;                /* update script if any, or NULL */
};

struct fact_s {
    char     *name;                      /* fact name */
    tstamp_t  stamp;                     /* touch-stamp */
};

struct mrp_resolver_s {
    target_t *targets;                   /* targets defined in the ruleset */
    int       ntarget;                   /* number of targets */
    fact_t   *facts;                     /* facts tracked as dependencies */
    int       nfact;                     /* number of tracked facts */
    tstamp_t  stamp;                     /* */
};


#endif /* __MURPHY_RESOLVER_TYPES_H__ */
