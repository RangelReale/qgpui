#include "qadb/Types.h"

#include <QHash>

namespace qadb {

ServerAddress ServerAddress::fromEnvironment()
{
    ServerAddress address;
    bool ok = false;
    const int port = qEnvironmentVariableIntValue("ANDROID_ADB_SERVER_PORT", &ok);
    if (ok && port > 0 && port < 65536)
        address.port = static_cast<quint16>(port);
    return address;
}

DeviceState deviceStateFromString(const QString& text)
{
    static const QHash<QString, DeviceState> states = {
        {QStringLiteral("device"), DeviceState::Device},
        {QStringLiteral("offline"), DeviceState::Offline},
        {QStringLiteral("unauthorized"), DeviceState::Unauthorized},
        {QStringLiteral("authorizing"), DeviceState::Authorizing},
        {QStringLiteral("connecting"), DeviceState::Connecting},
        {QStringLiteral("recovery"), DeviceState::Recovery},
        {QStringLiteral("sideload"), DeviceState::Sideload},
        {QStringLiteral("bootloader"), DeviceState::Bootloader},
        {QStringLiteral("rescue"), DeviceState::Rescue},
        {QStringLiteral("host"), DeviceState::Host},
    };
    if (text.startsWith(QLatin1String("no permissions")))
        return DeviceState::NoPermissions;
    return states.value(text, DeviceState::Unknown);
}

QString deviceStateName(DeviceState state)
{
    switch (state) {
    case DeviceState::Device: return QStringLiteral("device");
    case DeviceState::Offline: return QStringLiteral("offline");
    case DeviceState::Unauthorized: return QStringLiteral("unauthorized");
    case DeviceState::Authorizing: return QStringLiteral("authorizing");
    case DeviceState::Connecting: return QStringLiteral("connecting");
    case DeviceState::NoPermissions: return QStringLiteral("no permissions");
    case DeviceState::Recovery: return QStringLiteral("recovery");
    case DeviceState::Sideload: return QStringLiteral("sideload");
    case DeviceState::Bootloader: return QStringLiteral("bootloader");
    case DeviceState::Rescue: return QStringLiteral("rescue");
    case DeviceState::Host: return QStringLiteral("host");
    case DeviceState::Unknown: break;
    }
    return QStringLiteral("unknown");
}

bool DeviceInfo::isNetwork() const
{
    return serial.contains(QLatin1Char(':')) || serial.contains(QLatin1String("._tcp"));
}

} // namespace qadb
