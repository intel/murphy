/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
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

#ifndef __MURPHY_LUA_OBJECT_H__
#define __MURPHY_LUA_OBJECT_H__

#include <stdbool.h>

#include <lualib.h>
#include <lauxlib.h>

#include "murphy/common/list.h"
#include "murphy/core/lua-utils/lua-utils.h"

#define MRP_LUA_CONFIG_ENVVAR "__MURPHY_LUA_CONFIG"

#define MRP_LUA_ENTER                           \
    mrp_debug("enter")

#define MRP_LUA_LEAVE(_v)                       \
    do {                                        \
        mrp_debug("leave (%d)", (_v));          \
        return (_v);                            \
    } while (0)

#define MRP_LUA_LEAVE_NOARG                     \
    mrp_debug("leave")

#define MRP_LUA_LEAVE_ERROR(L, fmt, args...)    \
    luaL_error(L, fmt , ## args)

#define MRP_LUA_CLASSID_ROOT              "LuaBook."

#define MRP_LUA_CLASS(_name, _constr)     & _name ## _ ## _constr ## _class_def
#define MRP_LUA_CLASS_SIMPLE(_name)       & _name ## _class_def

#define MRP_LUA_METHOD_LIST(...)          { __VA_ARGS__  {NULL, NULL}}

#define MRP_LUA_METHOD(_name, _func)      { # _name, _func } ,
#define MRP_LUA_METHOD_CONSTRUCTOR(_func) { "new", _func } ,

#define MRP_LUA_OVERRIDE_GETFIELD(_func)  { "__index", _func } ,
#define MRP_LUA_OVERRIDE_SETFIELD(_func)  { "__newindex", _func } ,
#define MRP_LUA_OVERRIDE_CALL(_func)      { "__call", _func } ,
#define MRP_LUA_OVERRIDE_STRINGIFY(_func) { "__tostring", _func } ,
#define MRP_LUA_OVERRIDE_GETLENGTH(_func) { "__len", _func } ,

#define MRP_LUA_METHOD_LIST_TABLE(_name, ... ) \
    static luaL_reg _name[] = {                \
        __VA_ARGS__                            \
        { NULL, NULL }                         \
    }

#define MRP_LUA_CLASS_NAME(_name)        #_name
#define MRP_LUA_CLASS_ID(_name, _constr) MRP_LUA_CLASSID_ROOT#_name"_"#_constr
#define MRP_LUA_UDATA_ID(_name, _constr)                \
    MRP_LUA_CLASSID_ROOT#_name"."#_constr".userdata"

#define MRP_LUA_TYPE_ID(_class_def)   ((_class_def)->type_id)
#define MRP_LUA_TYPE_NAME(_class_def) ((_class_def)->type_name)

#define MRP_LUA_CLASS_DEF(_name, _constr, _type, _destr, _methods, _overrides)\
    static mrp_lua_classdef_t _name ## _ ## _constr ## _class_def = {         \
        .class_name    = MRP_LUA_CLASS_NAME(_name),                           \
        .class_id      = MRP_LUA_CLASS_ID(_name, _constr),                    \
        .constructor   = # _name "." # _constr,                               \
        .destructor    = _destr,                                              \
        .type_name     = #_type,                                              \
        .type_id       = -1,                                                  \
        .userdata_id   = MRP_LUA_UDATA_ID(_name, _constr),                    \
        .userdata_size = sizeof(_type),                                       \
        .methods       = _methods,                                            \
        .overrides     = _overrides,                                          \
        .members       = NULL,                                                \
        .nmember       = 0,                                                   \
        .natives       = NULL,                                                \
        .nnative       = 0,                                                   \
        .notify        = NULL,                                                \
        .flags         = 0,                                                   \
    }

#define MRP_LUA_CLASS_DEF_SIMPLE(_name, _type, _destr, _methods, _overrides) \
    static luaL_reg _name ## _class_methods[]   = _methods;             \
    static luaL_reg _name ## _class_overrides[] = _overrides;           \
                                                                        \
    static mrp_lua_classdef_t _name ## _class_def = {                   \
        .class_name    = MRP_LUA_CLASS_NAME(_name),                     \
        .class_id      = MRP_LUA_CLASS_ID(_name, _constr),              \
        .constructor   = # _name,                                       \
        .destructor    = _destr,                                        \
        .type_name     = #_type,                                        \
        .type_id       = -1,                                            \
        .userdata_id   = MRP_LUA_UDATA_ID(_name, _constr),              \
        .userdata_size = sizeof(_type),                                 \
        .methods       = _name ## _class_methods,                       \
        .overrides     = _name ## _class_overrides,                     \
        .members       = NULL,                                          \
        .nmember       = 0,                                             \
        .natives       = NULL,                                          \
        .nnative       = 0,                                             \
        .notify        = NULL,                                          \
        .flags         = 0,                                             \
    }

