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

#ifndef __HUD_ITEM_H__
#define __HUD_ITEM_H__

#include <glib-object.h>

#include "token.h"

#define HUD_TYPE_ITEM                                       (hud_item_get_type ())
#define HUD_ITEM(inst)                                      (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             HUD_TYPE_ITEM, HudItem))
#define HUD_ITEM_CLASS(class)                               (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             HUD_TYPE_ITEM, HudItemClass))
#define HUD_IS_ITEM(inst)                                   (G_TYPE_CHECK_INSTANCE_TYPE ((inst), HUD_TYPE_ITEM))
#define HUD_IS_ITEM_CLASS(class)                            (G_TYPE_CHECK_CLASS_TYPE ((class), HUD_TYPE_ITEM))
#define HUD_ITEM_GET_CLASS(inst)                            (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             HUD_TYPE_ITEM, HudItemClass))


typedef struct _HudItemPrivate                              HudItemPrivate;
typedef struct _HudItemClass                                HudItemClass;
typedef struct _HudItem                                     HudItem;

typedef struct _PronounceDict                               PronounceDict;
typedef struct _HudItemPronunciationData                    HudItemPronunciationData;

struct _HudItemClass
{
  GObjectClass parent_class;

  void (* activate) (HudItem  *item,
                     GVariant *platform_data);
};

struct _HudItem
{
  /*< private >*/
  GObject parent_instance;

  HudItemPrivate *priv;
};

struct _HudItemPronunciationData
{
  GHashTable *table;
  GRegex *regex;
  GPtrArray *list;
  PronounceDict *dict;
  GHashTable *unique_commands;
};

GType                   hud_item_get_type                               (void);

gpointer                hud_item_construct                              (GType          g_type,
                                                                         HudStringList *tokens,
                                                                         HudStringList *token_list,
                                                                         const gchar   *shortcut,
                                                                         const gchar   *app_id,
                                                                         const gchar   *app_icon,
                                                                         const gchar   *description,
                                                                         gboolean       enabled);
HudItem *               hud_item_new                                    (HudStringList *tokens,
                                                                         HudStringList *keywords,
                                                                         const gchar   *shortcut,
                                                                         const gchar   *app_id,
                                                                         const gchar   *app_icon,
                                                                         const gchar   *description,
                                                                         gboolean       enabled);
void                    hud_item_activate                               (HudItem       *item,
                                                                         GVariant      *platform_data);
void                    hud_item_mark_usage                             (HudItem       *item);
HudStringList *         hud_item_get_tokens                             (HudItem       *item);
HudStringList *         hud_item_get_keywords                             (HudItem       *item);
const gchar *           hud_item_get_app_id                             (HudItem       *item);
const gchar *           hud_item_get_app_icon                           (HudItem       *item);
const gchar *           hud_item_get_item_icon                          (HudItem       *item);
guint                   hud_item_get_usage                              (HudItem       *item);
gboolean                hud_item_get_enabled                            (HudItem       *item);
guint64                 hud_item_get_id                                 (HudItem       *item);
HudItem *               hud_item_lookup                                 (guint64        id);
HudTokenList *          hud_item_get_token_list                         (HudItem       *item);
const gchar *           hud_item_get_command                            (HudItem       *item);
const gchar *           hud_item_get_description                        (HudItem       *item);
const gchar *           hud_item_get_shortcut                           (HudItem       *item);
void                    hud_item_insert_pronounciation                  (HudItem       * item,
                                                                         HudItemPronunciationData    * user_data);

#endif /* __HUD_ITEM_H__ */