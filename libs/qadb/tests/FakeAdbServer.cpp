#include "FakeAdbServer.h"

#include <QTcpSocket>
#include <QtEndian>

namespace {

QByteArray hex4(qsizetype n)
{
    return QByteArray::number(n, 16).rightJustified(4, '0');
}

QByteArray hexString(const QByteArray& data)
{
    return hex4(data.size()) + data;
}

QByteArray failMessage(const QByteArray& message)
{
    return "FAIL" + hexString(message);
}

QByteArray le32(quint32 value)
{
    char bytes[4];
    qToLittleEndian(value, bytes);
    return QByteArray(bytes, 4);
}

QByteArray syncPacket(const char* id, const QByteArray& data)
{
    return QByteArray(id, 4) + le32(quint32(data.size())) + data;
}

QByteArray shellPacket(quint8 id, const QByteArray& data)
{
    return QByteArray(1, char(id)) + le32(quint32(data.size())) + data;
}

constexpr quint32 kMtime = 1700000000;

} // namespace

struct FakeAdbServer::Connection
{
    enum class Mode { Host, Transport, ShellV2, Install, Sync, SyncSend, Track, Discard, Closed };

    QTcpSocket* socket = nullptr;
    QByteArray buffer;
    Mode mode = Mode::Host;
    QString serial;
    bool trackLong = false;
    QString shellCommand;
    qint64 installRemaining = 0;
    QByteArray installData;
    QString sendPath;
    QByteArray sendData;

    void closeAfterWrite()
    {
        mode = Mode::Closed;
        socket->disconnectFromHost();
    }
};

FakeAdbServer::FakeAdbServer(QObject* parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &FakeAdbServer::onNewConnection);
}

FakeAdbServer::~FakeAdbServer()
{
    close();
}

bool FakeAdbServer::listen(quint16 port)
{
    return m_server.listen(QHostAddress::LocalHost, port);
}

void FakeAdbServer::close()
{
    m_server.close();
    const QList<Connection*> connections = std::exchange(m_connections, {});
    for (Connection* c : connections) {
        c->socket->disconnect(this);
        c->socket->abort();
        c->socket->deleteLater();
        delete c;
    }
}

void FakeAdbServer::setDevice(const QString& serial, const Device& device)
{
    m_devices.insert(serial, device);
    if (!m_transportIds.contains(serial))
        m_transportIds.insert(serial, int(m_transportIds.size()) + 1);
    notifyTrackers();
}

void FakeAdbServer::removeDevice(const QString& serial)
{
    m_devices.remove(serial);
    notifyTrackers();
}

void FakeAdbServer::onNewConnection()
{
    while (QTcpSocket* socket = m_server.nextPendingConnection()) {
        auto* c = new Connection;
        c->socket = socket;
        m_connections << c;
        connect(socket, &QTcpSocket::readyRead, this, [this, c] {
            c->buffer += c->socket->readAll();
            process(c);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, c] {
            m_connections.removeOne(c);
            c->socket->deleteLater();
            delete c;
        });
    }
}

QByteArray FakeAdbServer::deviceList(bool longFormat) const
{
    QByteArray text;
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        const int transportId = m_transportIds.value(it.key());
        if (longFormat) {
            text += it.key().toUtf8().leftJustified(22, ' ') + ' ' + it->state.toUtf8()
                    + " product:eureka model:" + it->model.toUtf8()
                    + " device:eureka transport_id:" + QByteArray::number(transportId) + '\n';
        } else {
            text += it.key().toUtf8() + '\t' + it->state.toUtf8() + '\n';
        }
    }
    return text;
}

void FakeAdbServer::notifyTrackers()
{
    for (Connection* c : std::as_const(m_connections)) {
        if (c->mode == Connection::Mode::Track)
            c->socket->write(hexString(deviceList(c->trackLong)));
    }
}

