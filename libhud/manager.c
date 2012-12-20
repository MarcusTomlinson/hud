/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of either or both of the following licences:
 *
 * 1) the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation; and/or
 * 2) the GNU Lesser General Public License version 2.1, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the applicable version of the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of both the GNU Lesser General Public
 * License version 3 and version 2.1 along with this program.  If not,
 * see <http://www.gnu.org/licenses/>
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "manager.h"
#include "action-publisher.h"
#include "service-iface.h"
#include "app-iface.h"

struct _HudManagerPrivate {
	gchar * application_id;

	GApplication * application;
	HudActionPublisher * app_pub;

	GCancellable * connection_cancel;
	GDBusConnection * session;
	_HudServiceComCanonicalHud * service_proxy;
	_HudAppIfaceComCanonicalHudApplication * app_proxy;

	GList * todo_add;
	GList * todo_remove;
	guint todo_idle;
};

#define HUD_MANAGER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), HUD_TYPE_MANAGER, HudManagerPrivate))

enum {
	PROP_0 = 0,
	PROP_APP_ID,
	PROP_APPLICATION,
};

static void hud_manager_class_init (HudManagerClass *klass);
static void hud_manager_init       (HudManager *self);
static void hud_manager_constructed (GObject *object);
static void hud_manager_dispose    (GObject *object);
static void hud_manager_finalize   (GObject *object);
static void set_property (GObject * obj, guint id, const GValue * value, GParamSpec * pspec);
static void get_property (GObject * obj, guint id, GValue * value, GParamSpec * pspec);
static void bus_get_cb (GObject * obj, GAsyncResult * res, gpointer user_data);

G_DEFINE_TYPE (HudManager, hud_manager, G_TYPE_OBJECT);

