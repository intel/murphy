#ifndef __MURPHY_DBUS_H__
#define __MURPHY_DBUS_H__

#include <dbus/dbus.h>

#define MRP_AF_DBUS 0xDB

/** Our D-BUS (connection) abstraction. */
struct mrp_dbus_s;
typedef struct mrp_dbus_s mrp_dbus_t;

/** D-BUS method or signal callback type. */
typedef int (*mrp_dbus_handler_t)(mrp_dbus_t *, DBusMessage *, void *);

/** Create a new connection to the given bus. */
mrp_dbus_t *mrp_dbus_connect(mrp_mainloop_t *ml, const char *address,
			     DBusError *errp);

/** Set up a DBusConnection with a mainloop. */
int mrp_dbus_setup_connection(mrp_mainloop_t *ml, DBusConnection *conn);

/** Increase the reference count of the given DBus (connection). */
mrp_dbus_t *mrp_dbus_ref(mrp_dbus_t *dbus);

/** Decrease the reference count of the given DBus (connection). */
int mrp_dbus_unref(mrp_dbus_t *dbus);

/** Acquire the given name on the given bus (connection). */
int mrp_dbus_acquire_name(mrp_dbus_t *dbus, const char *name, DBusError *error);

/** Release the given name on the given bus (connection). */
int mrp_dbus_release_name(mrp_dbus_t *dbus, const char *name, DBusError *error);

typedef void (*mrp_dbus_name_cb_t)(mrp_dbus_t *, const char *, int,
				   const char *, void *);
int mrp_dbus_follow_name(mrp_dbus_t *dbus, const char *name,
			 mrp_dbus_name_cb_t cb, void *user_data);
int mrp_dbus_forget_name(mrp_dbus_t *dbus, const char *name,
			 mrp_dbus_name_cb_t cb, void *user_data);

int mrp_dbus_export_method(mrp_dbus_t *dbus, const char *path,
			   const char *interface, const char *member,
			   mrp_dbus_handler_t handler, void *user_data);

int mrp_dbus_remove_method(mrp_dbus_t *dbus, const char *path,
			   const char *interface, const char *member,
			   mrp_dbus_handler_t handler, void *user_data);

MRP_NULLTERM int mrp_dbus_subscribe_signal(mrp_dbus_t *dbus,
					   mrp_dbus_handler_t handler,
					   void *user_data,
					   const char *sender,
					   const char *path,
					   const char *interface,
					   const char *member, ...);

MRP_NULLTERM int mrp_dbus_unsubscribe_signal(mrp_dbus_t *dbus,
					     mrp_dbus_handler_t handler,
					     void *user_data,
					     const char *sender,
					     const char *path,
					     const char *interface,
					     const char *member, ...);

MRP_NULLTERM int mrp_dbus_install_filter(mrp_dbus_t *dbus,
					 const char *sender,
					 const char *path,
					 const char *interface,
					 const char *member, ...);
int mrp_dbus_install_filterv(mrp_dbus_t *dbus,
			     const char *sender,
			     const char *path,
			     const char *interface,
			     const char *member,
			     va_list ap);

MRP_NULLTERM int mrp_dbus_remove_filter(mrp_dbus_t *dbus,
					const char *sender,
					const char *path,
					const char *interface,
					const char *member, ...);

int mrp_dbus_remove_filterv(mrp_dbus_t *dbus,
			    const char *sender,
			    const char *path,
			    const char *interface,
			    const char *member,
			    va_list ap);

int mrp_dbus_add_signal_handler(mrp_dbus_t *dbus, const char *sender,
				const char *path, const char *interface,
				const char *member, mrp_dbus_handler_t handler,
				void *user_data);
int mrp_dbus_del_signal_handler(mrp_dbus_t *dbus, const char *sender,
				const char *path, const char *interface,
				const char *member, mrp_dbus_handler_t handler,
				void *user_data);

typedef void (*mrp_dbus_reply_cb_t)(mrp_dbus_t *dbus, DBusMessage *reply,
				    void *user_data);

int32_t mrp_dbus_call(mrp_dbus_t *dbus, const char *dest,
		      const char *path, const char *interface,
		      const char *member, int timeout,
		      mrp_dbus_reply_cb_t cb, void *user_data,
		      int dbus_type, ...);
int mrp_dbus_call_cancel(mrp_dbus_t *dbus, int32_t id);

int mrp_dbus_reply(mrp_dbus_t *dbus, DBusMessage *msg, int type, ...);

int mrp_dbus_signal(mrp_dbus_t *dbus, const char *dest, const char *path,
		    const char *interface, const char *member, int type, ...);

int32_t mrp_dbus_send(mrp_dbus_t *dbus, const char *dest, const char *path,
		      const char *interface, const char *member, int timeout,
		      mrp_dbus_reply_cb_t cb, void *user_data,
		      DBusMessage *msg);

int mrp_dbus_send_msg(mrp_dbus_t *dbus, DBusMessage *msg);

const char *mrp_dbus_get_unique_name(mrp_dbus_t *dbus);

static inline void mrp_dbus_error_init(DBusError *error)
{
    /*
     * Prevent libdbus error messages for NULL DBusError's...
     */
    if (error != NULL)
	dbus_error_init(error);
}


static inline const char *mrp_dbus_errmsg(DBusError *err)
{
    if (err && dbus_error_is_set(err))
	return err->message;
    else
	return "unknown DBUS error";
}


#endif /* __MURPHY_DBUS_H__ */