void FakeAdbServer::process(Connection* c)
{
    using Mode = Connection::Mode;
    for (;;) {
        switch (c->mode) {
        case Mode::Host:
        case Mode::Transport: {
            if (c->buffer.size() < 4)
                return;
            bool ok = false;
            const int length = c->buffer.left(4).toInt(&ok, 16);
            if (!ok) {
                c->closeAfterWrite();
                return;
            }
            if (c->buffer.size() < 4 + length)
                return;
            const QString request = QString::fromUtf8(c->buffer.mid(4, length));
            c->buffer.remove(0, 4 + length);
            const bool more = c->mode == Mode::Host ? handleHostRequest(c, request)
                                                    : handleDeviceRequest(c, request);
            if (!more)
                return;
            break;
        }
        case Mode::ShellV2: {
            if (c->buffer.size() < 5)
                return;
            const quint32 length = qFromLittleEndian<quint32>(c->buffer.constData() + 1);
            if (c->buffer.size() < 5 + qsizetype(length))
                return;
            const auto id = quint8(c->buffer.at(0));
            c->buffer.remove(0, 5 + length);
            if (id == 4) { // Close stdin: run the command and answer.
                const ShellResult result =
                    m_shellHandler ? m_shellHandler(c->serial, c->shellCommand) : ShellResult{};
                QByteArray answer;
                if (!result.out.isEmpty())
                    answer += shellPacket(1, result.out);
                if (!result.err.isEmpty())
                    answer += shellPacket(2, result.err);
                answer += shellPacket(3, QByteArray(1, char(result.exitCode)));
                c->socket->write(answer);
                c->closeAfterWrite();
                return;
            }
            break;
        }
        case Mode::Install: {
            const qint64 take = qMin<qint64>(c->installRemaining, c->buffer.size());
            c->installData += c->buffer.left(take);
            c->buffer.remove(0, take);
            c->installRemaining -= take;
            if (c->installRemaining > 0)
                return;
            installedApks << c->installData;
            if (c->installData.startsWith("FAIL:")) // "FAIL:<code>" answers "Failure [<code>]".
                c->socket->write("Failure [" + c->installData.mid(5).trimmed() + "]\n");
            else
                c->socket->write(c->installData.startsWith("BAD") ? "Failure [INSTALL_FAILED_INVALID_APK]\n"
                                                                  : "Success\n");
            c->closeAfterWrite();
            return;
        }
        case Mode::Sync:
        case Mode::SyncSend:
            if (!processSync(c))
                return;
            break;
        case Mode::Track:
        case Mode::Discard:
        case Mode::Closed:
            c->buffer.clear();
            return;
        }
    }
}