#define MRP_LUA_CLASS_DEF_FLAGS(_name, _constr, _type, _destr, _methods,      \
                                _overrides, _class_flags)                     \
    static mrp_lua_classdef_t _name ## _ ## _constr ## _class_def = {         \
        .class_name    = MRP_LUA_CLASS_NAME(_name),                           \
        .class_id      = MRP_LUA_CLASS_ID(_name, _constr),                    \
        .constructor   = # _name "." # _constr,                               \
        .destructor    = _destr,                                              \
        .type_name     = #_type,                                              \
        .type_id       = -1,                                                  \
        .userdata_id   = MRP_LUA_UDATA_ID(_name, _constr),                    \
        .userdata_size = sizeof(_type),                                       \
        .methods       = _methods,                                            \
        .overrides     = _overrides,                                          \
        .members       = NULL,                                                \
        .nmember       = 0,                                                   \
        .natives       = NULL,                                                \
        .nnative       = 0,                                                   \
        .notify        = NULL,                                                \
        .flags         = _class_flags,                                        \
    }


#define MRP_LUA_CLASS_DEF_SIMPLE_FLAGS(_name, _type, _destr, _methods,  \
                                       _overrides, _class_flags)        \
    static luaL_reg _name ## _class_methods[]   = _methods;             \
    static luaL_reg _name ## _class_overrides[] = _overrides;           \
                                                                        \
    static mrp_lua_classdef_t _name ## _class_def = {                   \
        .class_name    = MRP_LUA_CLASS_NAME(_name),                     \
        .class_id      = MRP_LUA_CLASS_ID(_name, _constr),              \
        .constructor   = # _name,                                       \
        .destructor    = _destr,                                        \
        .type_name     = #_type,                                        \
        .type_id       = -1,                                            \
        .userdata_id   = MRP_LUA_UDATA_ID(_name, _constr),              \
        .userdata_size = sizeof(_type),                                 \
        .methods       = _name ## _class_methods,                       \
        .overrides     = _name ## _class_overrides,                     \
        .members       = NULL,                                          \
        .nmember       = 0,                                             \
        .natives       = NULL,                                          \
        .nnative       = 0,                                             \
        .notify        = NULL,                                          \
        .flags         = _class_flags,                                  \
    }


#define MRP_LUA_FOREACH_FIELD(_L, _i, _n, _l)                           \
    for (lua_pushnil(_L);                                               \
                                                                        \
         !(_l = 0) && lua_next(_L, _i) &&                               \
         (_n = (lua_type(_L, -2) == LUA_TSTRING) ?                      \
          lua_tolstring(_L, -2, &_l) : ""       );                      \
                                                                        \
         lua_pop(_L, 1))


/** _i=loopcount, _t=table idx, _type=key type, _n=field, _l=len or idx */
#define MRP_LUA_FOREACH_ALL(_L, _i, _t, _type, _n, _l)          \
    for (lua_pushnil(_L), _i = 0;                               \
                                                                \
    (lua_next(_L, _t) &&                                        \
    (((_type = lua_type(_L, -2)) == LUA_TSTRING ?               \
      (_n = lua_tolstring(_L, -2, &_l)) : (_n = NULL)) ||       \
     ((_type                     == LUA_TNIL    ?               \
        !(_n = NULL, _l = 0) :                                  \
       (_type                    == LUA_TNUMBER ?               \
        (_n = NULL, _l = lua_tointeger(_L, -2)) :               \
        !(_n = NULL, _l = 0))                           ||      \
       _type != LUA_TSTRING))));                                \
                                                                \
         lua_pop(_L, 1), _i++)


enum mrp_lua_event_type_e {
    MRP_LUA_OBJECT_DESTRUCTION = 1,
};


/*
 * stringification (debugging and Lua __tostring)
 */

/**
 * tostring/stringification mode
 */
