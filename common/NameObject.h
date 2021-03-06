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
 * Author: Pete Woods <pete.woods@canonical.com>
 */

#ifndef HUD_COMMON_NAMEOBJECT_H_
#define HUD_COMMON_NAMEOBJECT_H_

#include <QObject>
#include <QDBusArgument>

namespace hud {
namespace common {

class Q_DECL_EXPORT NameObject {
public:
	explicit NameObject();

	explicit NameObject(const QString &name, const QDBusObjectPath &object);

	explicit NameObject(const NameObject &other);

	NameObject & operator=(const NameObject &other);

	bool operator==(const NameObject &other) const;

	virtual ~NameObject();

	static void registerMetaTypes();

	QString m_name;

	QDBusObjectPath m_object;
};

}
}

Q_DECL_EXPORT
QDBusArgument &operator<<(QDBusArgument &argument,
		const hud::common::NameObject &nameObject);

Q_DECL_EXPORT
const QDBusArgument &operator>>(const QDBusArgument &argument,
		hud::common::NameObject &nameObject);

Q_DECLARE_METATYPE(hud::common::NameObject)

#endif /* HUD_COMMON_NAMEOBJECT_H_ */