bool FakeAdbServer::handleHostRequest(Connection* c, const QString& request)
{
    requests << request;
    QTcpSocket* s = c->socket;

    if (request == QLatin1String("host:version")) {
        s->write("OKAY" + hexString("0029"));
        c->closeAfterWrite();
        return false;
    }
    if (request == QLatin1String("host:devices") || request == QLatin1String("host:devices-l")) {
        s->write("OKAY" + hexString(deviceList(request.endsWith(QLatin1String("-l")))));
        c->closeAfterWrite();
        return false;
    }
    if (request == QLatin1String("host:track-devices")
        || request == QLatin1String("host:track-devices-l")) {
        const bool longFormat = request.endsWith(QLatin1String("-l"));
        if (longFormat && !m_trackLongSupported) {
            s->write(failMessage("unknown host service"));
            c->closeAfterWrite();
            return false;
        }
        c->trackLong = longFormat;
        c->mode = Connection::Mode::Track;
        s->write("OKAY" + hexString(deviceList(longFormat)));
        return false;
    }
    if (request.startsWith(QLatin1String("host:transport"))) {
        QString serial;
        if (request == QLatin1String("host:transport-any")) {
            if (m_devices.size() != 1) {
                s->write(failMessage(m_devices.isEmpty() ? "no devices/emulators found"
                                                         : "more than one device/emulator"));
                c->closeAfterWrite();
                return false;
            }
            serial = m_devices.firstKey();
        } else {
            serial = request.mid(QStringLiteral("host:transport:").size());
        }
        if (!m_devices.contains(serial) || m_devices.value(serial).state != QLatin1String("device")) {
            s->write(failMessage("device '" + serial.toUtf8() + "' not found"));
            c->closeAfterWrite();
            return false;
        }
        c->serial = serial;
        c->mode = Connection::Mode::Transport;
        s->write("OKAY");
        return true;
    }
    if (request.endsWith(QLatin1String(":features"))) {
        QString serial;
        if (request.startsWith(QLatin1String("host-serial:")))
            serial = request.mid(12, request.size() - 12 - 9);
        else if (m_devices.size() == 1)
            serial = m_devices.firstKey();
        if (!m_devices.contains(serial)) {
            s->write(failMessage("device '" + serial.toUtf8() + "' not found"));
        } else {
            s->write("OKAY" + hexString(m_devices.value(serial).features.join(QLatin1Char(',')).toUtf8()));
        }
        c->closeAfterWrite();
        return false;
    }
    if (request.startsWith(QLatin1String("host:connect:"))) {
        const QByteArray address = request.mid(13).toUtf8();
        const bool refused = connectFailures > 0;
        if (refused)
            --connectFailures;
        s->write("OKAY"
                 + hexString(refused || address.startsWith("bad")
                                 ? "failed to connect to '" + address + "'"
                                 : "connected to " + address));
        c->closeAfterWrite();
        return false;
    }
    if (request.startsWith(QLatin1String("host:pair:"))) {
        // host:pair:<code>:<address>. The fake accepts the code 123456.
        const QString rest = request.mid(10);
        const qsizetype colon = rest.indexOf(QLatin1Char(':'));
        const QString code = rest.left(colon);
        const QByteArray address = rest.mid(colon + 1).toUtf8();
        // Server version 41 answers a failure with OKAY, then FAIL and the message.
        if (code == QLatin1String("123456"))
            s->write("OKAY" + hexString("Successfully paired to " + address + " [guid=adb-FAKE1234-abcdef]"));
        else
            s->write("OKAY" + failMessage("Failed: Wrong password or connection was dropped."));
        c->closeAfterWrite();
        return false;
    }
    if (request.startsWith(QLatin1String("host:disconnect:"))) {
        s->write("OKAY" + hexString("disconnected everything"));
        c->closeAfterWrite();
        return false;
    }
    if (request == QLatin1String("host:mdns:services")) {
        s->write("OKAY" + hexString("adb-2G0YC1234-abcd\t_adb-tls-connect._tcp\t192.168.1.20:37099\n"));
        c->closeAfterWrite();
        return false;
    }
    if (request == QLatin1String("host:kill")) {
        s->write("OKAY");
        c->closeAfterWrite();
        return false;
    }
    s->write(failMessage("unknown host service"));
    c->closeAfterWrite();
    return false;
}

bool FakeAdbServer::handleDeviceRequest(Connection* c, const QString& request)
{
    requests << c->serial + QLatin1Char('|') + request;
    QTcpSocket* s = c->socket;
    const auto runShell = [&](const QString& command) {
        return m_shellHandler ? m_shellHandler(c->serial, command) : ShellResult{};
    };

    if (request.startsWith(QLatin1String("shell,v2,raw:"))) {
        c->shellCommand = request.mid(13);
        c->mode = Connection::Mode::ShellV2;
        s->write("OKAY");
        return true;
    }
    if (request.startsWith(QLatin1String("shell:"))) {
        const ShellResult result = runShell(request.mid(6));
        s->write("OKAY" + result.out + result.err);
        c->closeAfterWrite();
        return false;
    }
    if (request.startsWith(QLatin1String("exec:cmd package install -S "))) {
        const QStringList parts = request.split(QLatin1Char(' '));
        c->installRemaining = parts.value(4).toLongLong();
        c->mode = Connection::Mode::Install;
        s->write("OKAY");
        return true;
    }
    if (request.startsWith(QLatin1String("exec:"))) {
        s->write("OKAY" + runShell(request.mid(5)).out);
        c->closeAfterWrite();
        return false;
    }
    if (request == QLatin1String("sync:")) {
        c->mode = Connection::Mode::Sync;
        s->write("OKAY");
        return true;
    }
    if (request.startsWith(QLatin1String("tcpip:"))) {
        s->write("OKAYrestarting in TCP mode port: " + request.mid(6).toUtf8() + '\n');
        c->closeAfterWrite();
        return false;
    }
    if (request.startsWith(QLatin1String("reboot:"))) {
        s->write("OKAY");
        c->closeAfterWrite();
        return false;
    }
    s->write(failMessage("unknown service"));
    c->closeAfterWrite();
    return false;
}

