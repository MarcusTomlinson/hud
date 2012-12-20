/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include <glib/gi18n.h>

#include "huditem.h"

#include "usage-tracker.h"
#include "hudtoken.h"

/**
 * SECTION:huditem
 * @title: HudItem
 * @short_description: a user-interesting item that can be activated
 *
 * A #HudItem represents a user-interesting action that can be activated
 * from the Hud user interface.
 **/

/**
 * HudItem:
 *
 * This is an opaque structure type.
 **/

/**
 * HudItemClass:
 * @parent_class: the #GObjectClass
 * @activate: virtual function pointer for hud_item_activate()
 *
 * This is the class vtable for #HudItem.
 **/

static GHashTable *hud_item_table;
static guint64 hud_item_next_id;

struct _HudItemPrivate
{
  GObject parent_instance;

  gchar *desktop_file;

  HudTokenList *token_list;
  HudStringList *tokens;
  gchar *usage_tag;
  gchar *app_icon;
  gchar *shortcut;
  gboolean enabled;
  guint usage;
  guint64 id;
};

G_DEFINE_TYPE (HudItem, hud_item, G_TYPE_OBJECT)

static void
hud_item_finalize (GObject *object)
{
  HudItem *item = HUD_ITEM (object);

  g_hash_table_remove (hud_item_table, &item->priv->id);
  hud_token_list_free (item->priv->token_list);
  hud_string_list_unref (item->priv->tokens);
  g_free (item->priv->desktop_file);
  g_free (item->priv->app_icon);
  g_free (item->priv->usage_tag);
  g_free (item->priv->shortcut);

  G_OBJECT_CLASS (hud_item_parent_class)
    ->finalize (object);
}

static void
hud_item_init (HudItem *item)
{
  item->priv = G_TYPE_INSTANCE_GET_PRIVATE (item, HUD_TYPE_ITEM, HudItemPrivate);
}

static void
hud_item_class_init (HudItemClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  hud_item_table = g_hash_table_new (g_int64_hash, g_int64_equal);

  gobject_class->finalize = hud_item_finalize;

  g_type_class_add_private (class, sizeof (HudItemPrivate));
}

static void
hud_item_format_tokens (GString       *string,
                        HudStringList *tokens)
{
  HudStringList *tail;

  tail = hud_string_list_get_tail (tokens);

  if (tail)
    {
      hud_item_format_tokens (string, tail);
      g_string_append (string, "||");
    }

  g_string_append (string, hud_string_list_get_head (tokens));
}

static void
hud_item_setup_usage (HudItem *item)
{
  GString *tag;

  if (item->priv->tokens && item->priv->enabled)
    {
      tag = g_string_new (NULL);
      hud_item_format_tokens (tag, item->priv->tokens);
      item->priv->usage_tag = g_string_free (tag, FALSE);
      item->priv->usage = usage_tracker_get_usage (usage_tracker_get_instance (),
                                                   item->priv->desktop_file, item->priv->usage_tag);
    }
}

/**
 * hud_item_construct:
 * @g_type: a #GType
 * @tokens: the search tokens for the item
 * @shortcut: Keyboard shortcut for the item
 * @desktop_file: the desktop file of the provider of the item
 * @app_icon: the icon name for the application that created this item
 * @enabled: if the item is enabled
 *
 * This is the Vala-style chain-up constructor corresponding to
 * hud_item_new().  @g_type must be a subtype of #HudItem.
 *
 * Only subclasses of #HudItem should call this.
 *
 * Returns: a new #HudItem or #HudItem subclass
 **/
gpointer
hud_item_construct (GType          g_type,
                    HudStringList *tokens,
                    const gchar   *shortcut,
                    const gchar   *desktop_file,
                    const gchar   *app_icon,
                    gboolean       enabled)
{
  HudItem *item;

  item = g_object_new (g_type, NULL);
  item->priv->tokens = hud_string_list_ref (tokens);
  item->priv->desktop_file = g_strdup (desktop_file);
  item->priv->app_icon = g_strdup (app_icon);
  item->priv->shortcut = g_strdup (shortcut);
  item->priv->enabled = enabled;
  item->priv->id = hud_item_next_id++;
  item->priv->token_list = hud_token_list_new_from_string_list (tokens);

  g_hash_table_insert (hud_item_table, &item->priv->id, item);

  if (desktop_file)
    hud_item_setup_usage (item);

  return item;
}

/**
 * hud_item_new:
 * @tokens: the search tokens for the item
 * @shortcut: Keyboard shortcut for the item
 * @desktop_file: the desktop file of the provider of the item
 * @app_icon: the icon name for the application that created this item
 * @enabled: if the item is enabled
 *
 * Creates a new #HudItem.
 *
 * If @enabled is %FALSE then the item will never be in the result of a
 * search.
 *
 * Returns: a new #HudItem
 **/
HudItem *
hud_item_new (HudStringList *tokens,
              const gchar   *shortcut,
              const gchar   *desktop_file,
              const gchar   *app_icon,
              gboolean       enabled)
{
  return hud_item_construct (HUD_TYPE_ITEM, tokens, shortcut, desktop_file, app_icon, enabled);
}