typedef enum {
    /* verbosity modifiers */
    MRP_LUA_TOSTR_LUA      = 0x000,      /* native Lua tostr */
    MRP_LUA_TOSTR_MINIMAL  = 0x010,      /* minimal useful output */
    MRP_LUA_TOSTR_COMPACT  = 0x020,      /* compact (sub-oneline) output */
    MRP_LUA_TOSTR_ONELINE  = 0x040,      /* all output that fits into a line */
    MRP_LUA_TOSTR_SHORT    = 0x080,      /* shortest multiline output */
    MRP_LUA_TOSTR_MEDIUM   = 0x100,      /* medium multiline output */
    MRP_LUA_TOSTR_FULL     = 0x200,      /* full object output */
    MRP_LUA_TOSTR_VERBOSE  = 0x400,      /* dump all data imaginable */
    MRP_LUA_TOSTR_MODEMASK = 0xff0,      /* dump mode mask */

    /* content modifiers */
    MRP_LUA_TOSTR_META    = 0x001,       /* show metadata (userdata_t) part */
    MRP_LUA_TOSTR_DATA    = 0x002,       /* show user-visible data */
    MRP_LUA_TOSTR_BOTH    = 0x003,       /* both meta- and user-visible data */
} mrp_lua_tostr_mode_t;

/** Default configuration. */
#define MRP_LUA_TOSTR_DEFAULT   (MRP_LUA_TOSTR_COMPACT | MRP_LUA_TOSTR_META)

/** Stack-dump configuration. */
#define MRP_LUA_TOSTR_STACKDUMP (MRP_LUA_TOSTR_MINIMAL | MRP_LUA_TOSTR_META)

/** Stack-dump configuration for errors. */
#define MRP_LUA_TOSTR_ERRORDUMP (MRP_LUA_TOSTR_ONELINE | MRP_LUA_TOSTR_BOTH)

/** Stack-dump confguration for object-tracking check dump. */
#define MRP_LUA_TOSTR_CHECKDUMP (MRP_LUA_TOSTR_ONELINE | MRP_LUA_TOSTR_BOTH)

/** Type of an object stringification handler. */
typedef ssize_t (*mrp_lua_tostr_t)(mrp_lua_tostr_mode_t mode, char *buf,
                                   size_t size, lua_State *L, void *data);

/** Dump the given object to the provided buffer. */
ssize_t mrp_lua_object_tostr(mrp_lua_tostr_mode_t mode, char *buf, size_t size,
                             lua_State *L, void *data);

/** Dump the object at the given stack location to the provided buffer. */
ssize_t mrp_lua_index_tostr(mrp_lua_tostr_mode_t mode, char *buf, size_t size,
                            lua_State *L, int index);

/*
 * pre-declared class members
 */

typedef struct mrp_lua_class_member_s mrp_lua_class_member_t;
typedef union  mrp_lua_value_u        mrp_lua_value_t;
typedef int  (*mrp_lua_setter_t)(void *data, lua_State *L, int member,
                                 mrp_lua_value_t *v);
typedef int  (*mrp_lua_getter_t)(void *data, lua_State *L, int member,
                                 mrp_lua_value_t *v);

/*
 * class member/extension flags
 */

typedef enum {
    MRP_LUA_CLASS_NOFLAGS    = 0x000,    /* empty flags */
    MRP_LUA_CLASS_EXTENSIBLE = 0x001,    /* class is user-extensible from Lua */
    MRP_LUA_CLASS_READONLY   = 0x002,    /* class or member is readonly */
    MRP_LUA_CLASS_NOTIFY     = 0x004,    /* notify when member is changed */
    MRP_LUA_CLASS_NOINIT     = 0x008,    /* don't initialize member */
    MRP_LUA_CLASS_NOOVERRIDE = 0x010,    /* don't override setters, getters */
    MRP_LUA_CLASS_RAWGETTER  = 0x020,    /* getter pushes to the stack */
    MRP_LUA_CLASS_RAWSETTER  = 0x040,    /* setter takes args from the stack */
    MRP_LUA_CLASS_USESTACK   = 0x080,    /* autobridged method uses the stack */
    MRP_LUA_CLASS_DYNAMIC    = 0x100,    /* allow dynamic GC for this class */
} mrp_lua_class_flag_t;

/*
 * getter/setter statuses and macros
 */

#define MRP_LUA_OK_       1              /* successfully get/set */
#define MRP_LUA_NOTFOUND  0              /* member not found */
#define MRP_LUA_FAIL     -1              /* failed to get/set member */

/*
 * supported class member types
 */

