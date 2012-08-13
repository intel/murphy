#ifndef __MURPHY_RESOLVER_TYPES_H__
#define __MURPHY_RESOLVER_TYPES_H__

#include <stdint.h>


typedef struct target_s target_t;
typedef uint32_t        tstamp_t;
typedef struct fact_s   fact_t;

typedef enum {
    PREREQ_UNKNOWN = 0,
    PREREQ_FACT,
    PREREQ_TARGET
} prereq_type_t;

typedef struct {
    prereq_type_t type;                  /* PREREQ_FACT or PREREQ_TARGET */
    union {
        char     *name;                  /* fact or target name */
        target_t *target;                /* pointer to prerequisit target */
    };
} prereq_t;

struct target_s {
    tstamp_t  stamp;                     /* touch-stamp */
    char     *name;                      /* target name */
    char    **depends;                   /* dependencies stated in the input */
    int       ndepend;                   /* number of dependencies */
    int      *update_facts;              /* facts to check when updating */
    int      *update_targets;            /* targets to update when updating */
    char     *script;                    /* update script if any, or NULL */
};

struct fact_s {
    char     *name;
};


struct mrp_resolver_s {
    target_t *targets;
    int       ntarget;
    fact_t   *facts;
    int       nfact;
};


#endif /* __MURPHY_RESOLVER_TYPES_H__ */
