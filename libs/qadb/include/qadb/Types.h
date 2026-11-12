#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace qadb {

// The adb server listens on 127.0.0.1:5037 unless ANDROID_ADB_SERVER_PORT says otherwise.
struct ServerAddress
{
    static constexpr quint16 kDefaultPort = 5037;

    QString host = QStringLiteral("127.0.0.1");
    quint16 port = kDefaultPort;

    // The default address, with the port taken from ANDROID_ADB_SERVER_PORT when it is set.
    static ServerAddress fromEnvironment();

    friend bool operator==(const ServerAddress&, const ServerAddress&) = default;
};

enum class DeviceState {
    Unknown,
    Device,       // Online and usable.
    Offline,
    Unauthorized, // The headset is waiting for the user to accept the RSA key.
    Authorizing,
    Connecting,
    NoPermissions,
    Recovery,
    Sideload,
    Bootloader,
    Rescue,
    Host,
};

DeviceState deviceStateFromString(const QString& text);
QString deviceStateName(DeviceState state);

// One line of `adb devices -l`.
struct DeviceInfo
{
    QString serial;
    DeviceState state = DeviceState::Unknown;
    QString stateText; // The state exactly as the server sent it.
    QString product;
    QString model;
    QString device;
    QString usb;
    qint64 transportId = -1;

    bool isOnline() const { return state == DeviceState::Device; }
    // True for devices reached over the network ("192.168.1.20:5555" or an mDNS name).
    bool isNetwork() const;

    friend bool operator==(const DeviceInfo&, const DeviceInfo&) = default;
};

// An entry returned by `host:mdns:services`.
struct MdnsService
{
    QString name;
    QString type; // For example _adb-tls-connect._tcp.
    QString address; // host:port

    friend bool operator==(const MdnsService&, const MdnsService&) = default;
};

// A file on the device, as reported by the sync STAT and LIST requests.
struct RemoteFileInfo
{
    QString name; // The entry name for LIST; the requested path for STAT.
    quint32 mode = 0; // Unix st_mode; 0 means the path does not exist.
    quint64 size = 0;
    QDateTime modified;

    bool exists() const { return mode != 0; }
    bool isDir() const { return (mode & 0170000) == 0040000; }
    bool isFile() const { return (mode & 0170000) == 0100000; }
    bool isSymLink() const { return (mode & 0170000) == 0120000; }
};

// An installed package. versionCode is -1 when unknown.
struct PackageInfo
{
    QString packageName;
    qint64 versionCode = -1;
    QString versionName;       // Only from packageInfo(), not from packages().
    QDateTime firstInstallTime; // Only from packageInfo().
    QDateTime lastUpdateTime;   // Only from packageInfo().

    friend bool operator==(const PackageInfo&, const PackageInfo&) = default;
};

// The size of a file system on the device, as `stat -f` reports it.
struct StorageInfo
{
    qint64 totalBytes = 0;
    qint64 freeBytes = 0; // Available to apps.
};

struct InstallOptions
{
    bool replace = true;            // -r: reinstall, keeping the app's data.
    bool allowDowngrade = false;    // -d
    bool grantPermissions = false;  // -g: grant all runtime permissions.
    bool allowTestPackages = false; // -t
    QStringList extraArguments;     // Passed to `pm install` as they are.
};

} // namespace qadb

Q_DECLARE_METATYPE(qadb::DeviceInfo)