#define MRP_LUA_VBASE      (LUA_TTHREAD + 1)
#define MRP_LUA_VMAX       8192
#define MRP_LUA_VTYPE(idx) (MRP_LUA_VBASE + (idx))

typedef enum {
    MRP_LUA_NONE    = LUA_TNONE,         /* not a valid type */
    MRP_LUA_NULL    = LUA_TNIL,          /* don't use, nil */

    MRP_LUA_BOOLEAN = LUA_TBOOLEAN,      /* boolean member */
    MRP_LUA_STRING  = LUA_TSTRING,       /* string member */
    MRP_LUA_DOUBLE  = LUA_TNUMBER,       /* double member */
    MRP_LUA_FUNC    = LUA_TFUNCTION,     /* any Lua function member */
    MRP_LUA_INTEGER = MRP_LUA_VTYPE(0),  /* integer member */
    MRP_LUA_LFUNC   = MRP_LUA_VTYPE(1),  /* pure Lua function member */
    MRP_LUA_CFUNC   = MRP_LUA_VTYPE(2),  /* C function member */
    MRP_LUA_BFUNC   = MRP_LUA_VTYPE(3),  /* bridged function member */

    MRP_LUA_BOOLEAN_ARRAY = MRP_LUA_VTYPE(4),
    MRP_LUA_STRING_ARRAY  = MRP_LUA_VTYPE(5),
    MRP_LUA_INTEGER_ARRAY = MRP_LUA_VTYPE(6),
    MRP_LUA_DOUBLE_ARRAY  = MRP_LUA_VTYPE(7),

    MRP_LUA_ANY     = MRP_LUA_VTYPE(8),  /* member of any type */
    MRP_LUA_OBJECT  = MRP_LUA_VTYPE(9),  /* object member */
    /* dynamically registered types */
    MRP_LUA_MAX     = MRP_LUA_VTYPE(MRP_LUA_VMAX)
} mrp_lua_type_t;


/*
 * a generic class member value
 */

union mrp_lua_value_u {
    const char *str;                     /* string value */
    bool        bln;                     /* boolean value */
    int32_t     s32;                     /* integer value */
    double      dbl;                     /* double value */
    int         lfn;                     /* Lua function reference value */
    void       *bfn;                     /* bridged function value */
    int         any;                     /* Lua reference */
    struct {                             /* array value */
        void    **items;                 /* array items */
        union {
            size_t   *nitem64;           /* number of items */
            int      *nitem32;           /* number of items */
        };
    } array;
    struct {
        int       ref;                   /* object reference */
        void     *ptr;                   /* object pointer */
    } obj;
};


/*
 * a class member descriptor
 */

struct mrp_lua_class_member_s {
    char             *name;              /* member name */
    mrp_lua_type_t    type;              /* member type */
    size_t            offs;              /* offset within type buffer */
    size_t            size;              /* offset to size within type buffer */
    size_t            sizew;             /* width of size within type buffer */
    const char       *type_name;         /* object type name */
    mrp_lua_type_t    type_id;           /* object type id */
    mrp_lua_setter_t  setter;            /* setter if any */
    mrp_lua_getter_t  getter;            /* getter if any */
    int               flags;             /* member flags */
};



