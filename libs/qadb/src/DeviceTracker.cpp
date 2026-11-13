#include "qadb/DeviceTracker.h"

#include "Channel.h"
#include "qadb/Protocol.h"

#include <QHash>
#include <QTimer>

namespace qadb {

DeviceTracker::DeviceTracker(QObject* parent)
    : DeviceTracker(ServerAddress::fromEnvironment(), parent)
{
}

DeviceTracker::DeviceTracker(const ServerAddress& server, QObject* parent)
    : QObject(parent)
    , m_server(server)
    , m_retryTimer(new QTimer(this))
{
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &DeviceTracker::connectToServer);
}

DeviceTracker::~DeviceTracker() = default;

void DeviceTracker::setServer(const ServerAddress& server)
{
    if (m_server == server)
        return;
    m_server = server;
    if (m_running) {
        stop();
        start();
    }
}

void DeviceTracker::start()
{
    if (m_running)
        return;
    m_running = true;
    m_useLongFormat = true;
    m_reportedFailure = false;
    connectToServer();
}

void DeviceTracker::stop()
{
    if (!m_running)
        return;
    m_running = false;
    m_retryTimer->stop();
    if (m_channel) {
        m_channel->abort();
        m_channel->deleteLater();
    }
    setConnected(false);
}

void DeviceTracker::connectToServer()
{
    if (!m_running)
        return;
    if (m_channel) {
        m_channel->abort();
        m_channel->deleteLater();
    }

    auto* channel = new detail::Channel(m_server, this);
    m_channel = channel;
    connect(channel, &detail::Channel::failed, this,
            [this, channel](Reply::Error error, const QString& message) {
                if (channel != m_channel)
                    return;
                handleFailure(message, error == Reply::Error::ServerError);
            });
    channel->open([this, channel] {
        channel->sendRequest(m_useLongFormat ? QByteArrayLiteral("host:track-devices-l")
                                             : QByteArrayLiteral("host:track-devices"));
        channel->readStatus([this] {
            m_reportedFailure = false;
            setConnected(true);
            readNext();
        });
    });
}

void DeviceTracker::readNext()
{
    if (!m_channel)
        return;
    m_channel->readHexString([this](const QByteArray& payload) {
        handleList(payload);
        readNext();
    });
}

void DeviceTracker::handleList(const QByteArray& payload)
{
    const QList<DeviceInfo> devices = protocol::parseDeviceList(payload);

    QHash<QString, DeviceInfo> previous;
    for (const DeviceInfo& device : std::as_const(m_devices))
        previous.insert(device.serial, device);

    QList<DeviceInfo> added;
    QList<DeviceInfo> changed;
    for (const DeviceInfo& device : devices) {
        const auto it = previous.constFind(device.serial);
        if (it == previous.constEnd())
            added << device;
        else if (!(*it == device))
            changed << device;
        previous.remove(device.serial);
    }

    m_devices = devices;
    for (const DeviceInfo& device : std::as_const(previous))
        emit deviceRemoved(device);
    for (const DeviceInfo& device : std::as_const(added))
        emit deviceAdded(device);
    for (const DeviceInfo& device : std::as_const(changed))
        emit deviceChanged(device);
    emit devicesChanged(m_devices);
}

void DeviceTracker::handleFailure(const QString& message, bool serverRejected)
{
    if (m_channel) {
        m_channel->deleteLater();
        m_channel = nullptr;
    }
    // Servers older than track-devices-l reject it: fall back to the short list.
    if (serverRejected && m_useLongFormat && !m_connected) {
        m_useLongFormat = false;
        connectToServer();
        return;
    }
    // Disconnect first, so that a handler of errorOccurred sees isConnected() false and may start
    // the server.
    setConnected(false);
    // Report the first failure, not every retry while the server is down.
    if (!m_reportedFailure) {
        m_reportedFailure = true;
        emit errorOccurred(message);
    }
    if (m_running)
        m_retryTimer->start(m_retryInterval);
}

void DeviceTracker::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    if (!connected) {
        const QList<DeviceInfo> removed = std::exchange(m_devices, {});
        for (const DeviceInfo& device : removed)
            emit deviceRemoved(device);
        if (!removed.isEmpty())
            emit devicesChanged(m_devices);
    }
    emit connectedChanged(connected);
}

} // namespace qadb
