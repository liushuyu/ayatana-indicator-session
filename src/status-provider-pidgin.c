/*
A small wrapper utility to load indicators and put them as menu items
into the gnome-panel using it's applet interface.

Copyright 2009 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "status-provider.h"
#include "status-provider-pidgin.h"

#include <dbus/dbus-glib.h>

typedef enum {
	PG_STATUS_UNKNOWN,
	PG_STATUS_OFFLINE,
	PG_STATUS_AVAILABLE,
	PG_STATUS_UNAVAILABLE,
	PG_STATUS_INVISIBLE,
	PG_STATUS_AWAY,
	PG_STATUS_EXTENDEND_AWAY,
	PG_STATUS_MOBILE,
	PG_STATUS_TUNE
} pg_status_t;

static const StatusProviderStatus pg_to_sp_map[] = {
	/* PG_STATUS_UNKNOWN,        */   STATUS_PROVIDER_STATUS_OFFLINE,
	/* PG_STATUS_OFFLINE,        */   STATUS_PROVIDER_STATUS_OFFLINE,
	/* PG_STATUS_AVAILABLE,      */   STATUS_PROVIDER_STATUS_ONLINE,
	/* PG_STATUS_UNAVAILABLE,    */   STATUS_PROVIDER_STATUS_DND,
	/* PG_STATUS_INVISIBLE,      */   STATUS_PROVIDER_STATUS_INVISIBLE,
	/* PG_STATUS_AWAY,           */   STATUS_PROVIDER_STATUS_AWAY,
	/* PG_STATUS_EXTENDEND_AWAY, */   STATUS_PROVIDER_STATUS_AWAY,
	/* PG_STATUS_MOBILE,         */   STATUS_PROVIDER_STATUS_OFFLINE,
	/* PG_STATUS_TUNE            */   STATUS_PROVIDER_STATUS_OFFLINE
};

static const pg_status_t sp_to_pg_map[STATUS_PROVIDER_STATUS_LAST] = {
	/* STATUS_PROVIDER_STATUS_ONLINE,  */  PG_STATUS_AVAILABLE,
	/* STATUS_PROVIDER_STATUS_AWAY,    */  PG_STATUS_AWAY,
	/* STATUS_PROVIDER_STATUS_DND      */  PG_STATUS_UNAVAILABLE,
	/* STATUS_PROVIDER_STATUS_INVISIBLE*/  PG_STATUS_INVISIBLE,
	/* STATUS_PROVIDER_STATUS_OFFLINE  */  PG_STATUS_OFFLINE
};

typedef struct _StatusProviderPidginPrivate StatusProviderPidginPrivate;
struct _StatusProviderPidginPrivate {
	DBusGProxy * proxy;
	pg_status_t  pg_status;
};

#define STATUS_PROVIDER_PIDGIN_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), STATUS_PROVIDER_PIDGIN_TYPE, StatusProviderPidginPrivate))

/* Prototypes */
/* GObject stuff */
static void status_provider_pidgin_class_init (StatusProviderPidginClass *klass);
static void status_provider_pidgin_init       (StatusProviderPidgin *self);
static void status_provider_pidgin_dispose    (GObject *object);
static void status_provider_pidgin_finalize   (GObject *object);
/* Internal Funcs */
static void set_status (StatusProvider * sp, StatusProviderStatus status);
static StatusProviderStatus get_status (StatusProvider * sp);

G_DEFINE_TYPE (StatusProviderPidgin, status_provider_pidgin, STATUS_PROVIDER_TYPE);

static void
status_provider_pidgin_class_init (StatusProviderPidginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (StatusProviderPidginPrivate));

	object_class->dispose = status_provider_pidgin_dispose;
	object_class->finalize = status_provider_pidgin_finalize;

	StatusProviderClass * spclass = STATUS_PROVIDER_CLASS(klass);

	spclass->set_status = set_status;
	spclass->get_status = get_status;

	return;
}

static void
changed_status (DBusGProxy * proxy, gint savedstatus, GError ** error, StatusProviderPidgin * spp)
{

	return;
}

static void
proxy_destroy (DBusGProxy * proxy, StatusProviderPidgin * spp)
{
	StatusProviderPidginPrivate * priv = STATUS_PROVIDER_PIDGIN_GET_PRIVATE(spp);

	priv->proxy = NULL;
	priv->pg_status = PG_STATUS_OFFLINE;

	g_signal_emit(G_OBJECT(spp), STATUS_PROVIDER_SIGNAL_STATUS_CHANGED_ID, 0, pg_to_sp_map[priv->pg_status], TRUE);
	return;
}

