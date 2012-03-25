#ifndef __MDB_ASSERT_H__
#define __MDB_ASSERT_H__


#define MDB_ASSERT(cond, errcode, retval) \
    do {                                  \
        if (!(cond)) {                    \
            errno = errcode;              \
            return retval;                \
        }                                 \
    } while(0)

#define MDB_CHECKARG(cond, retval)      MDB_ASSERT(cond, EINVAL, retval)
#define MDB_PREREQUISITE(cond, retval)  MDB_ASSERT(cond, EIO, retval)


#endif  /* __MDB_ASSERT_H__ */