/* Initialize Class */
static void
hud_manager_class_init (HudManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (HudManagerPrivate));

	object_class->constructed = hud_manager_constructed;
	object_class->dispose = hud_manager_dispose;
	object_class->finalize = hud_manager_finalize;
	object_class->set_property = set_property;
	object_class->get_property = get_property;

	g_object_class_install_property (object_class, PROP_APP_ID,
	                                 g_param_spec_string(HUD_MANAGER_PROP_APP_ID, "ID for the application, typically the desktop file name",
	                                              "A unique identifier for the application.  Usually this aligns with the desktop file name or package name of the app.",
	                                              NULL,
	                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_APPLICATION,
	                                 g_param_spec_object(HUD_MANAGER_PROP_APPLICATION, "Instance of #GApplication if used for this application",
	                                              "GApplication object",
	                                              G_TYPE_APPLICATION,
	                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	return;
}

/* Initialize Instance */
static void
hud_manager_init (HudManager *self)
{
	self->priv->connection_cancel = g_cancellable_new();

	g_bus_get(G_BUS_TYPE_SESSION,
	          self->priv->connection_cancel,
	          bus_get_cb,
	          self);

	return;
}

/* Construct the object */
static void
hud_manager_constructed (GObject * object)
{
	HudManager * manager = HUD_MANAGER(object);

	if (manager->priv->application) {
		manager->priv->app_pub = hud_action_publisher_new_for_id(NULL);

		hud_action_publisher_add_action_group(manager->priv->app_pub, "app", NULL, g_application_get_dbus_object_path (manager->priv->application));
		hud_manager_add_actions(manager, manager->priv->app_pub);
	}

	return;
}

/* Clean up refs */
static void
hud_manager_dispose (GObject *object)
{
	HudManager * manager = HUD_MANAGER(object);

	if (manager->priv->connection_cancel) {
		g_cancellable_cancel(manager->priv->connection_cancel);
		g_clear_object(&manager->priv->connection_cancel);
	}

	g_list_free_full(manager->priv->todo_add, g_object_unref);
	manager->priv->todo_add = NULL;

	g_list_free_full(manager->priv->todo_remove, g_object_unref);
	manager->priv->todo_remove = NULL;

	if (manager->priv->todo_idle != 0) {
		g_source_remove(manager->priv->todo_idle);
		manager->priv->todo_idle = 0;
	}

	g_clear_object(&manager->priv->session);
	g_clear_object(&manager->priv->service_proxy);
	g_clear_object(&manager->priv->app_proxy);

	g_clear_object(&manager->priv->app_pub);
	g_clear_object(&manager->priv->application);

	G_OBJECT_CLASS (hud_manager_parent_class)->dispose (object);
	return;
}

/* Free Memory */
static void
hud_manager_finalize (GObject *object)
{
	HudManager * manager = HUD_MANAGER(object);

	g_clear_pointer(&manager->priv->application_id, g_free);

	G_OBJECT_CLASS (hud_manager_parent_class)->finalize (object);
	return;
}

/* Standard setting of props */
static void
set_property (GObject * obj, guint id, const GValue * value, GParamSpec * pspec)
{
	HudManager * manager = HUD_MANAGER(obj);

	switch (id) {
	case PROP_APP_ID:
		if (manager->priv->application == NULL) {
			g_clear_pointer(&manager->priv->application_id, g_free);
			manager->priv->application_id = g_value_dup_string(value);
		} else {
			g_warning("Application ID being set on HUD Manager already initialized with a GApplication");
		}
		break;
	case PROP_APPLICATION:
		g_clear_object(&manager->priv->application);
		g_clear_pointer(&manager->priv->application_id, g_free);

		manager->priv->application = g_value_dup_object(value);
		manager->priv->application_id = g_strdup(g_application_get_application_id(manager->priv->application));
		break;
	default:
		g_warning("Unknown property %d.", id);
		return;
	}

	return;
}

/* Standard getting of props */
static void
get_property (GObject * obj, guint id, GValue * value, GParamSpec * pspec)
{
	HudManager * manager = HUD_MANAGER(obj);

	switch (id) {
	case PROP_APP_ID:
		g_value_set_string(value, manager->priv->application_id);
		break;
	case PROP_APPLICATION:
		g_value_set_object(value, manager->priv->application);
		break;
	default:
		g_warning("Unknown property %d.", id);
		return;
	}

	return;
}

/* Take the todo queues and make them into DBus messages */
static void
process_todo_queues (HudManager * manager)
{
	/* TODO handle queues */

	return;
}

/* Application proxy callback */
static void
application_proxy_cb (GObject * obj, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;
	_HudAppIfaceComCanonicalHudApplication * proxy = _hud_app_iface_com_canonical_hud_application_proxy_new_finish(res, &error);

	if (error != NULL) {
		g_error("Unable to get app proxy: %s", error->message);
		g_error_free(error);
		return;
	}

	HudManager * manager = HUD_MANAGER(user_data);
	manager->priv->app_proxy = proxy;

	g_clear_object(&manager->priv->connection_cancel);

	process_todo_queues(manager);

	return;
}

/* Callback from getting the HUD service proxy */
static void
register_app_cb (GObject * obj, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;
	gchar * object = NULL;

	_hud_service_com_canonical_hud_call_register_application_finish ((_HudServiceComCanonicalHud *)obj,
		&object,
		res,
		&error);

	if (error != NULL) {
		g_error("Unable to register app: %s", error->message);
		g_error_free(error);
		return;
	}

	HudManager * manager = HUD_MANAGER(user_data);
	_hud_app_iface_com_canonical_hud_application_proxy_new(manager->priv->session,
		G_DBUS_PROXY_FLAGS_NONE,
		"com.canonical.hud",
		object,
		manager->priv->connection_cancel,
		application_proxy_cb,
		manager);

	g_free(object);

	return;
}

/* Callback from getting the HUD service proxy */
static void
service_proxy_cb (GObject * obj, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;
	_HudServiceComCanonicalHud * proxy = _hud_service_com_canonical_hud_proxy_new_finish(res, &error);

	if (error != NULL) {
		g_error("Unable to get session bus: %s", error->message);
		g_error_free(error);
		return;
	}

	HudManager * manager = HUD_MANAGER(user_data);
	manager->priv->service_proxy = proxy;

	_hud_service_com_canonical_hud_call_register_application(proxy,
		manager->priv->application_id,
		manager->priv->connection_cancel,
		register_app_cb,
		manager);

	return;
}

/* Callback from getting the session bus */
static void
bus_get_cb (GObject * obj, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;
	GDBusConnection * con = g_bus_get_finish(res, &error);

	if (error != NULL) {
		g_error("Unable to get session bus: %s", error->message);
		g_error_free(error);
		return;
	}

	HudManager * manager = HUD_MANAGER(user_data);
	manager->priv->session = con;

	_hud_service_com_canonical_hud_proxy_new(con,
	                                         G_DBUS_PROXY_FLAGS_NONE,
	                                         "com.canonical.hud",
	                                         "/com/canonical/hud",
	                                         manager->priv->connection_cancel,
	                                         service_proxy_cb,
	                                         manager);

	return;
}

/**
 * hud_manager_new:
 * @application_id: Unique identifier of the application, usually desktop file name
 *
 * Creates a new #HudManager object.
 *
 * Return value: (transfer full): New #HudManager
 */
HudManager *
hud_manager_new (const gchar * application_id)
{
	g_return_val_if_fail(application_id != NULL, NULL);

	return g_object_new(HUD_TYPE_MANAGER,
	                    HUD_MANAGER_PROP_APP_ID, application_id,
	                    NULL);
}

/**
 * hud_manager_new_for_application:
 * @application: #GApplication that we can use to get the ID and some actions from
 *
 * Creates a new #HudManager object using the application ID in th
 * @application object.  Also exports the default actions there in
 * the "app" namespace.
 *
 * Return value: (transfer full): New #HudManager
 */
HudManager *
hud_manager_new_for_application (GApplication * application)
{
	g_return_val_if_fail(G_IS_APPLICATION(application), NULL);

	return g_object_new(HUD_TYPE_MANAGER,
	                    HUD_MANAGER_PROP_APPLICATION, application,
	                    NULL);
}

/* Idle handler */
static gboolean
todo_handler (gpointer user_data)
{
	HudManager * manager = HUD_MANAGER(user_data);

	process_todo_queues(manager);

	manager->priv->todo_idle = 0;

	return FALSE;
}

/**
 * hud_manager_add_actions:
 * @manager: A #HudManager object
 * @pub: Action publisher object tracking the descriptions and action groups
 *
 * Sets up a set of actions and descriptions for a specific user
 * context.  This could be a window or a tab, depending on how the
 * application works.
 */
void
hud_manager_add_actions (HudManager * manager, HudActionPublisher * pub)
{
	g_return_if_fail(HUD_IS_MANAGER(manager));

	manager->priv->todo_add = g_list_append(manager->priv->todo_add, g_object_ref(pub));

	/* Should be if we're all set up */
	if (manager->priv->connection_cancel == NULL && manager->priv->todo_idle == 0) {
		manager->priv->todo_idle = g_idle_add(todo_handler, manager);
	}

	return;
}

/**
 * hud_manager_remove_actions:
 * @manager: A #HudManager object
 * @pub: Action publisher object tracking the descriptions and action groups
 *
 * Removes actions for being watched by the HUD.  Should be done when the object
 * is remove.  Does not require @pub to be a valid object so it can be used
 * with weak pointer style destroy.
 */
void
hud_manager_remove_actions (HudManager * manager, HudActionPublisher * pub)
{
	g_return_if_fail(HUD_IS_MANAGER(manager));

	manager->priv->todo_remove = g_list_append(manager->priv->todo_remove, g_object_ref(pub));

	/* Should be if we're all set up */
	if (manager->priv->connection_cancel == NULL && manager->priv->todo_idle == 0) {
		manager->priv->todo_idle = g_idle_add(todo_handler, manager);
	}

	return;
}