/**
 * Murphy Lua Object Infrastructure
 *
 * The Murphy Lua object infrastructure allow you to declare and define
 * the C implementation of a Lua class, along with a set of explicitly
 * and or implicitly defined class members and class methods.
 *
 * Implicitly-defined members and methods - direct full control
 *
 * With implicitly defined members and methods you have almost absolute
 * control over what members and methods, when and how your C object
 * backend exposes to the Lua runtime, with minimal intervention from
 * the Murphy object infrastructure.
 *
 * You can almost freely override any method of your Lua object. Ignoring
 * some Lua-intrinsic details about raw versus ordinary member lookup, in
 * practice overriding a method causes your corresponding C handler to be
 * invoked whenever the Lua runtime makes a call to a member you have
 * overridden in the Murphy Lua class definition of the object.
 *
 * A basic technique to gain almost full control of the behavior of your
 * objects exposed behavior is simply to override the getfield (Lua
 * __index) and setfield (Lua __newindex) meta-methods in the objects
 * class definition. This way whenever someone tries to fetch, or set
 * a specific member in your object you will be called backed to either
 * provide the associated data, for getfield, or take the provided data
 * and associate it with the supplied field name or index, for setfield.
 *
 * In your overridden C callbacks you take care of all the necessary
 * actions of pushing the values corresponfing to fetched fields/indices
 * and storing values associated with set fields or indices. You can do
 * value, object state, accessibility (read-write) checks or any other
 * check you see necessary and reject the operation by returning an error,
 * or throwing an exception to the Lua runtime.
 *
 * Explicitly-defined members and methods - less control, but less code
 *
 * With explicitly defined members and/or methods, you specify the
 * members and/or methods for your object class along with metainformation
 * such as read-write/readonly status, write/update notifications,
 * dedicated member-specific getter/setter callbacks, etc. and let the
 * infrastructure take care of the details of passing information between
 * your objects and the Lua interpreter. In the case of explicitly-
 * defined members/methods, the Murphy Lua object infrastructure still
 * offers a certain degree of flexibility about the details of how you
 * want to handle specific members/methods. You can configure these by
 * setting flags, each representing a certain option, on the member or
 * for some flags on the full class.
 *
 * To provide a good balance between maximum flexibility and maximum
 * control, the infrastructure allows you to mix and match pretty
 * explicit and implicit member/method control pretty freely. You can
 * declare parts of your members (typically the regularly behaving easy
 * ones) explicitly, while handle the hairy ones (or those that are
 * difficult or impossible to map to C by the metadata) with your own
 * overridden getfield and setfield handlers. Which approach you take is
 * fully up to you and you are very much encouraged to experiment with
 * both to understand the limitations vs. conveniences presented by
 * each in practical terms.
 *
 * As stated earlier, in pratice, you can select how much assistance you
 * want from the infra on a per-member basis. You can relatively freely
 * choose between fully automatic handling, where you do not need to do
 * much apart from providing the corret metadata describing your object,
 * and fully manual handling where you need to take care of almost all
 * details of setting and retrieval (setfield/getfield).
 *
 * Occasionally the infra has technical limitations and if you hit any of
 * these you have no choice but take care of the details yourself. Usually
 * such limitations should not exist, however, and the chosen level of
 * detail is mostly dictated by how complex rules or constrains a class
 * member needs to obey. For instance, scalar members (strings, numbers,
 * booleans, etc.) and functions without much restrictions you can let be
 * handled automatically. Members with semantic restrictions, such as a
 * restricted range of acceptable values, or with contextual dependencies,
 * for instance dependencies on the values of other class members, you need
 * to handle with a varying level of detail yourself.
 *
 * Even in the more complex cases, there are a few facilities offered by
 * the infrastructure which were designed to let you get along in as many
 * of the cases as possibly without having to write much extra code.
 *
 * Automatic Members (perhaps should be called automatic explicit members):
 *
 * 1. Automatic (maybe we should call them explicit) class members
 *    You can declare a member with its name, type, storage offset,
 *    and let the infra take care of setting and retrieving it.
 *
 * 2. Read-only Members
 *    You can mark members (or a full class) read-only. Read-only members
 *    can only be set from C. Trying to set a read-only member from Lua
 *    will raise a runtime exception.
 *
 * 3. Change Notifications
 *    You can request change notifications on a per-member basis. Whenever
 *    a member with notifications on is changed from Lua, a class-wide
 *    notification callback is invoked. You can check the newly set value
 *    of the member and take any actions necessary to reflect the updated
 *    value of the member. For instance, when setting a 'disabled' member
 *    to true, you might disable the normal actions you class instance is
 *    performing. Note that currently the notification is called back only
 *    after the member has been updated and the callback has no return
 *    value. IOW, there is no straightforward way to reject a change from
 *    the notification callback (although, you can restore the old value
 *    from a saved copy if the new one is not kosher and then raise a Lua
 *    exception). However, this is still subject to change and probably
 *    we'll change notification callbacks to receive both the old and new
 *    values and to return a accepted/rejected/I-ve-taken-care-of-it type
 *    of verdict.
 *
 * 4. Optional Getters and Setters
 *    You can set on a per-members basis a getter, a setter, or both. If
 *    a member has a getter it will be invoked instead of the common default
 *    getter when the member is read/retrieved. Similary, when a member has
 *    a setter this will be called instead of the default setter when the
 *    value of the member is set.
 *
 * 5. Native (Fully Manual) Members
 *    You can declare a set of members native (perhaps manual members would
 *    be a bit more descriptive term). These will always be handed to your
 *    getfield/setfield class methods so you need to take care of every
 *    detail of setting and retrieving these variables.
 *
 * 6. Extended Members
 *    You can mark your class extensible at will. If you do so, your users
 *    will be able to extend your class from Lua by simply setting other
 *    members than the ones you have declared for the class. This allows
 *    easy duck-typing and extensions to your class be implemented in a
 *    straightforward manner usually without complex layers of wrappers.
 *
 * Classes with Mixed Members
 *
 * You can quite freely mix and match automatic (both with and without
 * getters and/or setters), fully manual, and extended class members,
 * although the dominant usage tends to be to mix only automatic and
 * extended members and keep manual classes strictly as such.
 */

