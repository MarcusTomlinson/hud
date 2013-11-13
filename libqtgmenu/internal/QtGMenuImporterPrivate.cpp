/*
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marcus Tomlinson <marcus.tomlinson@canonical.com>
 */

#include <QtGMenuImporterPrivate.h>

#include <QtGMenuConverter.h>
#include <QtGMenuUtils.h>

#include <QEventLoop>

using namespace qtgmenu;

// this is used to suppress compiler warnings about unused parameters
template< typename ... T > void unused( T&& ... )
{
}

QtGMenuImporterPrivate::QtGMenuImporterPrivate( const QString& service, const QString& path,
    QtGMenuImporter& parent )
    : QObject( 0 ),
      m_parent( parent ),
      m_connection( g_bus_get_sync( G_BUS_TYPE_SESSION, NULL, NULL ) ),
      m_service( service.toStdString() ),
      m_path( path.toStdString() ),
      m_gmenu_model( nullptr ),
      m_gaction_group( nullptr )
{
  connect( &m_menu_poll_timer, SIGNAL( timeout() ), this, SLOT( RefreshGMenuModel() ) );
  connect( &m_actions_poll_timer, SIGNAL( timeout() ), this, SLOT( RefreshGActionGroup() ) );
  StartPolling( 100 );
}

QtGMenuImporterPrivate::~QtGMenuImporterPrivate()
{
  m_menu_poll_timer.stop();
  m_actions_poll_timer.stop();

  g_object_unref( m_connection );

  ClearGMenuModel();
  ClearGActionGroup();
}

GMenuModel* QtGMenuImporterPrivate::GetGMenuModel()
{
  if( m_menu_poll_timer.isActive() || m_gmenu_model == nullptr )
  {
    return nullptr;
  }

  return m_gmenu_model->Model();
}

GActionGroup* QtGMenuImporterPrivate::GetGActionGroup()
{
  if( m_actions_poll_timer.isActive() || m_gaction_group == nullptr )
  {
    return nullptr;
  }

  return m_gaction_group;
}

std::vector< std::shared_ptr< QMenu > > QtGMenuImporterPrivate::GetQMenus()
{
  GMenuModel* gmenu = GetGMenuModel();

  if( gmenu == nullptr )
  {
    return std::vector< std::shared_ptr< QMenu > >();
  }

  std::vector< std::shared_ptr< QMenu > > qmenu = QtGMenuConverter::ToQMenus( gmenu );

  // return a copy of m_gmenu_model as a QMenu
  return qmenu;
}

void QtGMenuImporterPrivate::StartPolling( int interval )
{
  m_menu_poll_timer.stop();
  m_menu_poll_timer.setInterval( interval );
  m_menu_poll_timer.start();

  m_actions_poll_timer.stop();
  m_actions_poll_timer.setInterval( interval );
  m_actions_poll_timer.start();
}

void QtGMenuImporterPrivate::ActionAddedCallback( GActionGroup* action_group, gchar* action_name,
    gpointer user_data )
{
  unused( action_group );
  QtGMenuImporter* importer = reinterpret_cast< QtGMenuImporter* >( user_data );
  emit importer->ActionAdded( action_name );
}

void QtGMenuImporterPrivate::ActionRemovedCallback( GActionGroup* action_group, gchar* action_name,
    gpointer user_data )
{
  unused( action_group );
  QtGMenuImporter* importer = reinterpret_cast< QtGMenuImporter* >( user_data );
  emit importer->ActionRemoved( action_name );
}

void QtGMenuImporterPrivate::ActionEnabledCallback( GActionGroup* action_group, gchar* action_name,
    gboolean enabled, gpointer user_data )
{
  unused( action_group );
  QtGMenuImporter* importer = reinterpret_cast< QtGMenuImporter* >( user_data );
  emit importer->ActionEnabled( action_name, enabled );
}

void QtGMenuImporterPrivate::ActionStateChangedCallback( GActionGroup* action_group,
    gchar* action_name, GVariant* value, gpointer user_data )
{
  unused( action_group );
  QtGMenuImporter* importer = reinterpret_cast< QtGMenuImporter* >( user_data );
  emit importer->ActionStateChanged( action_name, QtGMenuUtils::GVariantToQVariant( value ) );
}

void QtGMenuImporterPrivate::ClearGMenuModel()
{
  if( m_gmenu_model == nullptr )
  {
    return;
  }

  if( m_gmenu_model->Size() > 0 )
  {
    // notify parent that items are being removed
    emit m_parent.MenuItemsChanged();
  }

  delete m_gmenu_model;
  m_gmenu_model = nullptr;
}

