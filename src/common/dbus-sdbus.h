/*
 * Copyright (c) 2012, 2013, Intel Corporation
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

#ifndef __MURPHY_SD_BUS_H__
#define __MURPHY_SD_BUS_H__

#include <systemd/sd-bus.h>
#include <systemd/sd-bus-protocol.h>

#include <murphy/common/mainloop.h>
#include <murphy/common/dbus-error.h>

/** Type for a D-Bus (connection). */
struct mrp_dbus_s;
typedef struct mrp_dbus_s mrp_dbus_t;

/** Type for a D-Bus message. */
typedef struct mrp_dbus_msg_s mrp_dbus_msg_t;

/** Type for a D-Bus error. */
typedef sd_bus_error mrp_dbus_err_t;

/** D-BUS method or signal callback type. */
typedef int (*mrp_dbus_handler_t)(mrp_dbus_t *, mrp_dbus_msg_t *, void *);

/** Create a new connection to the given bus. */
mrp_dbus_t *mrp_dbus_connect(mrp_mainloop_t *ml, const char *address,
                             mrp_dbus_err_t *errp);
#define mrp_dbus_get mrp_dbus_connect

/** Set up an sd-bus instance with a mainloop. */
int mrp_dbus_setup_sd_bus(mrp_mainloop_t *ml, sd_bus *bus);

/** Increase the reference count of the given DBus (connection). */
mrp_dbus_t *mrp_dbus_ref(mrp_dbus_t *dbus);

/** Decrease the reference count of the given DBus (connection). */
int mrp_dbus_unref(mrp_dbus_t *dbus);

/** Acquire the given name on the given bus (connection). */
int mrp_dbus_acquire_name(mrp_dbus_t *dbus, const char *name,
                          mrp_dbus_err_t *error);

/** Release the given name on the given bus (connection). */
int mrp_dbus_release_name(mrp_dbus_t *dbus, const char *name,
                          mrp_dbus_err_t *error);

/** Type for a name tracking callback. */
typedef void (*mrp_dbus_name_cb_t)(mrp_dbus_t *, const char *, int,
                                   const char *, void *);
/** Start tracking the given name. */
int mrp_dbus_follow_name(mrp_dbus_t *dbus, const char *name,
                         mrp_dbus_name_cb_t cb, void *user_data);
/** Stop tracking the given name. */
int mrp_dbus_forget_name(mrp_dbus_t *dbus, const char *name,
                         mrp_dbus_name_cb_t cb, void *user_data);

/** Export a method to the bus. */
int mrp_dbus_export_method(mrp_dbus_t *dbus, const char *path,
                           const char *interface, const char *member,
                           mrp_dbus_handler_t handler, void *user_data);

/** Remove an exported method. */
int mrp_dbus_remove_method(mrp_dbus_t *dbus, const char *path,
                           const char *interface, const char *member,
                           mrp_dbus_handler_t handler, void *user_data);

/** Install a filter and add a handler for the given signal on the bus. */
MRP_NULLTERM int mrp_dbus_subscribe_signal(mrp_dbus_t *dbus,
                                           mrp_dbus_handler_t handler,
                                           void *user_data,
                                           const char *sender,
                                           const char *path,
                                           const char *interface,
                                           const char *member, ...);

/** Remove the signal handler and filter for the given signal on the bus. */
MRP_NULLTERM int mrp_dbus_unsubscribe_signal(mrp_dbus_t *dbus,
                                             mrp_dbus_handler_t handler,
                                             void *user_data,
                                             const char *sender,
                                             const char *path,
                                             const char *interface,
                                             const char *member, ...);

/** Install a filter for the given message on the bus. */
MRP_NULLTERM int mrp_dbus_install_filter(mrp_dbus_t *dbus,
                                         const char *sender,
                                         const char *path,
                                         const char *interface,
                                         const char *member, ...);

/** Install a filter for the given message on the bus. */
int mrp_dbus_install_filterv(mrp_dbus_t *dbus,
                             const char *sender,
                             const char *path,
                             const char *interface,
                             const char *member,
                             va_list ap);