/** Macro to define a Murphy Lua class. */
#define MRP_LUA_DEFINE_CLASS(_name, _constr, _type, _destr,               \
                             _methods, _overrides, _members,              \
                             _blacklist, _notify, _tostring,              \
                             _bridges, _class_flags)                      \
    static mrp_lua_classdef_t _name ## _ ## _constr ## _class_def = {     \
        .class_name    = MRP_LUA_CLASS_NAME(_name),                       \
        .class_id      = MRP_LUA_CLASS_ID(_name, _constr),                \
        .constructor   = # _name "." # _constr,                           \
        .destructor    = _destr,                                          \
        .type_name     = #_type,                                          \
        .type_id       = -1,                                              \
        .userdata_id   = MRP_LUA_UDATA_ID(_name, _constr),                \
        .userdata_size = sizeof(_type),                                   \
        .methods       = _methods,                                        \
        .overrides     = _overrides,                                      \
        .members       = _members,                                        \
        .nmember       = _members == NULL ? 0 : MRP_ARRAY_SIZE(_members), \
        .natives       = _blacklist,                                      \
        .nnative       = _blacklist==NULL ? 0:MRP_ARRAY_SIZE(_blacklist), \
        .bridges       = _bridges,                                        \
        .nbridge       = _bridges == NULL ? 0 : MRP_ARRAY_SIZE(_bridges), \
        .notify        = _notify,                                         \
        .tostring      = _tostring,                                       \
        .flags         = _class_flags,                                    \
    }

/** Macro to declare the list of class members. */
#define MRP_LUA_MEMBER_LIST_TABLE(_name, ...) \
    static mrp_lua_class_member_t _name[] = { __VA_ARGS__ }

/**
 * Generic generic macro to declare an automatic member for a class.
 *
 * @param _type    member type
 * @param _name    member name in Lua
 * @param _offs    member offset with visible part of userdata (if relevant)
 * @param _set     optional member-specific setter
 * @param _get     optional member-specific getter
 * @param _flags   member flags, bitwise or of MRP_LUA_CLASS_READONLY,
 *                 MRP_LUA_CLASS_NOTIFY, and MRP_LUA_CLASS_NOINIT. Also
 *                 MRP_LUA_CLASS_NOFLAGS is available to denote the empty
 *                 set of flags.
 */
#define MRP_LUA_CLASS_MEMBER(_type, _name, _offs, _set, _get, _flags)   \
                                                                        \
        .name = _name,                                                  \
        .type = _type,                                                  \
        .offs = _offs,                                                  \
        .setter = _set,                                                 \
        .getter = _get,                                                 \
        .flags  = _flags,                                               \

/*
 * type-specific convenience macros
 */

/** Declare an automatic string member. */
#define MRP_LUA_CLASS_STRING(_name, _offs, _set, _get, _flags)        \
    {MRP_LUA_CLASS_MEMBER(MRP_LUA_STRING, _name, _offs, _set, _get, _flags)},

/** Declare an automatic (signed 32-bit) integer member. */
#define MRP_LUA_CLASS_INTEGER(_name, _offs, _set, _get, _flags)       \
    {MRP_LUA_CLASS_MEMBER(MRP_LUA_INTEGER, _name, _offs, _set, _get, _flags)},

/** Declare an automatic double-precision floating point member. */
#define MRP_LUA_CLASS_DOUBLE(_name, _offs, _set, _get, _flags)        \
    {MRP_LUA_CLASS_MEMBER(MRP_LUA_DOUBLE, _name, _offs, _set, _get, _flags)},

/** Declare an automatic boolean member. */
#define MRP_LUA_CLASS_BOOLEAN(_name, _offs, _set, _get, _flags)       \
    {MRP_LUA_CLASS_MEMBER(MRP_LUA_BOOLEAN, _name, _offs, _set, _get, _flags)},

