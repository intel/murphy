/*
 * Copyright (c) 2014, Intel Corporation
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

#ifndef __MURPHY_OBJECT_H__
#define __MURPHY_OBJECT_H__

#include <stdint.h>
#include <stdbool.h>
#include <murphy/common/macros.h>

MRP_CDECL_BEGIN

/**
 * \addtogroup MurphyCommonInfra
 * @{
 *
 * @file object.h
 *
 * @brief Murphy runtime object extension infrastructure.
 *
 * The Murphy object extension mechanism aims to provide a lightweight
 * easy to use mechanism for extending objects at runtime with arbitrary
 * data. The implementation tries to strike the right balance between
 * being flexible enough, with low runtime overhead, while also providing
 * limited support for runtime type-checking.
 *
 * The primary intention behind the object extension mechanism is to help
 * us keeping the Murphy infrastructure generic and reusable. We can keep
 * the common infra clean, free of any domain-specific data and let dyna-
 * mically loadable plugins use this mechanism to attach application- and
 * domain-specific data to the common infra as needed. A typical example
 * could be, for instance, to write a plugin that provides Python bindings
 * for the Murphy mainloop. If runtime extensions are enabled for the
 * mainloop data structure, the plugin can directly hook its own data
 * necessary for interacting with the Python interpreter to the mainloop
 * itself as an extension.
 *
 * Two things need to happen before an extension can be attached to any
 * object.
 *
 *   1) object types which are subject to runtime extension need to be
 *      registered as such with the object extension infrastructure, and
 *   2) any potential extensions to an object type need to be registered
 *      with the object extension infrastructure
 *
 * Once an object type is registered as extensible, practically there
 * is no limit how many extensions and of what type it can have. The
 * only limitation is that the name of an extended member within a type
 * must be unique, similar to the uniqueness requirements for ordinary
 * structure/union members.
 *
 * A typical usage of the runtime object extension mechanism is to have
 * a dynamically loaded plugin hook its state bookkeeping into a pre-
 * existing Murphy common/core object that has been marked extensible.
 *
 * One particular implication of the implementation choices is that you
 * can't extend an object that has not been designed and registered for
 * extensibility. While it would be technically possible to implement
 * extensions otherwise, the design choice has been made in favour of
 * minimizing runtime penalty when accessing extensions. Also since we
 * the burden of making an object extensible has been made fairly small,
 * the current design choices seem to present an acceptable compromise.
 * They provide an easy to use and fairly efficient mechanism, requiring
 * minimal extra code from the one implementing an extensible object.
 *
 * Usage example:
 *
 * @code
 *
 * #include <murphy/common/object.h>
 *
 * typedef struct object_t {
 *     int foo;
 *     char *bar;
 *     MRP_EXTENSIBLE;
 * };
 *
 * ...
 *
 * // register object_t as an extensible object
 * uint32_t id = MRP_EXTENSIBLE_TYPE(object_t);
 *
 * if (id == MRP_EXTENSIBLE_NONE)
 *     bail_out("Failed to register object_t as extensible...");
 * ...
 *
 * // create an instance of object_t
 * object_t *o = mrp_allocz(sizeof(*o));
 * if (!o)
 *     bail_out("Failed to create object_t instnace...");
 * mrp_extensible_init(o, id);
 *
 * ...
 *
 * // somewhere else you can register extensions for object_t
 * static uint32_t ext_frob, ext_xyzz;
 *
 * static void free_frob(void *obj, uint32_t ext, void *value)
 * {
 *      if (ext != ext_frob && ext != ext_xyzzy)
 *          bail_out("Ugh ? Called for incorrect extension...");
 *      else {
 *          if (value == NULL)
 *              return;
 *
 *          if (ext == ext_frob)
 *              free_my_frob(value);
 *          else
 *              free_my_xyzzy(value);
 *      }
 * }
 *
 * ext_frob = MRP_EXTEND_TYPE(object_t, frob_t * , frob , free_frob );
 * ext_xyzz = MRP_EXTEND_TYPE(object_t, xyzzy_t *, xyzzy, free_xyzzy);
 *
 * if (ext_frob == MRP_EXTENSION_NONE || ext_xyzzy == MRP_EXTENSION_NONE)
 *     bail_out("Failed to extend object_t by frob/xyzzy...");
 *
 * ...
 *
 * // you can assign values to extensions
 * frob_t  *frob  = make_me_a_frob("please");
 * xyzzy_t *xyzzy = make_me_a_xyzzy("now");
 *
 * if (MRP_EXTEND(o, ext_frob , frob_t * , frob ) < 0 ||
 *     MRP_EXTEND(o, ext_xyzzy, xyzzy_t *, xyzzy) < 0)
 *     bail_out("Failed to set frob or xyzzy extension of object_t %p...", o);
 *
 * ...
 *
 * // you can fetch the values of the extensions
 * frob_t  *f;
 * xyzzy_t *x;
 *
 * if ((f = MRP_EXTENSION(o, ext_frob , frob_t  *)) == NULL ||
 *     (x = MRP_EXTENSION(o, ext_xyzzy, xyzzy_t *)) == NULL)
 *     bail_out("Failed to get frob or xyzzy extension of object_t %p...", o);
 *
 * ...
 *
 * // you can also explicitly free extension values
 * mrp_extension_free(o, ext_frob);
 * // this effectively does the same for xyzzy...
 * MRP_EXTEND(o, ext_xyzzy, xyzzy_t *, NULL);
 *
 * @endcode
 */

