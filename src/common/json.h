#ifndef __MURPHY_JSON_H__
#define __MURPHY_JSON_H__

#include <stdarg.h>
#include <stdbool.h>
#include <json/json.h>
#include <json/linkhash.h>

#include <murphy/common/macros.h>

MRP_CDECL_BEGIN

/*
 * We use json-c as the underlying json implementation, However, we do
 * not want direct json-c dependencies to spread all over the code base
 * (at least not yet). So we try to define here an envelop layer that
 * hides json-c underneath.
 */

/** Type of a JSON object. */
typedef json_object mrp_json_t;

/** JSON object/member types. */
typedef enum {
    MRP_JSON_STRING  = json_type_string,
    MRP_JSON_BOOLEAN = json_type_boolean,
    MRP_JSON_INTEGER = json_type_int,
    MRP_JSON_DOUBLE  = json_type_double,
    MRP_JSON_OBJECT  = json_type_object,
    MRP_JSON_ARRAY   = json_type_array
} mrp_json_type_t;

/** Type for a JSON member iterator. */
typedef json_object_iter mrp_json_iter_t;

/** Create a new JSON object of the given type. */
mrp_json_t *mrp_json_create(mrp_json_type_t type, ...);

/** Deserialize a string to a JSON object. */
mrp_json_t *mrp_json_string_to_object(const char *str, int len);

/** Serialize a JSON object to a string. */
const char *mrp_json_object_to_string(mrp_json_t *o);

/** Add a reference to the given JSON object. */
mrp_json_t *mrp_json_ref(mrp_json_t *o);

/** Remove a reference from the given JSON object, freeing if it was last. */
void mrp_json_unref(mrp_json_t *o);

/** Get the type of a JSON object. */
mrp_json_type_t mrp_json_get_type(mrp_json_t *o);

/** Check if a JSON object has the given type. */
int mrp_json_is_type(mrp_json_t *o, mrp_json_type_t type);

/** Set a member of a JSON object. */
void mrp_json_add(mrp_json_t *o, const char *key, mrp_json_t *m);

/** Create a new JSON object and set it as a member of another object. */
mrp_json_t *mrp_json_add_member(mrp_json_t *o, const char *key,
                                mrp_json_type_t type, ...);

/** Convenience macros to add members of various basic types. */
#define mrp_json_add_string(o, key, s) \
    mrp_json_add_string(o, key, MRP_JSON_STRING, s)

#define mrp_json_add_integer(o, key, i) \
    mrp_json_add_member(o, key, MRP_JSON_INTEGER, i)

#define mrp_json_add_double(o, key, d) \
    mrp_json_add_string(o, key, MRP_JSON_DOUBLE, d)

#define mrp_json_add_boolean(o, key, b) \
    mrp_json_add_string(o, key, MRP_JSON_BOOLEAN, (int)b)

/** Add an array member from a native C array of the given type. */
mrp_json_t *mrp_json_add_array(mrp_json_t *o, const char *key,
                               mrp_json_type_t type, ...);

/** Convenience macros for adding arrays of various basic types. */
#define mrp_json_add_string_array(o, key, arr, size) \
    mrp_json_add_array(o, key, MRP_JSON_STRING, arr, size)

#define mrp_json_add_int_array(o, key, arr, size) \
    mrp_json_add_array(o, key, MRP_JSON_INTEGER, arr, size)

#define mrp_json_add_double_array(o, key, arr, size) \
    mrp_json_add_array(o, key, MRP_JSON_DOUBLE, arr, size)

#define mrp_json_add_boolean_array(o, key, arr, size) \
    mrp_json_add_array(o, key, MRP_JSON_BOOLEAN, arr, size)

/** Get the member of a JSON object as a json object. */
mrp_json_t *mrp_json_get(mrp_json_t *o, const char *key);

/** Get the member of a JSON object in a type specific format. */
int mrp_json_get_member(mrp_json_t *o, const char *key,
                        mrp_json_type_t type, ...);

/** Delete a member of a JSON object. */
void mrp_json_del_member(mrp_json_t *o, const char *key);

/** Get the length of a JSON array object. */
int mrp_json_array_length(mrp_json_t *a);

/** Append a JSON object to an array object. */
int mrp_json_array_append(mrp_json_t *a, mrp_json_t *v);

/** Create and append a new item to a JSON array object. */
mrp_json_t *mrp_json_array_append_item(mrp_json_t *a, mrp_json_type_t type,
                                       ...);

/** Add a JSON object to a given index of an array object. */
int mrp_json_array_set(mrp_json_t *a, int idx, mrp_json_t *v);

/** Add a JSON object to a given index of an array object. */
int mrp_json_array_set_item(mrp_json_t *a, int idx, mrp_json_type_t type, ...);

/** Get the object at a given index of a JSON array object. */
mrp_json_t *mrp_json_array_get(mrp_json_t *a, int idx);

/** Get the element of a JSON array object at an index. */
int mrp_json_array_get_item(mrp_json_t *a, int idx, mrp_json_type_t type, ...);

/** Iterate through the members of an object. */
#define mrp_json_foreach_member(o, k, v, it)                    \
    for (it.entry = json_object_get_object((o))->head;          \
    (it.entry ?                                                 \
     (k = it.key = it.entry->k,                                 \
      v = it.val = (mrp_json_t *)it.entry->v,                   \
      it.entry) : 0);                                           \
         it.entry = it.entry->next)

MRP_CDECL_END

#endif /* __MURPHY_JSON_H__ */