static void
status_provider_pidgin_init (StatusProviderPidgin *self)
{
	StatusProviderPidginPrivate * priv = STATUS_PROVIDER_PIDGIN_GET_PRIVATE(self);

	priv->proxy = NULL;
	priv->pg_status = PG_STATUS_OFFLINE;

	DBusGConnection * bus = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);
	g_return_if_fail(bus != NULL); /* Can't do anymore DBus stuff without this,
	                                  all non-DBus stuff should be done */

	GError * error = NULL;
	priv->proxy = dbus_g_proxy_new_for_name_owner (bus,
	                                               "im.pidgin.purple.PurpleService",
	                                               "/im/pidgin/purple/PurpleObject",
	                                               "im.pidgin.purple.PurpleInterface",
	                                               &error);
	if (error != NULL) {
		g_debug("Unable to get Pidgin proxy: %s", error->message);
		g_error_free(error);
		return;
	}

	if (priv->proxy != NULL) {
		g_object_add_weak_pointer (G_OBJECT(priv->proxy), (gpointer *)&priv->proxy);
		g_signal_connect(G_OBJECT(priv->proxy), "destroy",
		                 G_CALLBACK(proxy_destroy), self);

		g_debug("Adding Pidgin Signals");
		dbus_g_object_register_marshaller(_status_provider_pidgin_marshal_VOID__INT_INT,
		                            G_TYPE_NONE,
		                            G_TYPE_INT,
		                            G_TYPE_INT,
		                            G_TYPE_INVALID);
		dbus_g_proxy_add_signal    (priv->proxy,
		                            "SavedstatusChanged",
		                            G_TYPE_INT,
		                            G_TYPE_INT,
		                            G_TYPE_INVALID);
		dbus_g_proxy_connect_signal(priv->proxy,
		                            "SavedstatusChanged",
		                            G_CALLBACK(changed_status),
		                            (void *)self,
		                            NULL);
	}

	return;
}

static void
status_provider_pidgin_dispose (GObject *object)
{
	StatusProviderPidginPrivate * priv = STATUS_PROVIDER_PIDGIN_GET_PRIVATE(object);

	if (priv->proxy != NULL) {
		g_object_unref(priv->proxy);
		priv->proxy = NULL;
	}

	G_OBJECT_CLASS (status_provider_pidgin_parent_class)->dispose (object);
	return;
}

static void
status_provider_pidgin_finalize (GObject *object)
{

	G_OBJECT_CLASS (status_provider_pidgin_parent_class)->finalize (object);
	return;
}

/**
	status_provider_pidgin_new:

	Creates a new #StatusProviderPidgin object.  No parameters or anything
	like that.  Just a convience function.

	Return value: A new instance of #StatusProviderPidgin
*/
StatusProvider *
status_provider_pidgin_new (void)
{
	return STATUS_PROVIDER(g_object_new(STATUS_PROVIDER_PIDGIN_TYPE, NULL));
}

/* Takes the status provided generically for Status providers
   and turns it into a Pidgin status and sends it to Pidgin. */
static void
set_status (StatusProvider * sp, StatusProviderStatus status)
{
	gchar * message = "";

	g_return_if_fail(IS_STATUS_PROVIDER_PIDGIN(sp));
	StatusProviderPidginPrivate * priv = STATUS_PROVIDER_PIDGIN_GET_PRIVATE(sp);

	g_debug("\tPidgin set status to %d", status);
	if (priv->proxy == NULL) {
		return;
	}

	priv->pg_status = sp_to_pg_map[status];
	gint status_val = 0;
	gboolean ret = FALSE;
	GError * error = NULL;

	ret = dbus_g_proxy_call(priv->proxy,
	                        "PurpleSavedstatusFindTransientByTypeAndMessage", &error,
							G_TYPE_INT, priv->pg_status,
							G_TYPE_STRING, message,
	                        G_TYPE_INVALID,
	                        G_TYPE_INT, &status_val,
	                        G_TYPE_INVALID);

	if (!ret) {
		if (error != NULL) {
			g_error_free(error);
		}
		error = NULL;
		status_val = 0;
		g_debug("No Pidgin saved status to apply");
	}

	if (status_val == 0) {
		ret = dbus_g_proxy_call(priv->proxy,
								"PurpleSavedstatusNew", &error,
								G_TYPE_STRING, message,
								G_TYPE_INT, priv->pg_status,
								G_TYPE_INVALID,
								G_TYPE_INT, &status_val,
								G_TYPE_INVALID);

		if (!ret) {
			status_val = 0;
			if (error != NULL) {
				g_warning("Unable to create Pidgin status for %d: %s", status, error->message);
				g_error_free(error);
			} else {
				g_warning("Unable to create Pidgin status for %d", status);
			}
			error = NULL;
		}
	}

	if (status_val == 0) {
		return;
	}

	ret = dbus_g_proxy_call(priv->proxy,
	                        "PurpleSavedstatusActivate", &error,
	                        G_TYPE_INT, status_val,
	                        G_TYPE_INVALID,
	                        G_TYPE_INVALID);

	if (!ret) {
		if (error != NULL) {
			g_warning("Pidgin unable to change to status: %s", error->message);
			g_error_free(error);
		} else {
			g_warning("Pidgin unable to change to status");
		}
		error = NULL;
	}

	return;
}

/* Takes the cached Pidgin status and makes it into the generic
   Status provider status */
static StatusProviderStatus
get_status (StatusProvider * sp)
{
	g_return_val_if_fail(IS_STATUS_PROVIDER_PIDGIN(sp), STATUS_PROVIDER_STATUS_OFFLINE);
	StatusProviderPidginPrivate * priv = STATUS_PROVIDER_PIDGIN_GET_PRIVATE(sp);
	return pg_to_sp_map[priv->pg_status];
}