/**
 * @brief Type used to administer a single extensible object type.
 */
typedef struct mrp_extensible_s mrp_extensible_t;

/**
 * @brief Type used to administer a single extension of an extensible type.
 */
typedef struct mrp_extension_s mrp_extension_t;

/**
 * @brief Type used to hook extensions into an extensible object.
 */
typedef struct mrp_extended_s mrp_extended_t;

/**
 * @brief Callback type to free the value of an object extension.
 */
typedef void (*mrp_extfree_t)(void *obj, uint32_t ext, void *value);

/**
 * @brief Reserved type for an invalid/failed/unknown extension.
 */
#define MRP_EXTENSION_NONE  ((uint32_t)0)

/**
 * @brief Reserved type for an invalid/failed/unknown extensible type.
 */
#define MRP_EXTENSIBLE_NONE ((uint32_t)0)

/**
 * @brief Metadata about a single extensible object type.
 */
struct mrp_extensible_s {
    char             *type;              /* object type name */
    size_t            size;              /* basic object size */
    size_t            offs;              /* offset to extensions */
    mrp_extension_t  *extensions;        /* registered extensions */
    uint32_t          nextension;        /* number of extensions */
};

/**
 * @brief Metadata about a single object extension.
 */
struct mrp_extension_s {
    const char    *type;                 /* extension type */
    const char    *name;                 /* extension name */
    uint32_t       id;                   /* extension id */
    mrp_extfree_t  free;                 /* value free callback */
    int            type_check : 1;       /* enable runtime type-check */
};

/**
 * @brief Active extensions within an instance of an extensible object.
 */
struct mrp_extended_s {
    uint32_t   id;                       /* extensible type id */
    void     **members;                  /* extended members */
    uint32_t   nmember;                  /* number of members */
};

/**
 * @brief Helper macro used in structure definitions for extensibility.
 *
 * Use this macro in the definition of those structures that you want to
 * make runtime-extensible. Simply include the macro among the members of
 * the structure. By convention this macro is preferably included after
 * the last member, although technically it can appear anywhere within
 * the structure.
 */
#define MRP_EXTENSIBLE mrp_extended_t _ext

/**
 * @brief Helper macro to register an object type for extensions.
 *
 * Use this macro to register your structure for runtime extensions with
 * the Murphy extensible object infrastructure.
 *
 * @param [in] _type  the type to register as an extensible object type
 */