/** Declare an automatic Lua function member. */
#define MRP_LUA_CLASS_LFUNC(_name, _offs, _set, _get, _flags)         \
    {MRP_LUA_CLASS_MEMBER(MRP_LUA_LFUNC, _name, _offs, _set, _get, _flags)},

/** Declare an automatic C function member. */
#define MRP_LUA_CLASS_CFUNC(_name, _offs, _set, _get, _flags)         \
    {MRP_LUA_CLASS_MEMBER(MRP_LUA_CFUNC, _name, _offs, _set, _get, _flags)},

/** Declare an automatic member with a value of any acceptable type. */
#define MRP_LUA_CLASS_ANY(_name, _offs, _set, _get, _flags)           \
    {MRP_LUA_CLASS_MEMBER(MRP_LUA_ANY, _name, _offs, _set, _get, _flags)},

/** Declare an automatic array and size member of the given type. */
#define MRP_LUA_CLASS_ARRAY(_name, _type, _ctype, _p, _n, _set, _get, _flags) \
    {MRP_LUA_CLASS_MEMBER(MRP_LUA_##_type##_ARRAY, _name,                     \
                          MRP_OFFSET(_ctype, _p), _set, _set, _flags)         \
            .size  = MRP_OFFSET(_ctype, _n),                                  \
            .sizew = sizeof(((_ctype *)NULL)->_n)    },

/** Declare an automatic object and reference member of the given type. */
#define MRP_LUA_CLASS_OBJECT(_name, _type, _poffs, _roffs, _set, _get, _flags) \
    {MRP_LUA_CLASS_MEMBER(MRP_LUA_OBJECT, _name, _poffs,  _set, _get,   \
                          _flags)                                       \
            .size = _roffs,                                             \
            .type_name = #_type,                                        \
            .type_id = -1        },


#include "murphy/core/lua-utils/funcbridge.h"

/*
 * a bridged class method descriptor
 */

typedef struct {
    char                   *name;        /* function name */
    const char             *signature;   /* function signature */
    union {
        mrp_funcbridge_cfunc_t  fc;      /* C function to bridge to */
        mrp_funcbridge_t       *fb;      /* function bridge */
    };
    int                     flags;       /* method flags */
} mrp_lua_class_bridge_t;


/** Macro to declare the list of bridged class methods. */
#define MRP_LUA_BRIDGE_LIST_TABLE(_name, ...) \
    static mrp_lua_class_bridge_t _name[] = { __VA_ARGS__ }

/**
 * Generic generic macro to declare a bridged method for a class.
 *
 * @param _name      member name in Lua for this method
 * @param _signature signature of this method (see funcbridge.h)
 * @param _fn        bridged function to be invoked for this method
 * @param _flags     member flags, currently either MRP_LUA_CLASS_NOFLAGS,
 *                   or MRP_LUA_CLASS_USESTACK.
 */

#define MRP_LUA_CLASS_BRIDGE(_method, _signature, _fn, _flags)          \
    {                                                                   \
        .name      = _method,                                           \
        .signature = _signature,                                        \
        .fc        = _fn,                                               \
        .flags     = _flags,                                            \
    }


#define MRP_LUA_CLASS_CHECKER(_type, _prefix, _class)                  \
    static _type *_prefix##_check(lua_State *L, int idx)               \
    {                                                                  \
        return (_type *)mrp_lua_check_object(L, _class, idx);          \
    } struct _prefix##_kludge_for_trailing_semicolon


typedef struct mrp_lua_classdef_s     mrp_lua_classdef_t;
typedef enum   mrp_lua_event_type_e   mrp_lua_event_type_t;

typedef void (*mrp_lua_class_notify_t)(void *data, lua_State *L, int member);


struct mrp_lua_classdef_s {
    const char   *class_name;
    const char   *class_id;
    const char   *constructor;
    void        (*destructor)(void *);
    const char   *type_name;
    int           type_id;
    const void   *type_meta;
    const char   *userdata_id;
    size_t        userdata_size;
    luaL_reg     *methods;
    luaL_reg     *overrides;

    mrp_lua_tostr_t tostring;            /* stringification handler */
    mrp_list_hook_t objects;             /* instances of this class */
    uint32_t        ncreated;            /* number of objects created */
    uint32_t        ndestroyed;          /* number of objects destroyed */
    int             nactive;             /* number of active objects */
    int             ndead;               /* nuber of dead objects */

