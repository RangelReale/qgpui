#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTcpServer>

#include <functional>

class QTcpSocket;

// An in-process stand-in for the adb server and one or more devices, for tests.
//
// It speaks the host protocol, transports, shell (legacy and v2), exec, the streamed
// `cmd package install`, and the sync protocol against an in-memory file system.
class FakeAdbServer : public QObject
{
    Q_OBJECT

public:
    struct ShellResult
    {
        QByteArray out;
        QByteArray err;
        int exitCode = 0;
    };

    struct Device
    {
        QString state = QStringLiteral("device");
        QString model = QStringLiteral("Quest_3");
        QStringList features = {QStringLiteral("shell_v2"), QStringLiteral("cmd"),
                                QStringLiteral("stat_v2")};
    };

    explicit FakeAdbServer(QObject* parent = nullptr);
    ~FakeAdbServer() override;

    bool listen(quint16 port = 0);
    void close();
    quint16 port() const { return m_server.serverPort(); }

    // Devices, by serial. Changing them notifies host:track-devices clients.
    void setDevice(const QString& serial, const Device& device);
    void removeDevice(const QString& serial);

    void setShellHandler(std::function<ShellResult(const QString& serial, const QString& command)> h)
    {
        m_shellHandler = std::move(h);
    }
    void setTrackLongSupported(bool supported) { m_trackLongSupported = supported; }
    // SEND to these paths answers FAIL.
    void setReadOnlyPath(const QString& path) { m_readOnly.insert(path); }

    // The device file system, shared by all devices.
    QMap<QString, QByteArray> files;
    // Every host and device request, in order ("host:version", "<serial>|shell,v2,raw:ls", ...).
    QStringList requests;
    // The next this many host:connect requests fail, like a device whose adbd is restarting.
    int connectFailures = 0;
    // APKs received through `cmd package install -S`, in order.
    QList<QByteArray> installedApks;

private:
    struct Connection;

    void onNewConnection();
    void process(Connection* c);
    bool handleHostRequest(Connection* c, const QString& request);
    bool handleDeviceRequest(Connection* c, const QString& request);
    bool processSync(Connection* c);
    QByteArray deviceList(bool longFormat) const;
    void notifyTrackers();

    QTcpServer m_server;
    QMap<QString, Device> m_devices;
    QMap<QString, int> m_transportIds; // Stable per serial, like the real server's.
    QList<Connection*> m_connections;
    std::function<ShellResult(const QString&, const QString&)> m_shellHandler;
    bool m_trackLongSupported = true;
    QSet<QString> m_readOnly;
};
