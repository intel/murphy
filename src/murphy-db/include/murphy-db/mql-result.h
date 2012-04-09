#ifndef __MQL_RESULT_H__
#define __MQL_RESULT_H__

#include <murphy-db/mqi-types.h>


typedef enum mql_result_type_e  mql_result_type_t;
typedef struct mql_result_s     mql_result_t;

enum mql_result_type_e {
    mql_result_error = -1,
    mql_result_unknown = 0,
    mql_result_dontcare = mql_result_unknown,
    mql_result_event,
    mql_result_columns,
    mql_result_rows,
    mql_result_string,
    mql_result_list,
};

struct mql_result_s {
    mql_result_type_t  type;
    uint8_t            data[0];
};

int              mql_result_is_success(mql_result_t *);

int              mql_result_error_get_code(mql_result_t *);
const char      *mql_result_error_get_message(mql_result_t *);

int              mql_result_columns_get_column_count(mql_result_t *);
const char      *mql_result_columns_get_name(mql_result_t *, int);
mqi_data_type_t  mql_result_columns_get_type(mql_result_t *, int);
int              mql_result_columns_get_length(mql_result_t *, int);

int              mql_result_rows_get_row_count(mql_result_t *);
const char      *mql_result_rows_get_string(mql_result_t*, int,int, char*,int);
int32_t          mql_result_rows_get_integer(mql_result_t *, int,int);
uint32_t         mql_result_rows_get_unsigned(mql_result_t *, int,int);
double           mql_result_rows_get_floating(mql_result_t *, int,int);

const char      *mql_result_string_get(mql_result_t *);

int              mql_result_list_get_length(mql_result_t *);
const char      *mql_result_list_get_string(mql_result_t *, int, char *, int);
int32_t          mql_result_list_get_integer(mql_result_t *, int);
int32_t          mql_result_list_get_unsigned(mql_result_t *, int);
double           mql_result_list_get_floating(mql_result_t *, int);

void             mql_result_free(mql_result_t *);


#endif /* __MQL_RESULT_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
