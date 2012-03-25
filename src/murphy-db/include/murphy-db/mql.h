#ifndef __MQL_MQL_H__
#define __MQL_MQL_H__

#include <murphy-db/statement.h>

int mql_exec_file(const char *);
mql_result_t *mql_exec_string(mql_result_type_t, const char *);
mql_statement_t *mql_precompile(const char *);


#endif  /* __MQL_MQL_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