#define MRP_EXTENSIBLE_TYPE(_type)                                      \
    mrp_extensible_register(#_type, sizeof(_type), MRP_OFFSET(_type, _ext))

/**
 * @brief Helper macro to register an extension for an object type.
 *
 * Use this macro to register an extension for an extensible object type.
 * The extended type does not need to be registered for extensions at the
 * time of invoking this macro.
 *
 * @param [in] _type      the type being extended
 * @param [in] _ext_type  the type of the extension
 * @param [in] _ext_name  the name of the extension
 * @param [in] _free      callback to call if/when the value of the extension
 *                        needs to be freed, or @NULL
 *
 * @return This macro evalutes to the unique @uint32_t identifier of the
 *         registered extension, or @MRP_EXTENSION_NONE upon failure.
 */
#define MRP_EXTEND_TYPE(_type, _ext_type, _ext_name, _ext_free)         \
    mrp_extension_register(#_type, #_ext_type, #_ext_name, _ext_free)

/**
 * @brief Helper macro to set the value of an object extension.
 *
 * Use this macro to set the value of an extension of an object. Both the
 * object type and the extension need to be registered by the time this macro
 * is invoked.
 *
 * @param [in] _obj    pointer to the object instance to set the extension of
 * @param [in] _ext    unique identifier of the extension to set
 * @param [in] _type   type of the value being set for type checking
 * @param [in] _value  value to set the extension to
 *
 * @return This macro evaluates to 0 upon success, or -1 upon failure.
 *
 * Because of the way the macros are set up, when using @MRP_EXTEND and
 * @MRP_EXTENSION to operate on extended objects, the value of your extension
 * should always be a pointer (otherwise you might casting warnings). Even if
 * your extension consists of a single numeric scalar value, it is best to
 * allocate memory for it separately and set the pointer as the value of the
 * extension. This very seldom is an issue of practical importance as objects
 * tend to get extended by much more complex ways than just a trivial single
 * scalar. Keep this in mind, anyway.
 */
#define MRP_EXTEND(_obj, _ext, _type, _value)                           \
    _mrp_extension_set((void *)_obj, _ext, #_type, _value)

/**
 * @brief Helper macro to retrieve the value of an object extension.
 *
 * Use this macro to get the value of a registered object extension.
 *
 * @param [in] _obj   pointer to the object instance to get the value from
 * @param [in] _ext   uique identifier of the extension to get the value for
 * @param [in] _type  expected type of the value
 *
 * @return This macro evaluates to the value of the extension retrieved.
 *
 * Because of the way the macros are set up, when using @MRP_EXTEND and
 * @MRP_EXTENSION to operate on extended objects, the value of your extension
 * should always be a pointer (otherwise you might casting warnings). Even if
 * your extension consists of a single numeric scalar value, it is best to
 * allocate memory for it separately and set the pointer as the value of the
 * extension. This very seldom is an issue of practical importance as objects
 * tend to get extended by much more complex ways than just a trivial single
 * scalar. Keep this in mind, anyway.
 */
#define MRP_EXTENSION(_obj, _ext, _type)                                \
    (_type)_mrp_extension_get((_obj), (_ext), #_type)

/**
 * @brief Function to register an extensible object type.
 *
 * Function used by @MRP_EXTENSIBLE_TYPE to register an object type for
 * runtime extensions.
 *
 * @param [in] obj_type  name of the extensible object type
 * @param [in] size      size (sizeof) of the extensibe object
 * @param [in] offs      offset to extensions within the object type
 *
 * @return This function returns the unique @uint32_t extensible object type
 *         identifier for @obj_type, or @MRP_EXTENSIBLE_NONE upon error.
 *
 */
uint32_t mrp_extensible_register(const char *obj_type, size_t size, size_t offs);

/**
 * @brief Helper macro to forward declare an extensible object type.
 *
 * You can use this macro to forward declare a type for runtime extensions.
 * Forward-declaring will allocate a unique object type identifier for the
 * type and mark it as forward declared. Instances of the type cannot be
 * initialized or extended before the type is registered for extensions
 * using the @MRP_EXTENSIBLE_TYPE macro or its underlying mechanism.
 *
 * @param [in] _obj_type  the type to forward-declare for extensions
 *
 * @return This macro evaluates to the unique type identifier of the type.
 *         Once invoked, @MRP_EXTENSIBLE_TYPE will return the same id. Note
 *         that normally it is not necessary to invoke this macro. It wil be
 *         invoked on behalf of you whenever you try to register extensions
 *         for an unregistered type.
 */
#define mrp_extensible_declare(_obj_type)                               \
    mrp_extensible_register((_obj_type), 0, 0)

/**
 * @brief Helper macro to look up the unique type id for an extensible type.
 *
 * You can use this macro to look up the unique type identifier for a
 * registered or forward-delared extensible type.
 *
 * @param [in] _type  type to look up identifier for
 *
 * @return Evaluates to the unique type identifier of the given type, or
 *         @MRP_EXTENSIBLE_NONE if the type is unknown.
*/
#define mrp_extensible_id(_type) _mrp_extensible_id(#_type)

/**
 * @brief Functon to look up the unique type id for the named type.
 *
 * @param [in] type  type name to look up the unique identifier for
 *
 * @return Returns the unique identifier for the given type name, or
 *         @MRP_EXTENSIBLE_NONE if the named type is unknown.
 */
uint32_t _mrp_extensible_id(const char *type);

/**
 * @brief Function to initialize the extensible part of an object.
 *
 * Use this function to initialize the extensions of an object that you
 * have registered as extensible.
 *
 * @param [in] obj  pointer to the object instance to initialize
 * @param [in] id   unique identifier of the type for @obj
 *
 * @return Returns 0 upon success, -1 upon failure.
*/
int mrp_extensible_init(void *obj, uint32_t id);

/**
 * @brief Function to clean up the extensible part of an object.
 *
 * Use this function to do a final cleanup of the extension of an object
 * before freeing it.
 *
 * @param [in] obj  pointer to the object instance to clean up
 * @param [in] id   unique identifier of the type for @obj
 */
void mrp_extensible_cleanup(void *obj, uint32_t id);

/**
 * @brief  Check if an object instance looks to be of the given extensible type.
 *
 * You can use this function to check if an object is of a given type.
 *
 * @param [in] obj  object instance to check
 * @param [in] id   unique identifier of the expected object type
 *
 * @return Returns 0 if the types match, -1 otherwise.
 */
int mrp_extensible_check(void *obj, uint32_t id);

/**
 * @brief Helper macro to check if an object instance is of a given type.
 *
 * You can use this macro to check an object is of a given type.
 *
 * @param [in] _obj   object instance to check
 * @param [in] _type  expected object type
 *
 * @return Evaluates to @true is the type check succeeds, @false otherwise.
 */
#define mrp_extensible_of_type(_obj, _type)                             \
    _mrp_extensible_of_type((_obj), #_type)

/**
 * @brief Function to check if an object is of a given named type.
 *
 * Use this macro to check if an object has the given named type.
 *
 * @param [in] obj   object instance to check
 * @param {in] type  name of the expected object type
 *
 * @return Evaluates to @true if the type check succeeds, @false otheriwse.
 */
bool _mrp_extensible_of_type(void *obj, const char *type);

/**
 * @brief Function to register an extension for an extensible object type.
 *
 * Register for the given named object type the extension with the given
 * name, named type, and specified value-freeing function.
 *
 * @param [in] obj_type  name of the extensible type
 * @param [in] ext_type  name of the type of the extension to register
 * @param [in] ext_name  name of the extension to register
 * @param [in] free      function to free values of this extension, or @NULL
 *
 * @return Returns the unique @uint32_t extension identifier for the newly
 *         registere extension, or @MRP_EXTENSION_NONE upon failure.
*/
uint32_t mrp_extension_register(const char *obj_type, const char *ext_type,
                                const char *ext_name, mrp_extfree_t free);

/**
 * @brief Function to control runtime type checking for the given extension.
 *
 * Use this function of you want to enable or disable runtime type checking
 * of an object extension. By default type checking is enabled.
 *
 * @param [in] id      unique identifier of the extension
 * @param [in] enable  whether to enable or disable type checking
 *
 * @return Returns 0 upon success, -1 otherwise.
 */
int mrp_extension_typecheck(uint32_t id, bool enable);

/**
 * @brief Helper macro to set the value of an object extension.
 *
 * Use this macro to set the value of a registered object extension.
 *
 * @param [in] _obj    object instance to set the value for
 * @param [in] _ext    unique identifier of extension to set
 * @param [in] _type   type of value to set
 * @param [in] _value  value to set the extension to
 *
 * @return Evaluates to 0 upon success, -1 otherwise. Upon success, any
 *         previously set value is freed using the free callback registered
 *         for the extension.
 */
#define mrp_extension_set(_obj, _ext, _type, _value)                    \
    _mrp_extension_set((_obj), (_ext), #_type, (_value))

/**
 * @brief Function to set the value of an object extension.
 *
 * Low-level function used by @mrp_extension_set to set the value of an
 * extension.
 *
 * @param [in] obj   object instance
 * @param [in] ext   unique extension identifier
 * @param [in] type  name of the type of the value being set
 * @param [in] value pointer to the value being set
 *
 * @return Return 0 upon success, -1 otherwise. Upon success, any previously
 *         set value for the extension is freed using the free callback
 *          registered for the extension.
 */
int _mrp_extension_set(void *obj, uint32_t ext, const char *type, void *value);

/**
 * @brief Helper macro used to get the value of an object extension.
 *
 * Use this macr to retrieve the value previously set for an object extension.
 *
 * @param [in] _obj  object instance to fetch the value from
 * @param [in] _type expected value type
 * @param [in] _ext  unique identifier of extension to get the value of
 *
 * @return Evaluates to the value previously set for the extension.
 */
#define mrp_extension_get(_obj, _type, _ext)                            \
    (_type)_mrp_extension_get((_obj), (_ext), #_type)

/**
 * @brief Function to get the value of an object extension.
 *
 * Low-level function used by @mrp_extension_get to retrive the value set for
 * an object extension.
 *
 * @param [in] obj   object instance to fetch the value from
 * @param [in] ext   unique extension identifier to fetch the value of
 * @param [in] type  name of the expected type of the value
 *
 * @return Returns the pointer to the value previously set by
 *         @_mrp_extension_set.
 */
void *_mrp_extension_get(void *obj, uint32_t ext, const char *type);

/**
 * @brief Free the value of a given object extension.
 *
 * Use this function to explicitly free the value of an object extension.
 *
 * @param [in] obj  object instance to free value from
 * @param [in] ext  unique identifier of extension to free value of
 *
 */
void mrp_extension_free(void *obj, uint32_t ext);

/**
 * @brief Free the values of all extensions of an object.
 *
 * Use this function to free all set extension of an object.
 *
 * @param [in] obj  object instance to free values from
 * @param [in] id   unique type identifier for the type of obj
 */
void mrp_extension_free_all(void *obj, uint32_t id);

/** @} */

MRP_CDECL_END

#endif /* __MURPHY_OBJECT_H__ */
