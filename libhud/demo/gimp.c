#include "../hud.h"

#include <gtk/gtk.h>


typedef GtkApplicationClass GimpAppClass;
typedef GtkApplication GimpApp;

G_DEFINE_TYPE (GimpApp, gimp_app, GTK_TYPE_APPLICATION)

/* helpers... */
static void
set_state_cb (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  g_simple_action_set_state (action, parameter);
}

static void
toggle_cb (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  GVariant *old;

  old = g_action_get_state (G_ACTION (action));
  g_simple_action_set_state (action, g_variant_new_boolean (!g_variant_get_boolean (old)));
  g_variant_unref (old);
}

/* Blur is an operation with parameters but doesn't do a live update */
static void
gimp_app_apply_blur (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  HudOperation *operation = user_data;

  g_print ("Applying %s blur with radius %f\n",
           hud_operation_get_boolean (operation, "gaussian") ? "gaussian" : "simple",
           hud_operation_get_double (operation, "radius"));
}

static const GActionEntry blur_actions[] = {
  { "gaussian", toggle_cb,    NULL, "false", set_state_cb },
  { "radius",   set_state_cb, "d",  "0.0",   set_state_cb },
  { "apply",    gimp_app_apply_blur }
};

/* Colourify is an operation that does live updates */
static void
gimp_app_colourify_started (HudOperation *operation,
                            gpointer      user_data)
{
  g_print ("Starting colourify operation (at %d, %d, %d)...\n",
           hud_operation_get_int (operation, "red"),
           hud_operation_get_int (operation, "green"),
           hud_operation_get_int (operation, "blue"));
}

static void
gimp_app_colourify_ended (HudOperation *operation,
                          gpointer      user_data)
{
  g_print ("Ending colourify operation...\n");
}

static void
gimp_app_colourify_changed (HudOperation *operation,
                            const gchar  *action_name,
                            gpointer      user_data)
{
  g_print ("Colourify preview: '%s' changed, now (%d, %d, %d)\n", action_name,
           hud_operation_get_int (operation, "red"),
           hud_operation_get_int (operation, "green"),
           hud_operation_get_int (operation, "blue"));
}

static void
gimp_app_apply_colourify (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  HudOperation *operation = user_data;

  g_print ("Applying colourify: (%d, %d, %d)\n",
           hud_operation_get_int (operation, "red"),
           hud_operation_get_int (operation, "green"),
           hud_operation_get_int (operation, "blue"));
}

static const GActionEntry colourify_actions[] = {
  { "red",   set_state_cb, "i", "0", set_state_cb },
  { "green", set_state_cb, "i", "0", set_state_cb },
  { "blue",  set_state_cb, "i", "0", set_state_cb },
  { "apply", gimp_app_apply_colourify }
};

/* Simple quit action... */
static void
gimp_app_quit_action (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  GimpApp *ga = user_data;

  g_application_quit (G_APPLICATION (ga));
}

static const GActionEntry app_actions[] = {
  { "quit", gimp_app_quit_action }
};

static const HudActionEntry hud_entries[] = {
  { "blur", blur_actions, G_N_ELEMENTS (blur_actions) },

  { "colourify", colourify_actions, G_N_ELEMENTS (colourify_actions),
    .started  = gimp_app_colourify_started,
    .changed  = gimp_app_colourify_changed,
    .ended    = gimp_app_colourify_ended }
};

static void
gimp_app_startup (GApplication *app)
{
  HudActionPublisher *publisher;

  G_APPLICATION_CLASS (gimp_app_parent_class)
    ->startup (app);

  g_print ("Hello, my name is %s\n", g_dbus_connection_get_unique_name (g_application_get_dbus_connection (app)));

  publisher = hud_action_publisher_new_for_application (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app), app_actions, G_N_ELEMENTS (app_actions), app);
  hud_action_entries_install (G_ACTION_MAP (app), hud_entries, G_N_ELEMENTS (hud_entries), app);

  hud_action_publisher_add_descriptions_from_file (publisher, "gimp.xml");
}

static void
gimp_app_activate (GApplication *app)
{
  g_print ("Showing main window...\n");
  g_application_hold (app);
}

static void
gimp_app_init (GimpApp *ga)
{
}

static void
gimp_app_class_init (GimpAppClass *class)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (class);

  app_class->startup = gimp_app_startup;
  app_class->activate = gimp_app_activate;
}

int
main (int argc, char **argv)
{
  GApplication *app;
  gint status;

  g_type_init ();

  app = g_object_new (gimp_app_get_type (), "application-id", "com.example.gimp", NULL);
  status = g_application_run (app, argc, argv);
  g_object_unref (app);

  return status;
}
