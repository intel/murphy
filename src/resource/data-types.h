/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MURPHY_DATA_TYPES_H__
#define __MURPHY_DATA_TYPES_H__

#include <stdint.h>
#include <stdbool.h>

#include <murphy-db/mqi-types.h>

/*
 * Remember: this should be smaller than
 * sizeof(mrp_zone_mask_t) * 8
 */
#define MRP_ZONE_MAX            8
#define MRP_ZONE_MASK           (((mrp_zone_mask_t)1 << MRP_ZONE_MAX) - 1)

#define MRP_KEY_STAMP_BITS      27
#define MRP_KEY_STATE_BITS      1
#define MRP_KEY_USAGE_BITS      1
#define MRP_KEY_PRIORITY_BITS   3

#define MRP_ZONE_ID_INVALID        (~(uint32_t)0)
#define MRP_RESOURCE_ID_INVALID    (~(uint32_t)0)
#define MRP_RESOURCE_REQNO_INVALID (~(uint32_t)0)

#define MRP_RESOURCE_MAX  (sizeof(mrp_resource_mask_t) * 8)
#define MRP_ATTRIBUTE_MAX (sizeof(mrp_attribute_mask_t) * 8)

typedef enum   mrp_resource_state_e     mrp_resource_state_t;
typedef enum   mrp_resource_order_e     mrp_resource_order_t;
typedef enum   mrp_resource_access_e    mrp_resource_access_t;
typedef enum   mrp_resource_event_e     mrp_resource_event_t;

typedef struct mrp_resource_client_s    mrp_resource_client_t;
typedef union  mrp_attr_value_u         mrp_attr_value_t;
typedef struct mrp_attr_def_s           mrp_attr_def_t;
typedef struct mrp_attr_s               mrp_attr_t;
typedef struct mrp_zone_def_s           mrp_zone_def_t;
typedef struct mrp_zone_s               mrp_zone_t;
typedef struct mrp_application_class_s  mrp_application_class_t;
typedef struct mrp_resource_owner_s     mrp_resource_owner_t;
typedef struct mrp_resource_set_s       mrp_resource_set_t;
typedef struct mrp_resource_def_s       mrp_resource_def_t;
typedef struct mrp_resource_s           mrp_resource_t;
typedef struct mrp_resource_mgr_ftbl_s  mrp_resource_mgr_ftbl_t;
typedef struct mrp_resource_mgr_s       mrp_resource_mgr_t;

typedef struct mrp_resource_ownersref_s mrp_resource_ownersref_t;
typedef struct mrp_resource_setref_s    mrp_resource_setref_t;

typedef uint32_t                        mrp_resource_mask_t;
typedef uint32_t                        mrp_attribute_mask_t;
typedef uint32_t                        mrp_zone_mask_t;


enum mrp_resource_state_e {
    mrp_resource_no_request = 0,
    mrp_resource_release,
    mrp_resource_acquire,
    mrp_resource_pending_release,
};


enum mrp_resource_access_e {
    MRP_RESOURCE_ACCESS_NONE  = 0,
    MRP_RESOURCE_READ  = 1,
    MRP_RESOURCE_WRITE = 2,
    MRP_RESOURCE_RW    = (MRP_RESOURCE_READ | MRP_RESOURCE_WRITE)
};

enum mrp_resource_order_e {
    MRP_RESOURCE_ORDER_UNKNOWN = 0,
    MRP_RESOURCE_ORDER_FIFO,
    MRP_RESOURCE_ORDER_LIFO
};

union mrp_attr_value_u {
    const char  *string;
    int32_t      integer;
    uint32_t     unsignd;
    double       floating;
};

struct mrp_attr_def_s {
    const char            *name;
    mrp_resource_access_t  access;
    mqi_data_type_t        type;
    mrp_attr_value_t       value;
};

struct mrp_attr_s {
    const char      *name;
    mqi_data_type_t  type;
    mrp_attr_value_t value;
};


typedef void (*mrp_resource_event_cb_t)(uint32_t, mrp_resource_set_t *, void*);

enum mrp_resource_event_e {
    MRP_RESOURCE_EVENT_UNKNOWN = 0,
    MRP_RESOURCE_EVENT_CREATED,
    MRP_RESOURCE_EVENT_DESTROYED,
    MRP_RESOURCE_EVENT_ACQUIRE,
    MRP_RESOURCE_EVENT_RELEASE
};

typedef void (*mrp_manager_notify_func_t)(mrp_resource_event_t, mrp_zone_t *,
                                          mrp_application_class_t *,
                                          mrp_resource_t *, void *);
typedef void (*mrp_manager_init_func_t)(mrp_zone_t *, void *);
typedef bool (*mrp_manager_alloc_func_t)(mrp_zone_t *,mrp_resource_t *,void*);
typedef void (*mrp_manager_free_func_t)(mrp_zone_t *,mrp_resource_t *,void *);
typedef bool (*mrp_manager_advice_func_t)(mrp_zone_t *,mrp_resource_t*,void *);
typedef void (*mrp_manager_commit_func_t)(mrp_zone_t *, void *);

struct mrp_resource_mgr_ftbl_s  {
    mrp_manager_notify_func_t   notify;
    mrp_manager_init_func_t     init;
    mrp_manager_alloc_func_t    allocate;
    mrp_manager_free_func_t     free;
    mrp_manager_advice_func_t   advice;
    mrp_manager_commit_func_t   commit;
};



#endif  /* __MURPHY_DATA_TYPES_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
