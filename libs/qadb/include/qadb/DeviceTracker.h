#pragma once

#include "qadb/Types.h"

#include <QList>
#include <QObject>
#include <QPointer>

class QTimer;

namespace qadb {

namespace detail {
class Channel;
}

// Keeps a live list of devices with host:track-devices. The server sends a new list whenever a
// device connects, disconnects or changes state, so there is no polling.
//
// When the server is not running, or stops, the tracker retries every retryInterval() until it can
// connect again. Start the server with AdbServer::start() if it is not running.
class DeviceTracker : public QObject
{
    Q_OBJECT

public:
    explicit DeviceTracker(QObject* parent = nullptr);
    explicit DeviceTracker(const ServerAddress& server, QObject* parent = nullptr);
    ~DeviceTracker() override;

    ServerAddress server() const { return m_server; }
    void setServer(const ServerAddress& server);

    int retryInterval() const { return m_retryInterval; }
    void setRetryInterval(int msecs) { m_retryInterval = msecs; }

    bool isRunning() const { return m_running; }
    bool isConnected() const { return m_connected; }
    QList<DeviceInfo> devices() const { return m_devices; }

public slots:
    void start();
    void stop();

signals:
    // Emitted with the full list after every change, after the per-device signals.
    void devicesChanged(const QList<qadb::DeviceInfo>& devices);
    void deviceAdded(const qadb::DeviceInfo& device);
    void deviceRemoved(const qadb::DeviceInfo& device);
    // The state or another field of a device changed.
    void deviceChanged(const qadb::DeviceInfo& device);
    // Connected to the adb server or lost it. On disconnect, every device is reported removed.
    void connectedChanged(bool connected);
    void errorOccurred(const QString& message);

private:
    void connectToServer();
    void handleList(const QByteArray& payload);
    void handleFailure(const QString& message, bool serverRejected);
    void setConnected(bool connected);
    void readNext();

    ServerAddress m_server;
    int m_retryInterval = 2000;
    bool m_running = false;
    bool m_connected = false;
    bool m_useLongFormat = true;
    bool m_reportedFailure = false;
    QList<DeviceInfo> m_devices;
    QPointer<detail::Channel> m_channel;
    QTimer* m_retryTimer = nullptr;
};

} // namespace qadb
