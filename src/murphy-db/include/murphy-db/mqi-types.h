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

#ifndef __MQI_TYPES_H__
#define __MQI_TYPES_H__

#include <stdint.h>
#include <stdbool.h>

/** macro to tag a variable unused */
#define MQI_UNUSED(var) ((void)var)

/** maximum number of rows a query can produce */
#define MQI_QUERY_RESULT_MAX   8192
/** the maximum number columns a table can have */
#define MQI_COLUMN_MAX         ((int)(sizeof(mqi_bitfld_t) * 8))
/** maximum length of a condition table (i.e. array of mqi_cond_entry_t) */
#define MQI_COND_MAX           64
#define MQL_PARAMETER_MAX      16
/** maximum depth for nested transactions */
#define MQI_TXDEPTH_MAX        16

/**
 * mqi_handle_t value for nonexisting handle. Zero is a valid handle
 * thus casting a zero to mqi_handle_t will produce a valid handle
 * (remeber this when using static mqi_handle_t).
 */
#define MQI_HANDLE_INVALID     (~((mqi_handle_t)0))

/**
 * Stamp for a non-existing table or a table without any inserts ever.
 */
#define MQI_STAMP_NONE ((uint32_t)0)


#define MQI_DIMENSION(array)  \
    (sizeof(array) / sizeof(array[0]))

#define MQI_OFFSET(structure, member)  \
    ((int)((char *)((&((structure *)0)->member)) - (char *)0))

#define MQI_BIT(b)            (((mqi_bitfld_t)1) << (b))

#define MQL_BIND_INDEX_BITS   8
#define MQL_BIND_INDEX_MAX    (1 << MQL_BIND_INDEX_BITS)
#define MQL_BIND_INDEX_MASK   (MQL_BIND_INDEX_MAX - 1)

#define MQL_BINDABLE          (1 << (MQL_BIND_INDEX_BITS + 0))
#define MQL_BIND_INDEX(v)     ((v) & MQL_BIND_INDEX_MASK)

#define MQI_COLUMN_KEY        (1UL << 0)
#define MQI_COLUMN_AUTOINCR   (1UL << 1)

enum mqi_data_type_e {
    mqi_error = -1,    /* not a data type; used to return error conditions */
    mqi_unknown = 0,
    mqi_varchar,
    mqi_string = mqi_varchar,
    mqi_integer,
    mqi_unsignd,
    mqi_floating,
    mqi_blob,
};

enum mqi_operator_e {
    mqi_done = 0,
    mqi_end  = mqi_done,
    mqi_begin,                  /* expression start */
    mqi_and,
    mqi_or,
    mqi_less,
    mqi_leq,
    mqi_eq,
    mqi_geq,
    mqi_gt,
    mqi_not,
    mqi_operator_max
};

enum mqi_cond_entry_type_e {
    mqi_operator,
    mqi_variable,
    mqi_column
};

enum mqi_event_type_e {
    mqi_event_unknown = 0,
    mqi_column_changed,
    mqi_row_inserted,
    mqi_row_deleted,
    mqi_table_created,
    mqi_table_dropped,
    mqi_transaction_start,
    mqi_transaction_end
};



typedef uint32_t  mqi_handle_t;
typedef uint32_t  mqi_bitfld_t;

typedef enum mqi_data_type_e         mqi_data_type_t;
typedef struct mqi_column_def_s      mqi_column_def_t;
typedef struct mqi_column_desc_s     mqi_column_desc_t;

typedef enum mqi_operator_e          mqi_operator_t;
typedef struct mqi_variable_s        mqi_variable_t;
typedef enum mqi_cond_entry_type_e   mqi_cond_entry_type_t;
typedef struct mqi_cond_entry_s      mqi_cond_entry_t;

typedef enum mqi_event_type_e        mqi_event_type_t;
typedef union mqi_event_u            mqi_event_t;

typedef struct mqi_change_table_s    mqi_change_table_t;
typedef struct mqi_change_select_s   mqi_change_select_t;
typedef struct mqi_change_coldsc_s   mqi_change_coldsc_t;
typedef union mqi_change_data_u      mqi_change_data_t;
typedef struct mqi_change_value_s    mqi_change_value_t;

typedef struct mqi_column_event_s    mqi_column_event_t;
typedef struct mqi_row_event_s       mqi_row_event_t;
typedef struct mqi_table_event_s     mqi_table_event_t;
typedef struct mqi_transact_event_s  mqi_transact_event_t;

typedef void (*mqi_trigger_cb_t)(mqi_event_t *, void *);



struct mqi_column_def_s {
    const char      *name;
    mqi_data_type_t  type;
    int              length;
    uint32_t         flags;
};

struct mqi_column_desc_s {
    int     cindex;              /* column index */
    int     offset;              /* offset within the data struct */
};

struct mqi_variable_s {
    mqi_data_type_t  type;
    uint32_t         flags;
    union {
        char       **varchar;
        int32_t     *integer;
        uint32_t    *unsignd;
        double      *floating;
        void       **blob;
        void        *generic;
    } v;
};


struct mqi_cond_entry_s {
    mqi_cond_entry_type_t  type;
    union {
        mqi_operator_t     operator_;
        mqi_variable_t     variable;
        int                column;     /* column index actually */
    } u;
};


struct mqi_change_table_s {
    mqi_handle_t  handle;
    const char   *name;
};

struct mqi_change_select_s {
    int   length;
    void *data;
};

struct mqi_change_coldsc_s {
    int         index;
    const char *name;
};

union mqi_change_data_u {
    char    *varchar;
    char    *string;
    int32_t  integer;
    uint32_t unsignd;
    double   floating;
    void    *generic;
};

struct mqi_change_value_s {
    mqi_data_type_t   type;
    mqi_change_data_t old;
    mqi_change_data_t new_;
};


struct mqi_column_event_s {
    mqi_event_type_t    event;
    mqi_change_table_t  table;
    mqi_change_coldsc_t column;
    mqi_change_value_t  value;
    mqi_change_select_t select;
};

struct mqi_row_event_s {
    mqi_event_type_t    event;
    mqi_change_table_t  table;
    mqi_change_select_t select;
};

struct mqi_table_event_s {
    mqi_event_type_t   event;
    mqi_change_table_t table;
};

struct mqi_transact_event_s {
    mqi_event_type_t  event;
    uint32_t          depth;
};


union mqi_event_u {
    mqi_event_type_t     event;
    mqi_column_event_t   column;
    mqi_row_event_t      row;
    mqi_table_event_t    table;
    mqi_transact_event_t transact;
};


const char *mqi_data_type_str(mqi_data_type_t);

int mqi_data_compare_integer(int, void *, void *);
int mqi_data_compare_unsignd(int, void *, void *);
int mqi_data_compare_string(int, void *, void *);
int mqi_data_compare_pointer(int, void *, void *);
int mqi_data_compare_varchar(int, void *, void *);
int mqi_data_compare_blob(int, void *, void *);

int mqi_data_print_integer(void *, char *, int);
int mqi_data_print_unsignd(void *, char *, int);
int mqi_data_print_string(void *, char *, int);
int mqi_data_print_pointer(void *, char *, int);
int mqi_data_print_varchar(void *, char *, int);
int mqi_data_print_blob(void *, char *, int);


#endif /* __MQI_TYPES_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