/**
 * hud_item_activate:
 * @item: a #HudItem
 * @platform_data: platform data
 *
 * Activates @item.
 *
 * @platform_data is platform data in the #GApplication or
 * #GRemoteActionGroup sense.  It should be a #GVariant with the type
 * <literal>a{sv}</literal>.
 **/
void
hud_item_activate (HudItem  *item,
                   GVariant *platform_data)
{
  g_return_if_fail (HUD_IS_ITEM (item));

  HUD_ITEM_GET_CLASS (item)
    ->activate (item, platform_data);

  if (item->priv->usage_tag)
    {
      usage_tracker_mark_usage (usage_tracker_get_instance (), item->priv->desktop_file, item->priv->usage_tag);
      item->priv->usage = usage_tracker_get_usage (usage_tracker_get_instance (),
                                                   item->priv->desktop_file, item->priv->usage_tag);
    }
}

/**
 * hud_item_get_tokens:
 * @item: a #HudItem
 *
 * Gets the tokens that represent the description of @item.
 *
 * This is a #HudStringList in reverse order of how the item should
 * appear in the Hud.  For example, "File > Open" would be represneted
 * by the list <code>['Open', 'File']</code>.
 *
 * Returns: (transfer none): the tokens
 **/
HudStringList *
hud_item_get_tokens (HudItem *item)
{
  g_return_val_if_fail (HUD_IS_ITEM (item), NULL);

  return item->priv->tokens;
}

/**
 * hud_item_get_item_icon:
 * @item: a #HudItem
 *
 * Gets the icon for the action represented by @item, if one exists.
 *
 * Returns: the icon name, or "" if there is no icon
 **/
const gchar *
hud_item_get_item_icon (HudItem *item)
{
  return "";
}

/**
 * hud_item_get_app_icon:
 * @item: a #HudItem
 *
 * Gets the icon of the application that @item lies within.
 *
 * Returns: the icon name, or "" if there is no icon
 **/
const gchar *
hud_item_get_app_icon (HudItem *item)
{
  return item->priv->app_icon ? item->priv->app_icon : "";
}

/**
 * hud_item_get_usage:
 * @item: a #HudItem
 *
 * Gets the use-count of @item.
 *
 * This is the number of times the item has been activated in recent
 * history.
 *
 * Returns: the usage count
 **/
guint
hud_item_get_usage (HudItem *item)
{
  return item->priv->usage;
}

/**
 * hud_item_get_enabled:
 * @item: a #HudItem
 *
 * Checks if the item is disabled or enabled.
 *
 * Disabled items should never appear in search results.
 *
 * Returns: if the item is enabled
 **/
gboolean
hud_item_get_enabled (HudItem *item)
{
  return item->priv->enabled;
}

/**
 * hud_item_get_id:
 * @item: a #HudItem
 *
 * Gets the unique identifier of this #HudItem.
 *
 * The identifier can be used to refetch the item using
 * hud_item_lookup() for as long as the item has not been destroyed.
 *
 * Returns: the ID of the item
 **/
guint64
hud_item_get_id (HudItem *item)
{
  return item->priv->id;
}

/**
 * hud_item_lookup:
 * @id: an item identifier
 *
 * Looks up a #HudItem by its ID.
 *
 * The ID for a #HudItem can be queried with hud_item_get_id().
 *
 * Returns: (transfer none): the #HudItem with the given @id, or %NULL
 *   if none exists
 **/
HudItem *
hud_item_lookup (guint64 id)
{
  return g_hash_table_lookup (hud_item_table, &id);
}

HudTokenList *
hud_item_get_token_list (HudItem *item)
{
  return item->priv->token_list;
}

/**
 * hud_item_get_command:
 * @item: a #HudItem
 *
 * Returns the user visible string that describes this command
 *
 * Returns: A string that can be shown to the user
 */
const gchar *
hud_item_get_command (HudItem *item)
{
	g_return_val_if_fail(HUD_IS_ITEM(item), NULL);

	if (item->priv->tokens == NULL) {
		return _("No Command");
	}

	const gchar * head = hud_string_list_get_head(item->priv->tokens);
	if (head == NULL) {
		return _("No Command");
	}

	return head;
}

/**
 * hud_item_get_context:
 * @item: a #HudItem
 *
 * Returns the context of this command as a string
 *
 * Returns: A string that can be shown to the user
 */
gchar *
hud_item_get_context (HudItem *item)
{
	g_return_val_if_fail(HUD_IS_ITEM(item), NULL);

	if (item->priv->tokens == NULL) {
		return "";
	}

	HudStringList * tail = hud_string_list_get_tail(item->priv->tokens);
	if (tail == NULL) {
		return "";
	}

	return hud_string_list_pretty_print(tail);
}

/**
 * hud_item_get_shortcut:
 * @item: a #HudItem
 *
 * Returns the shortcut for activating this command
 *
 * Returns: A string that can be shown to the user
 */
const gchar *
hud_item_get_shortcut (HudItem *item)
{
	g_return_val_if_fail(HUD_IS_ITEM(item), "");

	return item->priv->shortcut;
}
