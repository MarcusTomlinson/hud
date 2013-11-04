#ifndef QTGMENUEXPORTER_H
#define QTGMENUEXPORTER_H

#include <QObject>
#include <QDBusConnection>

class QAction;
class QMenu;

namespace qtgmenu
{

class QtGMenuExporterPrivate;

class QtGMenuExporter final : public QObject
{
    Q_OBJECT

public:
    QtGMenuExporter(const QString& dbusObjectPath, QMenu* menu, const QDBusConnection& dbusConnection = QDBusConnection::sessionBus());
    ~QtGMenuExporter();

    void activateAction(QAction* action);
    void setStatus(const QString& status);
    QString status() const;

protected:
    virtual QString iconNameForAction(QAction* action);

private Q_SLOTS:
    void doUpdateActions();
    void doEmitLayoutUpdated();
    void slotActionDestroyed(QObject*);

private:
    Q_DISABLE_COPY(QtGMenuExporter)
    QtGMenuExporterPrivate* const d;

    friend class QtGMenuExporterPrivate;
    friend class QtGMenuExporterDBus;
    friend class QtGMenu;
};

} // namespace qtgmenu

#endif /* QTGMENUEXPORTER_H */