    /* pre-declared members */
    mrp_lua_class_member_t  *members;    /* pre-declared members */
    int                      nmember;    /* number of pre-declared members */
    char                   **natives;    /* 'native' member names */
    int                      nnative;    /* number of native member names */
    mrp_lua_class_bridge_t  *bridges;    /* bridged methods */
    int                      nbridge;    /* number of bridged methods */
    int                      brmeta;     /* reference to bridging metatable */
    mrp_lua_class_flag_t     flags;      /* class member flags */
    mrp_lua_class_notify_t   notify;     /* member change notify callback */
    lua_CFunction            setfield;   /* overridden setfield, if any */
    lua_CFunction            getfield;   /* overridden getfield, if any */
};

int   mrp_lua_create_object_class(lua_State *L, mrp_lua_classdef_t *def);
void  mrp_lua_get_class_table(lua_State *L, mrp_lua_classdef_t *def);
void *mrp_lua_create_object(lua_State *L, mrp_lua_classdef_t *def,
                            const char *name, int);
void  mrp_lua_set_object_name(lua_State  *L, mrp_lua_classdef_t *def,
                              const char *name);
void mrp_lua_set_object_index(lua_State *L, mrp_lua_classdef_t *def, int idx);

void  mrp_lua_destroy_object(lua_State *L, const char *name,int, void *object);

int   mrp_lua_find_object(lua_State *L, mrp_lua_classdef_t *def,
                          const char *name);

void *mrp_lua_check_object(lua_State *L, mrp_lua_classdef_t *def, int argnum);
void *mrp_lua_to_object(lua_State *L, mrp_lua_classdef_t *def, int idx);
int   mrp_lua_push_object(lua_State *L, void *object);

mrp_lua_classdef_t *mrp_lua_get_object_classdef(void *);

/** Specify pre-declared object members. */
int mrp_lua_declare_members(mrp_lua_classdef_t *def, mrp_lua_class_flag_t flags,
                            mrp_lua_class_member_t *members, int nmember,
                            char **natives, int nnative,
                            mrp_lua_class_notify_t notify);
/** Store and return a reference the value at the given stack location. */
int mrp_lua_object_ref_value(void *object, lua_State *L, int idx);
/** Remove the given stored reference. */
void mrp_lua_object_unref_value(void *object, lua_State *L, int ref);
/** Retrieve and push to the stack the value for the fiven reference. */
int mrp_lua_object_deref_value(void *object, lua_State *L, int ref,
                               int pushnil);
/** Get a private reference for the given reference owned by the given object. */
int mrp_lua_object_getref(void *owner, void *object, lua_State *L, int ref);
/** Decreate the reference count of the given object reference. */
#define mrp_lua_object_putref(o, L, ref) mrp_lua_object_unref_value(o, L, ref)
/** Set a pre-declared object member. */
int mrp_lua_set_member(void *data, lua_State *L, char *err, size_t esize);
/** Get and push a pre-declared object member. */
int mrp_lua_get_member(void *data, lua_State *L, char *err, size_t esize);
/** Initialize pre-declared object members from table at the given index. */
int mrp_lua_init_members(void *data, lua_State *L, int idx,
                         char *err, size_t esize);
/** Attempt to an array at tidx of expected type, with *nitemp max items. */
int mrp_lua_object_collect_array(lua_State *L, int tidx, void **itemsp,
                                 size_t *nitemp, int *expected, int dup,
                                 char *e, size_t esize);
/** Free an array duplicated by mrp_lua_object_collect_array. */
void mrp_lua_object_free_array(void **itemsp, size_t *nitemp, int type);
/** Get the class type id for the given class name. */
mrp_lua_type_t mrp_lua_class_name_type(const char *class_name);
/** Get the class type id for the given class id. */
mrp_lua_type_t mrp_lua_class_id_type(const char *class_id);
/** Get the class type id for the given type name. */
mrp_lua_type_t mrp_lua_class_type(const char *type_name);
/** Check if the object at the given stack index is of the given type. */
int mrp_lua_object_of_type(lua_State *L, int idx, mrp_lua_type_t type);
/** Check if the given (known to be murphy Lua) object is of given type. */
int mrp_lua_pointer_of_type(void *data, mrp_lua_type_t type);
/** Enable/disable per-class Murphy Lua object tracking. */
void mrp_lua_track_objects(bool enable);
/** Dump all active murphy Lua objects. */
void mrp_lua_dump_objects(mrp_lua_tostr_mode_t mode, lua_State *L, FILE *fp);

#endif  /* __MURPHY_LUA_OBJECT_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