/** Remove a filter for the given message on the bus. */
MRP_NULLTERM int mrp_dbus_remove_filter(mrp_dbus_t *dbus,
                                        const char *sender,
                                        const char *path,
                                        const char *interface,
                                        const char *member, ...);

/** Remove a filter for the given message on the bus. */
int mrp_dbus_remove_filterv(mrp_dbus_t *dbus,
                            const char *sender,
                            const char *path,
                            const char *interface,
                            const char *member,
                            va_list ap);

/** Add a signal handler for the gvien signal on the bus. */
int mrp_dbus_add_signal_handler(mrp_dbus_t *dbus, const char *sender,
                                const char *path, const char *interface,
                                const char *member, mrp_dbus_handler_t handler,
                                void *user_data);

/** Remove the given signal handler for the given signal on the bus. */
int mrp_dbus_del_signal_handler(mrp_dbus_t *dbus, const char *sender,
                                const char *path, const char *interface,
                                const char *member, mrp_dbus_handler_t handler,
                                void *user_data);

/** Type of a method call reply callback. */
typedef void (*mrp_dbus_reply_cb_t)(mrp_dbus_t *dbus, mrp_dbus_msg_t *reply,
                                    void *user_data);

/** Call the given method on the bus. */
int32_t mrp_dbus_call(mrp_dbus_t *dbus, const char *dest,
                      const char *path, const char *interface,
                      const char *member, int timeout,
                      mrp_dbus_reply_cb_t cb, void *user_data,
                      int dbus_type, ...);

/** Cancel an ongoing method call on the bus. */
int mrp_dbus_call_cancel(mrp_dbus_t *dbus, int32_t id);

/** Send a reply to the given method call on the bus. */
int mrp_dbus_reply(mrp_dbus_t *dbus, mrp_dbus_msg_t *msg, int type, ...);

/** Send an error reply to the given method call on the bus. */
int mrp_dbus_reply_error(mrp_dbus_t *dbus, mrp_dbus_msg_t *msg,
                         const char *errname, const char *errmsg,
                         int type, ...);

/** Emit the given signal on the bus. */
int mrp_dbus_signal(mrp_dbus_t *dbus, const char *dest, const char *path,
                    const char *interface, const char *member, int type, ...);

/** Send the given message on the bus. */
int mrp_dbus_send_msg(mrp_dbus_t *dbus, mrp_dbus_msg_t *msg);

/** Get our unique name on the bus. */
const char *mrp_dbus_get_unique_name(mrp_dbus_t *dbus);

/** Initialize the given error. */
static inline mrp_dbus_err_t *mrp_dbus_error_init(mrp_dbus_err_t *err)
{
    if (err != NULL)
        memset(err, 0, sizeof(*err));

    return err;
}


/** Set the given error buffer up with the error name and message. */
static inline mrp_dbus_err_t *mrp_dbus_error_set(mrp_dbus_err_t *err,
                                                 const char *name,
                                                 const char *message)
{
    sd_bus_error_set(err, name, "%s", message);

    return err;
}


/** Get the error message from the given bus error message. */
static inline const char *mrp_dbus_errmsg(mrp_dbus_err_t *err)
{
    if (err && sd_bus_error_is_set(err))
        return err->message;
    else
        return "unknown DBUS error";
}


/** Increase the reference count of a message. */
mrp_dbus_msg_t *mrp_dbus_msg_ref(mrp_dbus_msg_t *m);

/** Decrease the reference count of a message, freeing it if necessary. */
int mrp_dbus_msg_unref(mrp_dbus_msg_t *m);


/** Create a new method call message. */
mrp_dbus_msg_t *mrp_dbus_msg_method_call(mrp_dbus_t *bus,
                                         const char *destination,
                                         const char *path,
                                         const char *interface,
                                         const char *member);

/** Create a new method return message. */
mrp_dbus_msg_t *mrp_dbus_msg_method_return(mrp_dbus_t *bus,
                                           mrp_dbus_msg_t *msg);

/** Create a new error reply message. */
mrp_dbus_msg_t *mrp_dbus_msg_error(mrp_dbus_t *bus, mrp_dbus_msg_t *msg,
                                   mrp_dbus_err_t *err);

