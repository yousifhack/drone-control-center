#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtQml/QQmlExtensionPlugin>

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"
#include "domain/Contracts.h"
#include "presentation/Foundation.h"

Q_IMPORT_QML_PLUGIN(YccFoundationQmlPlugin)

namespace {
// QGC objects stay on their owning UI thread. Only JSON copies cross to the core.
// No independent MAVLink socket and no parameter or actuator writes.
class QgcReadBridge : public QObject
{
public:
    explicit QgcReadBridge(QObject* parent) : QObject(parent)
    {
        auto* manager = MultiVehicleManager::instance();
        connect(manager, &MultiVehicleManager::activeVehicleChanged, this,
                [this](Vehicle* vehicle) { select(vehicle); });
        select(manager->activeVehicle());
        auto* timer = new QTimer(this);
        timer->setInterval(1000);
        connect(timer, &QTimer::timeout, this, [this] { publish(); });
        timer->start();
    }

private:
    void select(Vehicle* vehicle)
    {
        if (_vehicle)
            disconnect(_vehicle, nullptr, this, nullptr);
        if (_link)
            disconnect(_link, nullptr, this, nullptr);
        _vehicle = vehicle;
        _link = nullptr;
        _session = ycc::newId();
        if (vehicle) {
            _link = vehicle->vehicleLinkManager();
            connect(vehicle, &Vehicle::armedChanged, this, [this] { publish(); });
            connect(vehicle, &Vehicle::flightModeChanged, this, [this] { publish(); });
            connect(vehicle, &QObject::destroyed, this, [this] {
                _vehicle = nullptr;
                publish();
            });
            if (_link)
                connect(_link, &VehicleLinkManager::communicationLostChanged, this, [this] {
                    _session = ycc::newId();
                    publish();
                });
        }
        publish();
    }

    void publish()
    {
        QJsonObject context;
        if (_vehicle) {
            context = {{"sessionId", _session},
                       {"systemId", _vehicle->id()},
                       {"displayName", QString("Vehicle %1").arg(_vehicle->id())},
                       {"firmware", _vehicle->firmwareTypeString()},
                       {"armed", _vehicle->armed()},
                       {"flightMode", _vehicle->flightMode()},
                       {"linkLost", !_link || _link->communicationLost()},
                       {"observedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                       {"source", "QGC vehicle state; values may be retained during link loss"}};
        }
        ycc::Foundation::instance()->observeVehicle(context);
    }

    QPointer<Vehicle> _vehicle;
    QPointer<VehicleLinkManager> _link;
    QString _session;
};

void startBridge()
{
    QTimer::singleShot(0, QCoreApplication::instance(), [] { new QgcReadBridge(QCoreApplication::instance()); });
}
}  // namespace
Q_COREAPP_STARTUP_FUNCTION(startBridge)
