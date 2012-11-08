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

#ifndef __MQL_MQL_H__
#define __MQL_MQL_H__

#include <murphy-db/mql-statement.h>
#include <murphy-db/mql-trigger.h>

/**
 * @brief execute a series of MQL statements stored in a file
 *
 * This function is to execute a series of MQL statements stored in a file.
 * The MQL statements supposed to separated with ';'. No ';' shall follow
 * the last statement. In case of an error the the execution of the statements
 * will stop, errno is set and the function will return -1. Error messages
 * will be written to stderr. Currently there is no programmatic way to figure
 * out what statement failed and how many statement were sucessfully executed.
 *
 * @param [in] file  is the path of the file, eg. "~/mql/create_table.mql"
 *
 * @return If the execution failed mql_exec_file() returns -1 and errno is
 *         set. It returns 0 if all the statements in the file were
 *         successfully executed.
 */
int mql_exec_file(const char *file);

/**
 * @brief execute an MQL statement
 *
 * This function is to execute a single MQL statement. The result of the
 * operation is returned in a data structure. The returned result can be
 * either the type of the requested result or an error result. It is
 * recommended that the returned result will be checked first by calling
 * mql_result_is_success() function.
 *
 * To process a result the relevant mql_result_xxx() functions are to use.
 * Values returned by the mql_result_xxx() functions are valid till the
 * next MQL commit or rollback. For instance accessing the returned strings
 * after a commit might lead to segfaults.
 *
 * The returned result should be freed by mql_result_free().
 *
 * @param [in] result_type   specifies the expected type of the returned
 *                           result. However, if the execution failed for
 *                           some reason the returned result will have
 *                           mql_result_error type.
 *
 * @param [in] statement     is the string of the MQL statement to execute
 *
 * @code
 *    #include <stdio.h>
 *    #include <murphy-db/mql.h>
 *
 *    const char *statement = "SELECT * FROM persons WHERE name = 'murphy'";
 *    mql_result_t *r = mql_exec_string(mql_result_type_string, statement);
 *
 *    if (mql_result_is_success(r))
 *      printf("the result of the query:\n%s\n", r->mql_result_string_get(r));
 * @endcode
 */
mql_result_t *mql_exec_string(mql_result_type_t result_type,
                              const char *statement);

/**
 * @brief precompile an MQL statement
 *
 * @param [in] statement     is the string of the MQL statement to execute
 *
 * For performance optimisation purposes the execution of MQL statements
 * can be done in a precompilation and an execution phase. This allows
 * a single first phase for frequently executed MQL statements followed
 * by a series of second phase.
 *
 * Precompilation is the parsing of the ASCII MQL statement, making all the
 * necessary lookups, generating the data structures what the the underlying
 * MQI interface needs for the execution and packing all these information
 * into a dynamically allocated memory block.
 *
 * mql_bind_value() can be used to assign values for parameters of the
 * preecompiled statement, if any.
 *
 * A precompiled statement can be executed by mql_exec_statement().
 *
 * Precompiled statements should be freed by mql_statement_free() when they
 * are not needed any more.
 *
 * @return mql_precompile() returns a pointer to the precompile statement in
 *         case the precompilation succeeded or NULL if the precompilation
 *         failed. In the later case errno is set to give a clue what went
 *         wrong.
 *
 *         Note that a successfull precompilation do not garantie the
 *         successfull execution of the precompiled statement. For instance
 *         a successfully precompiled of a SELECT statement will fail
 *         if the table, it tries to operate on, was meanwhile deleted.
 * @code
 *    #include <stdio.h>
 *    #include <string.h>
 *    #include <errno.h>
 *    #include <murphy-db/mql.h>
 *
 *    const char *group[] = {"joe", "jack", "murphy", NULL};
 *    const char *query = "SELECT name, email FROM persons WHERE name = %s";
 *    mql_statement_t *stmnt;
 *    mql_result_t *r;
 *    char *person;
 *    int i;
 *
 *    if ((stmnt = mql_precompilation(query)) == NULL)
 *      printf("precompilation failed (%d): %s\n", errno, strerror(errno));
 *    else {
 *      for (i = 0;  (person = group[i]) != NULL;  i++) {
 *        if (mql_bind_value(stmnt, 0,mqi_varchar, person) < 0)
 *          printf("bindig failed (%d): %s\n", errno, strerror(errno));
 *        else {
 *          r = mql_exec_statement(mql_result_string, stmnt);
 *          if (!mql_result_is_success(r))
 *            printf("exec failed %d: %s\n",
 *                   mql_result_error_get_code(r),
 *                   mql_result_error_get_message(r));
 *          else
 *            printf("query %d\n%s\n", i, mql_result_string_get(r));
 *        }
 *      }
 *    }
 *
 *    mql_statement_free(stmnt);
 * @endcode
 */
mql_statement_t *mql_precompile(const char *statement);


#endif  /* __MQL_MQL_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