/** Create a new signal message. */
mrp_dbus_msg_t *mrp_dbus_msg_signal(mrp_dbus_t *bus,
                                    const char *destination,
                                    const char *path,
                                    const char *interface,
                                    const char *member);

/** Bus message types. */
typedef enum {
#ifndef SD_BUS_MESSAGE_TYPE_INVALID
#   define SD_BUS_MESSAGE_TYPE_INVALID _SD_BUS_MESSAGE_TYPE_INVALID
#endif
#   define MAP(t, f) MRP_DBUS_MESSAGE_TYPE_##t = SD_BUS_MESSAGE_TYPE_##f
    MAP(INVALID      , INVALID),
    MAP(METHOD_CALL  , METHOD_CALL),
    MAP(METHOD_RETURN, METHOD_RETURN),
    MAP(ERROR        , METHOD_ERROR),
    MAP(SIGNAL       , SIGNAL)
#   undef MAP
} mrp_dbus_msg_type_t;

/** Get the type of the given message. */
mrp_dbus_msg_type_t mrp_dbus_msg_type(mrp_dbus_msg_t *msg);

/** Message type checking convenience functions. */
#define TYPE_CHECK_FUNCTION(type, TYPE)                                 \
    static inline int mrp_dbus_msg_is_##type(mrp_dbus_msg_t *msg)       \
    {                                                                   \
        return mrp_dbus_msg_type(msg) == MRP_DBUS_MESSAGE_TYPE_##TYPE;  \
    }                                                                   \
    struct __mrp_dbus_allow_traling_semicolon

TYPE_CHECK_FUNCTION(method_call  , METHOD_CALL);
TYPE_CHECK_FUNCTION(method_return, METHOD_RETURN);
TYPE_CHECK_FUNCTION(error        , ERROR);
TYPE_CHECK_FUNCTION(signal       , SIGNAL);

/** Message argument types. */
typedef enum {
#ifndef SD_BUS_TYPE_INVALID
#   define SD_BUS_TYPE_INVALID _SD_BUS_TYPE_INVALID
#endif
#define TYPE(t) MRP_DBUS_TYPE_##t = SD_BUS_TYPE_##t
    TYPE(INVALID),
    TYPE(BYTE),
    TYPE(BOOLEAN),
    TYPE(INT16),
    TYPE(UINT16),
    TYPE(INT32),
    TYPE(UINT32),
    TYPE(INT64),
    TYPE(UINT64),
    TYPE(DOUBLE),
    TYPE(STRING),
    TYPE(OBJECT_PATH),
    TYPE(SIGNATURE),
    TYPE(UNIX_FD),
    TYPE(ARRAY),
    TYPE(VARIANT),
    TYPE(STRUCT),
    TYPE(DICT_ENTRY),
    TYPE(STRUCT_BEGIN),
    TYPE(STRUCT_END),
    TYPE(DICT_ENTRY_BEGIN),
    TYPE(DICT_ENTRY_END)
#undef TYPE
} mrp_dbus_type_t;

/** Message argument types as strings. */
static const char _type_as_string[][2] = {
#define MAP(_type) [SD_BUS_TYPE_##_type] = { SD_BUS_TYPE_##_type, '\0' }
    MAP(BYTE),
    MAP(BOOLEAN),
    MAP(INT16),
    MAP(UINT16),
    MAP(INT32),
    MAP(UINT32),
    MAP(INT64),
    MAP(UINT64),
    MAP(DOUBLE),
    MAP(STRING),
    MAP(OBJECT_PATH),
    MAP(SIGNATURE),
    MAP(UNIX_FD),
    MAP(ARRAY),
    MAP(VARIANT),
    MAP(STRUCT),
    MAP(DICT_ENTRY),
    MAP(STRUCT_BEGIN),
    MAP(STRUCT_END),
    MAP(DICT_ENTRY_BEGIN),
    MAP(DICT_ENTRY_END)
#undef MAP
};

