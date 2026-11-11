#pragma once

#include "qadb/Reply.h"
#include "qadb/Types.h"

#include <QHash>
#include <QObject>
#include <QStringList>

class QIODevice;

namespace qadb {

// A client for the adb server (the background `adb` process on port 5037).
//
// Every method sends one request over its own connection and returns a reply right away. A
// `serial` names the device, as listed by devices(); an empty serial means "the only device", and
// fails when there are several.
class Client : public QObject
{
    Q_OBJECT

public:
    explicit Client(QObject* parent = nullptr);
    explicit Client(const ServerAddress& server, QObject* parent = nullptr);

    ServerAddress server() const { return m_server; }
    void setServer(const ServerAddress& server);

    // --- Server requests ------------------------------------------------------------------

    // host:version. output() holds the version as decimal text, for example "41".
    Reply* serverVersion();
    // host:devices-l
    DevicesReply* devices();
    // host:connect. Connects to a device over the network. Fails with CommandFailed when the
    // server could not connect; errorString() then holds the server's message.
    Reply* connectDevice(const QString& host, quint16 port = 5555);
    // host:pair. Pairs with a device's wireless debugging (Android 11 and later). `address` is the
    // host:port, or the mDNS service name, shown under "Pair device with pairing code"; `code`
    // is the 6-digit code shown there. The server stores the pairing and then connects to the
    // device by itself, so DeviceTracker reports it shortly after. output() holds the server's
    // message, such as "Successfully paired to 192.168.1.20:41234 [guid=adb-XXXX-YYYY]".
    // Fails with CommandFailed for a wrong code or an unreachable address.
    Reply* pair(const QString& address, const QString& code);
    // host:disconnect. An empty address disconnects every network device.
    Reply* disconnectDevice(const QString& address = {});
    // host:mdns:services. Devices on the local network that advertise wireless debugging.
    MdnsReply* mdnsServices();
    // host:kill. Stops the adb server.
    Reply* killServer();

    // --- Device requests ------------------------------------------------------------------

    // The adb features the device and server share, such as shell_v2 and cmd. Cached per serial.
    FeaturesReply* features(const QString& serial);
    void clearFeatureCache(const QString& serial = {});

    // Runs a command in the device's shell. Arguments are not quoted: use protocol::shellQuote()
    // or the QStringList overload.
    ShellReply* shell(const QString& serial, const QString& command);
    ShellReply* shell(const QString& serial, const QStringList& arguments);
    // exec: runs a command without the shell protocol. output() holds stdout byte for byte.
    Reply* exec(const QString& serial, const QString& command);
    // Restarts the device. `target` is empty, "bootloader", "recovery" or "sideload".
    Reply* reboot(const QString& serial, const QString& target = {});
    // Makes adbd listen on a TCP port, for `connectDevice` over Wi-Fi. The device must be
    // reachable by USB or already by network.
    Reply* enableTcpip(const QString& serial, quint16 port = 5555);

    // --- Files ----------------------------------------------------------------------------

    // Copies a local file to the device. `remotePath` is the full destination file path.
    Reply* push(const QString& serial, const QString& localPath, const QString& remotePath,
                quint32 permissions = 0644);
    // Copies bytes to a file on the device.
    Reply* pushData(const QString& serial, const QByteArray& data, const QString& remotePath,
                    quint32 permissions = 0644);
    // Copies a file from the device. The local file is written only if the copy succeeds.
    Reply* pull(const QString& serial, const QString& remotePath, const QString& localPath);
    StatReply* stat(const QString& serial, const QString& remotePath);
    ListReply* list(const QString& serial, const QString& remotePath);

    // --- Packages -------------------------------------------------------------------------

    // Installs an APK. Streams it to the package manager when the device supports `cmd`, or else
    // copies it to /data/local/tmp and runs `pm install`. output() holds the package manager's
    // answer; a "Failure [...]" answer finishes with CommandFailed.
    Reply* install(const QString& serial, const QString& apkPath,
                   const InstallOptions& options = {});
    Reply* uninstall(const QString& serial, const QString& packageName, bool keepData = false);

private:
    friend struct ClientOps;

    ServerAddress m_server;
    QHash<QString, QStringList> m_featureCache;
};

} // namespace qadb
