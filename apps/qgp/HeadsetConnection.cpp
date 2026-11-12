#include "HeadsetConnection.h"

#include "qadb/AdbServer.h"
#include "qadb/Client.h"
#include "qadb/DeviceTracker.h"
#include "qadb/Reply.h"

#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QSettings>

#include <algorithm>

namespace {

const QString kSerialSetting = QStringLiteral("headset/serial");
const QString kNameSetting = QStringLiteral("headset/name");
const QString kHardwareSetting = QStringLiteral("headset/hardwareSerial");

// How long after pairing or connecting a new headset may take to show up and become current.
constexpr int kAdoptMsecs = 30000;

// A serial such as "192.168.1.20:5555", which the server forgets when it restarts.
bool isAddressSerial(const QString& serial)
{
    return serial.contains(QLatin1Char(':')) && !serial.contains(QLatin1String("._tcp"));
}

// Splits "host:port" (or "host", with defaultPort).
std::pair<QString, quint16> splitAddress(const QString& address, quint16 defaultPort)
{
    const qsizetype colon = address.lastIndexOf(QLatin1Char(':'));
    if (colon < 0)
        return {address, defaultPort};
    bool ok = false;
    const quint16 port = address.mid(colon + 1).toUShort(&ok);
    return {address.left(colon), ok ? port : defaultPort};
}

} // namespace

HeadsetConnection::HeadsetConnection(QObject* parent)
    : QObject(parent)
    , m_client(new qadb::Client(this))
    , m_tracker(new qadb::DeviceTracker(this))
{
    QSettings settings;
    m_serial = settings.value(kSerialSetting).toString();
    m_name = settings.value(kNameSetting).toString();
    m_hardwareSerial = settings.value(kHardwareSetting).toString();

    connect(m_tracker, &qadb::DeviceTracker::connectedChanged, this, [this](bool connected) {
        if (connected) {
            m_startTried = false;
            m_adbMissing = false;
            m_adbError.clear();
            m_reconnectPending = true;
        }
        emit statusChanged();
    });
    connect(m_tracker, &qadb::DeviceTracker::errorOccurred, this, [this] {
        if (!m_tracker->isConnected() && !m_startTried)
            startServer();
    });
    connect(m_tracker, &qadb::DeviceTracker::devicesChanged, this, &HeadsetConnection::trackerDevicesChanged);
}

void HeadsetConnection::start()
{
    m_tracker->start();
}

void HeadsetConnection::retryAdb()
{
    if (m_tracker->isConnected() || m_startReply)
        return;
    startServer();
}

void HeadsetConnection::startServer()
{
    m_startTried = true;
    const QString adb = qadb::AdbServer::findAdb();
    m_adbMissing = adb.isEmpty();
    if (m_adbMissing) {
        emit statusChanged();
        return;
    }

    m_adbError.clear();
    m_startReply = qadb::AdbServer::start(adb, m_tracker->server(), this);
    qadb::Reply* reply = m_startReply;
    connect(reply, &qadb::Reply::finished, this, [this, reply] {
        // On success the tracker connects on its next retry.
        if (!reply->isSuccess())
            m_adbError = reply->errorString();
        reply->deleteLater();
        emit statusChanged();
    });
    emit statusChanged();
}

bool HeadsetConnection::isAdbRunning() const
{
    return m_tracker->isConnected();
}

QList<qadb::DeviceInfo> HeadsetConnection::devices() const
{
    return m_tracker->devices();
}

std::optional<qadb::DeviceInfo> HeadsetConnection::currentDevice() const
{
    if (m_serial.isEmpty())
        return std::nullopt;
    for (const qadb::DeviceInfo& device : m_tracker->devices()) {
        if (device.serial == m_serial)
            return device;
    }
    return std::nullopt;
}

void HeadsetConnection::setCurrentSerial(const QString& serial)
{
    m_adoptUntil = QDeadlineTimer();
    m_serial = serial;
    if (const auto device = currentDevice())
        m_name = displayName(*device);
    else if (serial.isEmpty())
        m_name.clear();

    m_hardwareSerial = m_hardwareBySerial.value(serial);

    QSettings settings;
    settings.setValue(kSerialSetting, m_serial);
    settings.setValue(kNameSetting, m_name);
    settings.setValue(kHardwareSetting, m_hardwareSerial);
    lookUpHardwareSerials();
    emit statusChanged();
    emit devicesChanged();
}

HeadsetConnection::Status HeadsetConnection::status() const
{
    if (!m_tracker->isConnected()) {
        if (m_adbMissing)
            return Status::AdbMissing;
        if (!m_startReply && !m_adbError.isEmpty())
            return Status::AdbFailed;
        return Status::StartingAdb;
    }
    if (m_serial.isEmpty())
        return Status::NoHeadset;

    const auto device = currentDevice();
    if (!device)
        return Status::NotConnected;
    switch (device->state) {
    case qadb::DeviceState::Device:
        return Status::Ready;
    case qadb::DeviceState::Unauthorized:
    case qadb::DeviceState::NoPermissions:
        return Status::NeedsPermission;
    case qadb::DeviceState::Authorizing:
    case qadb::DeviceState::Connecting:
        return Status::Connecting;
    default:
        return Status::NotConnected;
    }
}