void QtGMenuImporterPrivate::ClearGActionGroup()
{
  if( m_gaction_group == nullptr )
  {
    return;
  }

  gchar** actions_list = g_action_group_list_actions( m_gaction_group );

  int action_index = 0;
  while( actions_list[action_index] != nullptr )
  {
    ActionRemovedCallback( m_gaction_group, actions_list[action_index], &m_parent );
    ++action_index;
  }

  g_strfreev( actions_list );

  g_signal_handler_disconnect( m_gaction_group, m_action_added_handler );
  g_signal_handler_disconnect( m_gaction_group, m_action_removed_handler );
  g_signal_handler_disconnect( m_gaction_group, m_action_enabled_handler );
  g_signal_handler_disconnect( m_gaction_group, m_action_state_changed_handler );

  m_action_added_handler = 0;
  m_action_removed_handler = 0;
  m_action_enabled_handler = 0;
  m_action_state_changed_handler = 0;

  g_object_unref( m_gaction_group );
  m_gaction_group = nullptr;
}

bool QtGMenuImporterPrivate::RefreshGMenuModel()
{
  bool menu_was_valid = m_gmenu_model != nullptr;

  // clear the menu model for the refresh
  ClearGMenuModel();

  m_gmenu_model = new QtGMenuModel( G_MENU_MODEL( g_dbus_menu_model_get( m_connection, m_service.c_str(), m_path.c_str()) ) );

  connect( m_gmenu_model, SIGNAL( MenuItemsChanged(QtGMenuModel*,int,int,int)), &m_parent, SIGNAL( MenuItemsChanged()) );

  gint item_count = m_gmenu_model->Size();

  if( item_count == 0 )
  {
    // block until "items-changed" fires or timeout reached
    QEventLoop menu_refresh_wait;
    QTimer timeout;

    menu_refresh_wait.connect( &m_parent, SIGNAL( MenuItemsChanged() ),
        SLOT( quit() ) );
    timeout.singleShot( 100, &menu_refresh_wait, SLOT( quit() ) );
    menu_refresh_wait.exec();

    // check item count again
    item_count = m_gmenu_model->Size();
  }

  bool menu_is_valid = item_count != 0;

  if( menu_is_valid )
  {
    // menu is valid, no need to continue polling
    m_menu_poll_timer.stop();
  }
  else if( !menu_is_valid )
  {
    // clear the menu model
    ClearGMenuModel();
  }

  if( !menu_was_valid && menu_is_valid )
  {
    emit m_parent.MenuAppeared();
  }
  else if( menu_was_valid && !menu_is_valid )
  {
    emit m_parent.MenuDisappeared();
  }

  return menu_is_valid;
}

bool QtGMenuImporterPrivate::RefreshGActionGroup()
{
  bool actions_were_valid = m_gaction_group != nullptr;

  // clear the action group for the refresh
  ClearGActionGroup();

  m_gaction_group =
      G_ACTION_GROUP( g_dbus_action_group_get( m_connection, m_service.c_str(), m_path.c_str()) );

  m_action_added_handler =
      g_signal_connect( m_gaction_group, "action-added", G_CALLBACK( ActionAddedCallback ), &m_parent );
  m_action_removed_handler =
      g_signal_connect( m_gaction_group, "action-removed", G_CALLBACK( ActionRemovedCallback ), &m_parent );
  m_action_enabled_handler =
      g_signal_connect( m_gaction_group, "action-enabled-changed", G_CALLBACK( ActionEnabledCallback ), &m_parent );
  m_action_state_changed_handler =
      g_signal_connect( m_gaction_group, "action-state-changed", G_CALLBACK( ActionStateChangedCallback ), &m_parent );

  gchar** actions_list = g_action_group_list_actions( m_gaction_group );

  if( actions_list[0] == nullptr )
  {
    // block until "items-changed" fires or timeout reached
    QEventLoop actions_refresh_wait;
    QTimer timeout;

    actions_refresh_wait.connect( &m_parent, SIGNAL( ActionAdded( QString ) ), SLOT( quit() ) );
    timeout.singleShot( 100, &actions_refresh_wait, SLOT( quit() ) );
    actions_refresh_wait.exec();

    // check item count again
    g_strfreev( actions_list );
    actions_list = g_action_group_list_actions( m_gaction_group );
  }

  bool actions_are_valid = actions_list[0] != nullptr;

  g_strfreev( actions_list );

  if( actions_are_valid )
  {
    // actions are valid, no need to continue polling
    m_actions_poll_timer.stop();
  }
  else if( !actions_are_valid )
  {
    // clear the action group
    ClearGActionGroup();
  }

  if( !actions_were_valid && actions_are_valid )
  {
    emit m_parent.ActionsAppeared();
  }
  else if( actions_were_valid && !actions_are_valid )
  {
    emit m_parent.ActionsDisappeared();
  }

  return actions_are_valid;
}

#include <QtGMenuImporterPrivate.moc>