bool FakeAdbServer::processSync(Connection* c)
{
    QTcpSocket* s = c->socket;
    if (c->buffer.size() < 8)
        return false;
    const QByteArray id = c->buffer.left(4);
    const quint32 length = qFromLittleEndian<quint32>(c->buffer.constData() + 4);

    if (c->mode == Connection::Mode::SyncSend) {
        if (id == "DATA") {
            if (c->buffer.size() < 8 + qsizetype(length))
                return false;
            c->sendData += c->buffer.mid(8, length);
            c->buffer.remove(0, 8 + length);
            return true;
        }
        c->buffer.remove(0, 8);
        if (id == "DONE") {
            files.insert(c->sendPath, c->sendData);
            s->write(syncPacket("OKAY", {}));
            c->mode = Connection::Mode::Sync;
            return true;
        }
        c->closeAfterWrite();
        return false;
    }

    if (id == "QUIT") {
        c->buffer.clear();
        c->closeAfterWrite();
        return false;
    }
    if (c->buffer.size() < 8 + qsizetype(length))
        return false;
    const QString path = QString::fromUtf8(c->buffer.mid(8, length));
    c->buffer.remove(0, 8 + length);
    requests << c->serial + QLatin1String("|sync:") + QString::fromLatin1(id) + QLatin1Char(' ')
                    + path;

    const QString dirPrefix = path.endsWith(QLatin1Char('/')) ? path : path + QLatin1Char('/');
    const auto isDir = [&] {
        for (auto it = files.cbegin(); it != files.cend(); ++it) {
            if (it.key().startsWith(dirPrefix))
                return true;
        }
        return false;
    };

    if (id == "STAT") {
        quint32 mode = 0;
        quint32 size = 0;
        quint32 mtime = 0;
        if (files.contains(path)) {
            mode = 0100644;
            size = quint32(files.value(path).size());
            mtime = kMtime;
        } else if (isDir()) {
            mode = 040755;
            mtime = kMtime;
        }
        s->write("STAT" + le32(mode) + le32(size) + le32(mtime));
        return true;
    }
    if (id == "LIST") {
        QByteArray answer;
        const auto dent = [&](const QByteArray& name, quint32 mode, quint32 size) {
            answer += "DENT" + le32(mode) + le32(size) + le32(kMtime) + le32(quint32(name.size()))
                      + name;
        };
        dent(".", 040755, 0);
        dent("..", 040755, 0);
        QSet<QString> seenDirs;
        for (auto it = files.cbegin(); it != files.cend(); ++it) {
            if (!it.key().startsWith(dirPrefix))
                continue;
            const QString rest = it.key().mid(dirPrefix.size());
            const qsizetype slash = rest.indexOf(QLatin1Char('/'));
            if (slash < 0) {
                dent(rest.toUtf8(), 0100644, quint32(it->size()));
            } else if (!seenDirs.contains(rest.left(slash))) {
                seenDirs.insert(rest.left(slash));
                dent(rest.left(slash).toUtf8(), 040755, 0);
            }
        }
        answer += "DONE" + QByteArray(16, '\0');
        s->write(answer);
        return true;
    }
    if (id == "SEND") {
        const QString target = path.left(path.lastIndexOf(QLatin1Char(',')));
        if (m_readOnly.contains(target)) {
            s->write(syncPacket("FAIL", "Permission denied"));
            // Like adbd, stop reading; the client closes the connection.
            c->mode = Connection::Mode::Discard;
            return false;
        }
        c->sendPath = target;
        c->sendData.clear();
        c->mode = Connection::Mode::SyncSend;
        return true;
    }
    if (id == "RECV") {
        if (!files.contains(path)) {
            s->write(syncPacket("FAIL", "No such file or directory"));
            return true;
        }
        const QByteArray data = files.value(path);
        for (qsizetype offset = 0; offset < data.size(); offset += 64 * 1024)
            s->write(syncPacket("DATA", data.mid(offset, 64 * 1024)));
        s->write(syncPacket("DONE", {}));
        return true;
    }
    s->write(syncPacket("FAIL", "unknown sync request"));
    return true;
}
