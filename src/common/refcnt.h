#ifndef __MURPHY_REFCNT_H__
#define __MURPHY_REFCNT_H__

/*
 * A place/typeholder, so we can switch easily to atomic type
 * if/when necessary.
 */

typedef int mrp_refcnt_t;

static inline void *_mrp_ref_obj(void *obj, off_t offs)
{
    mrp_refcnt_t *refcnt;
    
    if (obj != NULL) {
	refcnt = obj + offs;
	(*refcnt)++;
    }

    return obj;
}


static inline int _mrp_unref_obj(void *obj, off_t offs)
{
    mrp_refcnt_t *refcnt;

    if (obj != NULL) {
	refcnt = obj + offs;
	--(*refcnt);

	if (*refcnt <= 0)
	    return TRUE;
    }

    return FALSE;
}


static inline void mrp_refcnt_init(mrp_refcnt_t *refcnt)
{
    *refcnt = 1;
}

#define mrp_ref_obj(obj, member)					\
    (typeof(obj))_mrp_ref_obj(obj, MRP_OFFSET(typeof(*(obj)), member))

#define mrp_unref_obj(obj, member)					\
    _mrp_unref_obj(obj, MRP_OFFSET(typeof(*(obj)), member))

#endif /* __MURPHY_REFCNT_H__ */
