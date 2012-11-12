#ifndef foomurphyresourceprivatefoo
#define foomurphyresourceprivatefoo

#include "resource-api.h"

typedef struct {
    uint32_t        dim;
    const char     *elems[0];
} string_array_t;

typedef struct {
    const char     *name;
    char            type;       /* s:char *, i:int32_t, u:uint32_t, f:double */
    union {
        const char *string;
        int32_t     integer;
        uint32_t    unsignd;
        double      floating;
    };
} attribute_t;

typedef struct {
    uint32_t       dim;
    attribute_t    elems[0];
} attribute_array_t;

typedef struct {
    const char        *name;
    attribute_array_t *attrs;
} resource_def_t;

typedef struct {
    uint32_t          dim;
    resource_def_t    defs[0];
} resource_def_array_t;

struct murphy_resource_private_s {
    murphy_resource *pub; /* composition */
    murphy_resource_set *set; /* owning set */

    attribute_array_t *attrs;
};

struct murphy_resource_set_private_s {
    murphy_resource_set *pub; /* composition */
    uint32_t id;
    uint32_t seqno;

    murphy_resource_callback cb;
    void *user_data;

    mrp_list_hook_t hook;
};

struct murphy_resource_context_private_s {
    int connection_id;

    mrp_htbl_t *rset_mapping;

    murphy_state_callback cb;
    void *user_data;

    mrp_mainloop_t *ml;
    mrp_sockaddr_t saddr;
    mrp_transport_t *transp;
    bool connected;

    /* do we know the resource and class states? */
    string_array_t *classes;
    resource_def_array_t *available_resources;

    /* sometimes we need to know which query was answered */
    uint32_t next_seqno;

    mrp_list_hook_t pending_sets;
};




#endif /* foomurphyresourceprivatefoo */