#define _STRTYPE(_type) _type_as_string[SD_BUS_TYPE_##_type]
#define _EVAL(_type)    _type
#define MRP_DBUS_TYPE_BYTE_AS_STRING        _EVAL(_STRTYPE(BYTE))
#define MRP_DBUS_TYPE_BOOLEAN_AS_STRING     _EVAL(_STRTYPE(BOOLEAN))
#define MRP_DBUS_TYPE_INT16_AS_STRING       _EVAL(_STRTYPE(INT16))
#define MRP_DBUS_TYPE_UINT16_AS_STRING      _EVAL(_STRTYPE(UINT16))
#define MRP_DBUS_TYPE_INT32_AS_STRING       _EVAL(_STRTYPE(INT32))
#define MRP_DBUS_TYPE_UINT32_AS_STRING      _EVAL(_STRTYPE(UINT32))
#define MRP_DBUS_TYPE_INT64_AS_STRING       _EVAL(_STRTYPE(INT64))
#define MRP_DBUS_TYPE_UINT64_AS_STRING      _EVAL(_STRTYPE(UINT64))
#define MRP_DBUS_TYPE_DOUBLE_AS_STRING      _EVAL(_STRTYPE(DOUBLE))
#define MRP_DBUS_TYPE_STRING_AS_STRING      _EVAL(_STRTYPE(STRING))
#define MRP_DBUS_TYPE_OBJECT_PATH_AS_STRING _EVAL(_STRTYPE(OBJECT_PATH))
#define MRP_DBUS_TYPE_SIGNATURE_AS_STRING   _EVAL(_STRTYPE(SIGNATURE))
#define MRP_DBUS_TYPE_UNIX_FD_AS_STRING     _EVAL(_STRTYPE(UNIX_FD))
#define MRP_DBUS_TYPE_ARRAY_AS_STRING       _EVAL(_STRTYPE(ARRAY))
#define MRP_DBUS_TYPE_VARIANT_AS_STRING     _EVAL(_STRTYPE(VARIANT))
#define MRP_DBUS_TYPE_STRUCT_AS_STRING      _EVAL(_STRTYPE(STRUCT))
#define MRP_DBUS_TYPE_DICT_ENTRY_AS_STRING  _EVAL(_STRTYPE(DICT_ENTRY))

/** Get the path of the given message. */
const char *mrp_dbus_msg_path(mrp_dbus_msg_t *msg);

/** Get the interface of the given message. */
const char *mrp_dbus_msg_interface(mrp_dbus_msg_t *msg);

/** Get the member of the given message. */
const char *mrp_dbus_msg_member(mrp_dbus_msg_t *msg);

/** Get the destination of the given message. */
const char *mrp_dbus_msg_destination(mrp_dbus_msg_t *msg);

/** Get the sender of the given message. */
const char *mrp_dbus_msg_sender(mrp_dbus_msg_t *msg);

/** Open a new container of the given type and cotained types. */
int mrp_dbus_msg_open_container(mrp_dbus_msg_t *m, char type,
                                const char *contents);

/** Close the current container. */
int mrp_dbus_msg_close_container(mrp_dbus_msg_t *m);

/** Append an argument of a basic type to the given message. */
int mrp_dbus_msg_append_basic(mrp_dbus_msg_t *m, char type, void *valuep);

/** Get the type of the current message argument. */
mrp_dbus_type_t mrp_dbus_msg_arg_type(mrp_dbus_msg_t *m, const char **contents);

/** Open the current container (of the given type and contents) for reading. */
int mrp_dbus_msg_enter_container(mrp_dbus_msg_t *msg, char type,
                                 const char *contents);

/** Exit from the container being currently read. */
int mrp_dbus_msg_exit_container(mrp_dbus_msg_t *m);

/** Read the next argument (of basic type) from the given message. */
int mrp_dbus_msg_read_basic(mrp_dbus_msg_t *m, char type, void *valuep);

/** Read the next array of one of the basic types. */
int mrp_dbus_msg_read_array(mrp_dbus_msg_t *m, char type,
                            void **itemsp, size_t *nitemp);

/** Set up an sd_bus to be pumped by a murphy mainloop. */
int mrp_dbus_setup_with_mainloop(mrp_mainloop_t *ml, sd_bus *bus);
#endif /* __MURPHY_SD_BUS_H__ */