QString HeadsetConnection::statusText() const
{
    const QString name = m_name.isEmpty() ? tr("Headset") : m_name;
    switch (status()) {
    case Status::StartingAdb:
        return tr("Starting adb...");
    case Status::AdbMissing:
        return tr("adb not found");
    case Status::AdbFailed:
        return tr("adb did not start");
    case Status::NoHeadset:
        return tr("No headset");
    case Status::NotConnected:
        return tr("%1 not connected").arg(name);
    case Status::NeedsPermission:
        return tr("%1 waiting for permission").arg(name);
    case Status::Connecting:
        return tr("Connecting to %1...").arg(name);
    case Status::Ready:
        return tr("Connected to %1").arg(name);
    }
    return {};
}

QString HeadsetConnection::statusAdvice() const
{
    switch (status()) {
    case Status::StartingAdb:
    case Status::Ready:
    case Status::Connecting:
        return {};
    case Status::AdbMissing:
        return tr("Install the Android SDK Platform Tools, which include adb, and try again.");
    case Status::AdbFailed:
        return tr("adb could not start: %1").arg(m_adbError);
    case Status::NoHeadset:
        return tr("Connect the headset with a USB cable, or add it over Wi-Fi.");
    case Status::NotConnected:
        return tr("Connect the headset with a USB cable and turn it on.");
    case Status::NeedsPermission:
        return tr("Put on the headset and allow USB debugging.");
    }
    return {};
}

void HeadsetConnection::trackerDevicesChanged()
{
    adoptNewHeadset();

    // Remember the name of the current headset, for when it is not connected.
    if (const auto device = currentDevice()) {
        const QString name = displayName(*device);
        if (name != m_name) {
            m_name = name;
            QSettings().setValue(kNameSetting, m_name);
        }
    }

    if (m_reconnectPending) {
        m_reconnectPending = false;
        reconnectRemembered();
    }

    lookUpHardwareSerials();
    selectByHardwareSerial();

    emit devicesChanged();
    emit statusChanged();
}

// Makes a headset current when none is chosen and it is the only one online, or when it comes
// online shortly after the user paired or connected it.
void HeadsetConnection::adoptNewHeadset()
{
    QList<QString> online;
    for (const qadb::DeviceInfo& device : m_tracker->devices()) {
        if (device.isOnline())
            online << device.serial;
    }
    const QList<QString> before = std::exchange(m_onlineBefore, online);

    if (!m_adoptUntil.hasExpired()) {
        for (const QString& serial : std::as_const(online)) {
            if (!before.contains(serial) && serial != m_serial) {
                setCurrentSerial(serial);
                return;
            }
        }
    }
    if (m_serial.isEmpty() && online.size() == 1)
        setCurrentSerial(online.first());
}

void HeadsetConnection::lookUpHardwareSerials()
{
    QSet<QString> online;
    for (const qadb::DeviceInfo& device : m_tracker->devices()) {
        if (device.isOnline())
            online.insert(device.serial);
    }
    // Forget devices that went away: their address may come back as another headset.
    for (auto it = m_hardwareBySerial.begin(); it != m_hardwareBySerial.end();) {
        if (online.contains(it.key()))
            ++it;
        else
            it = m_hardwareBySerial.erase(it);
    }
    for (const QString& serial : std::as_const(online)) {
        if (m_hardwareBySerial.contains(serial) || m_hardwarePending.contains(serial))
            continue;
        m_hardwarePending.insert(serial);
        qadb::Reply* reply = m_client->getProperty(serial, QStringLiteral("ro.serialno"));
        connect(reply, &qadb::Reply::finished, this, [this, reply, serial] {
            reply->deleteLater();
            m_hardwarePending.remove(serial);
            const QString hardware = reply->outputText().trimmed();
            if (!reply->isSuccess() || hardware.isEmpty())
                return;
            m_hardwareBySerial.insert(serial, hardware);
            if (serial == m_serial && hardware != m_hardwareSerial) {
                m_hardwareSerial = hardware;
                QSettings().setValue(kHardwareSetting, m_hardwareSerial);
            }
            selectByHardwareSerial();
        });
    }
}

void HeadsetConnection::selectByHardwareSerial()
{
    if (m_hardwareSerial.isEmpty())
        return;
    if (const auto device = currentDevice(); device && device->isOnline())
        return;
    // Prefer a USB connection, then any other: the first one found in the device list.
    QString match;
    for (const qadb::DeviceInfo& device : m_tracker->devices()) {
        if (!device.isOnline() || m_hardwareBySerial.value(device.serial) != m_hardwareSerial)
            continue;
        if (match.isEmpty() || !isAddressSerial(device.serial))
            match = device.serial;
    }
    if (!match.isEmpty() && match != m_serial)
        setCurrentSerial(match);
}

