#ifndef __MQI_TYPES_H__
#define __MQI_TYPES_H__

#include <stdint.h>

#define MQI_QUERY_RESULT_MAX  8192
#define MQI_COLUMN_MAX        64
#define MQI_COND_MAX          64
#define MQL_PARAMETER_MAX     16
#define MQI_TXDEPTH_MAX       16

#define MQI_DIMENSION(array)  \
    (sizeof(array) / sizeof(array[0]))

#define MQI_OFFSET(structure, member)  \
    ((int)((void *)((&((structure *)0)->member)) - (void *)0))

#define MQL_BIND_INDEX_BITS   8
#define MQL_BIND_INDEX_MAX    (1 << MQL_BIND_INDEX_BITS)
#define MQL_BIND_INDEX_MASK   (MQL_BIND_INDEX_MAX - 1)

#define MQL_BINDABLE           (1 << (MQL_BIND_INDEX_BITS + 0))
#define MQL_BIND_INDEX(v)      ((v) & MQL_BIND_INDEX_MASK)

#define MQI_COLUMN_KEY          (1UL << 0)
#define MQI_COLUMN_AUTOINCR     (1UL << 1)


typedef enum {
    mqi_error = -1,    /* not a data type; used to return error conditions */
    mqi_unknown = 0,
    mqi_varchar,
    mqi_string = mqi_varchar,
    mqi_integer,
    mqi_unsignd,
    mqi_floating,
    mqi_blob,
} mqi_data_type_t;

typedef struct {
    const char      *name;
    mqi_data_type_t  type;
    int              length;
    uint32_t         flags;
} mqi_column_def_t;

typedef struct {
    int     cindex;              /* column index */
    int     offset;              /* offset within the data struct */
} mqi_column_desc_t;

typedef enum {
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
} mqi_operator_t;


typedef struct {
    mqi_data_type_t  type;
    uint32_t         flags;
    union {
        char       **varchar;
        int32_t     *integer;
        uint32_t    *unsignd;
        double      *floating;
        void       **blob;
        void        *generic;
    };
} mqi_variable_t;

typedef enum {
    mqi_operator,
    mqi_variable,
    mqi_column
} mqi_cond_entry_type_t;

typedef struct {
    mqi_cond_entry_type_t  type;
    union {
        mqi_operator_t     operator;
        mqi_variable_t     variable;
        int                column;     /* column index actually */
    };
} mqi_cond_entry_t;


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
