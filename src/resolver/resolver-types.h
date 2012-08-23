#ifndef __MURPHY_RESOLVER_TYPES_H__
#define __MURPHY_RESOLVER_TYPES_H__

#include <stdint.h>

#include <murphy/common/hashtbl.h>
#include <murphy/resolver/script.h>

typedef struct target_s target_t;        /* opaque type for resolver targets */
typedef struct fact_s   fact_t;          /* opaque type for tracked facts */

/*
 * a resolver target
 */
struct target_s {
    char         *name;                  /* target name */
    uint32_t      stamp;                 /* touch-stamp */
    char        **depends;               /* dependencies stated in the input */
    int           ndepend;               /* number of dependencies */
    int          *update_facts;          /* facts to check when updating */
    int          *update_targets;        /* targets to check when updating */
    mrp_script_t *script;                /* update script if any, or NULL */
};


/*
 * a tracked fact
 */
struct fact_s {
    char     *name;                      /* fact name */
    uint32_t  stamp;                     /* touch-stamp */
};


/*
 * a context variable (used to pass contextual data to resolver targets)
 */
typedef struct {
    const char        *name;             /* variable name */
    mrp_script_type_t  type;             /* type if declared */
    int                id;               /* variable id */
} context_var_t;


/*
 * a context frame (a set of context variable values)
 */
typedef struct context_value_s context_value_t;
struct context_value_s {
    int                 id;              /* variable id */
    mrp_script_value_t  value;           /* value for this variable */
    context_value_t    *next;            /* next value in this frame */
};

typedef struct context_frame_s context_frame_t;
struct context_frame_s {
    context_value_t *values;             /* hook to more value */
    context_frame_t *prev;               /* previous frame */
};


typedef struct {
    context_var_t   *variables;          /* known/declared context variables */
    int              nvariable;          /* number of variables */
    mrp_htbl_t      *names;              /* variable name to id mapping */
    context_frame_t *frame;              /* active frame */
} context_tbl_t;


struct mrp_resolver_s {
    target_t      *targets;              /* targets defined in the ruleset */
    int            ntarget;              /* number of targets */
    fact_t        *facts;                /* facts tracked as dependencies */
    int            nfact;                /* number of tracked facts */
    uint32_t       stamp;                /* update stamp */
    context_tbl_t *ctbl;                 /* context variable table */
};


#endif /* __MURPHY_RESOLVER_TYPES_H__ */