// The server forgets network headsets when it restarts: connect the current one again.
void HeadsetConnection::reconnectRemembered()
{
    if (!isAddressSerial(m_serial) || currentDevice())
        return;
    const auto [host, port] = splitAddress(m_serial, 5555);
    qadb::Reply* reply = m_client->connectDevice(host, port);
    connect(reply, &qadb::Reply::finished, reply, &QObject::deleteLater);
}

void HeadsetConnection::scan()
{
    if (m_scanReply)
        return;
    m_scanReply = m_client->mdnsServices();
    qadb::MdnsReply* reply = static_cast<qadb::MdnsReply*>(m_scanReply.data());
    connect(reply, &qadb::Reply::finished, this, [this, reply] {
        reply->deleteLater();
        m_scanReply = nullptr;
        if (!reply->isSuccess()) {
            emit scanFinished(tr("Could not search for wireless headsets: %1").arg(reply->errorString()), {});
            return;
        }

        QString pairingAddress;
        int found = 0;
        const QList<qadb::DeviceInfo> known = m_tracker->devices();
        for (const qadb::MdnsService& service : reply->services()) {
            if (service.type.startsWith(QLatin1String("_adb-tls-pairing"))) {
                if (pairingAddress.isEmpty())
                    pairingAddress = service.address;
                continue;
            }
            if (!service.type.startsWith(QLatin1String("_adb-tls-connect")) &&
                !service.type.startsWith(QLatin1String("_adb._tcp")))
                continue;
            ++found;
            const bool connected = std::any_of(known.begin(), known.end(), [&](const qadb::DeviceInfo& device) {
                return device.serial == service.address || device.serial.startsWith(service.name);
            });
            if (connected)
                continue;
            const auto [host, port] = splitAddress(service.address, 5555);
            qadb::Reply* connectReply = m_client->connectDevice(host, port);
            connect(connectReply, &qadb::Reply::finished, connectReply, &QObject::deleteLater);
        }

        QString message;
        if (found > 0)
            message = found == 1 ? tr("Found 1 wireless headset.") : tr("Found %1 wireless headsets.").arg(found);
        else if (!pairingAddress.isEmpty())
            message = tr("Found a headset that is ready to pair. Enter its pairing code below.");
        else
            message = tr("No wireless headsets found.");
        emit scanFinished(message, pairingAddress);
    });
}

qadb::Reply* HeadsetConnection::addWirelessHeadset(const QString& address, const QString& code)
{
    m_adoptUntil.setRemainingTime(kAdoptMsecs);
    if (!code.isEmpty())
        return m_client->pair(address, code);
    const auto [host, port] = splitAddress(address, 5555);
    return m_client->connectDevice(host, port);
}

qadb::Reply* HeadsetConnection::switchToWifi(const QString& serial)
{
    qadb::Reply* reply = m_client->switchToWifi(serial);
    connect(reply, &qadb::Reply::finished, this, [this, reply] {
        if (reply->isSuccess())
            setCurrentSerial(reply->outputText());
    });
    return reply;
}

QString HeadsetConnection::displayName(const qadb::DeviceInfo& device)
{
    QString name = device.model;
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    return name.isEmpty() ? device.serial : name;
}

QString HeadsetConnection::linkName(const qadb::DeviceInfo& device)
{
    return device.isNetwork() ? tr("Wi-Fi") : tr("USB");
}

QString HeadsetConnection::stateDescription(const qadb::DeviceInfo& device)
{
    switch (device.state) {
    case qadb::DeviceState::Device:
        return tr("Ready");
    case qadb::DeviceState::Unauthorized:
        return tr("Put on the headset and allow USB debugging");
    case qadb::DeviceState::NoPermissions:
        return tr("This computer has no permission to use the headset");
    case qadb::DeviceState::Authorizing:
    case qadb::DeviceState::Connecting:
        return tr("Connecting...");
    case qadb::DeviceState::Offline:
        return tr("Not responding. Reconnect the cable or restart the headset");
    default:
        return tr("Not ready (%1)").arg(device.stateText);
    }
}

QIcon HeadsetConnection::statusIcon(bool ok)
{
    const QColor color = ok ? QColor(0x2e, 0xa0, 0x43) : QColor(0xd1, 0x24, 0x2f);
    QIcon icon;
    for (int size : {12, 16, 24, 32}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(color.darker(130), size / 16.0));
        painter.setBrush(color);
        const qreal margin = size / 6.0;
        painter.drawEllipse(QRectF(margin, margin, size - 2 * margin, size - 2 * margin));
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}
