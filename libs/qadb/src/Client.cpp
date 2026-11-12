#include "qadb/Client.h"

#include "Channel.h"
#include "ReplyAccess.h"
#include "qadb/Protocol.h"

#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>

#include <memory>

namespace qadb {

using detail::Channel;
using RA = detail::ReplyAccess;

namespace {

// Stop queueing writes above this many buffered bytes, and continue on bytesWritten().
constexpr qint64 kWriteHighWater = 1024 * 1024;

QByteArray transportRequest(const QString& serial)
{
    if (serial.isEmpty())
        return QByteArrayLiteral("host:transport-any");
    return QByteArrayLiteral("host:transport:") + serial.toUtf8();
}

QByteArray hostPrefix(const QString& serial)
{
    if (serial.isEmpty())
        return QByteArrayLiteral("host:");
    return QByteArrayLiteral("host-serial:") + serial.toUtf8() + ':';
}

QStringList installFlags(const InstallOptions& options)
{
    QStringList flags;
    if (options.replace)
        flags << QStringLiteral("-r");
    if (options.allowDowngrade)
        flags << QStringLiteral("-d");
    if (options.grantPermissions)
        flags << QStringLiteral("-g");
    if (options.allowTestPackages)
        flags << QStringLiteral("-t");
    flags << options.extraArguments;
    return flags;
}

// The package manager prints "Success", or "Failure [REASON]" and sometimes more lines.
bool isPackageManagerSuccess(const QByteArray& output)
{
    return output.contains("Success") && !output.contains("Failure");
}

QString packageManagerError(const QByteArray& output, const QString& fallback)
{
    const QString text = QString::fromUtf8(output).trimmed();
    return text.isEmpty() ? fallback : text;
}

// Reads `source` and writes it to `channel` in chunks, wrapping each chunk with `frame`. Stops
// queueing while the socket holds more than kWriteHighWater bytes. Calls `onEnd` once the source
// is exhausted, or fails `reply` with FileError on a read error.
void pump(Reply* reply, Channel* channel, std::shared_ptr<QIODevice> source, qint64 total,
          std::function<QByteArray(const QByteArray&)> frame, std::function<void()> onEnd)
{
    struct State
    {
        qint64 sent = 0;
        bool ended = false;
    };
    auto state = std::make_shared<State>();
    QPointer<Channel> guard(channel);

    auto step = [=] {
        if (state->ended || reply->isFinished() || !guard || guard->isDone())
            return;
        while (guard->bytesToWrite() < kWriteHighWater) {
            QByteArray buffer(protocol::kSyncMaxChunk, Qt::Uninitialized);
            const qint64 count = source->read(buffer.data(), buffer.size());
            if (count < 0) {
                state->ended = true;
                guard->abort();
                RA::fail(reply, Reply::Error::FileError,
                         QStringLiteral("Cannot read the local file: %1").arg(source->errorString()));
                return;
            }
            if (count == 0) {
                state->ended = true;
                onEnd();
                return;
            }
            buffer.resize(count);
            guard->write(frame ? frame(buffer) : buffer);
            state->sent += count;
            // A progress handler may delete the reply, and the channel with it.
            RA::progress(reply, state->sent, total);
            if (!guard)
                return;
        }
    };
    QObject::connect(channel, &Channel::bytesWritten, reply, step);
    step();
}

QDateTime fromUnixSeconds(quint32 seconds)
{
    return QDateTime::fromSecsSinceEpoch(seconds);
}

RemoteFileInfo parseStat(const QByteArray& data, const QString& name)
{
    RemoteFileInfo info;
    info.name = name;
    info.mode = protocol::readLittleEndian32(data.constData() + 4);
    info.size = protocol::readLittleEndian32(data.constData() + 8);
    info.modified = fromUnixSeconds(protocol::readLittleEndian32(data.constData() + 12));
    return info;
}

bool checkRemotePath(Reply* reply, const QString& path)
{
    if (path.toUtf8().size() > protocol::kSyncMaxPathLength) {
        RA::fail(reply, Reply::Error::CommandFailed,
                 QStringLiteral("The remote path is longer than %1 bytes")
                     .arg(protocol::kSyncMaxPathLength));
        return false;
    }
    return true;
}

} // namespace

// The request implementations. A struct, so that Client can befriend it.
struct ClientOps
{
    static Channel* newChannel(Client* client, Reply* reply)
    {
        auto* channel = new Channel(client->server(), reply);
        QObject::connect(channel, &Channel::failed, reply,
                         [reply](Reply::Error error, const QString& message) {
                             RA::fail(reply, error, message);
                         });
        QPointer<Channel> guard(channel);
        RA::setAbortHandler(reply, [guard] {
            if (guard)
                guard->abort();
        });
        return channel;
    }

    // Sends a host request. When `hasPayload`, reads the hex-length answer that follows OKAY.
    static void hostQuery(Client* client, Reply* reply, const QByteArray& request, bool hasPayload,
                          std::function<void(const QByteArray&)> onAnswer)
    {
        detail::runLater(reply, [=] {
            if (reply->isFinished())
                return;
            Channel* channel = newChannel(client, reply);
            channel->open([=] {
                channel->sendRequest(request);
                channel->readStatus([=] {
                    if (!hasPayload) {
                        channel->close();
                        onAnswer({});
                        return;
                    }
                    channel->readHexString([=](const QByteArray& answer) {
                        channel->close();
                        onAnswer(answer);
                    });
                });
            });
        });
    }

    // Switches a new connection to the device, starts `service` and calls `onOpen` after OKAY.
    // Runs immediately; callers are already inside the event loop.
    static void deviceService(Client* client, Reply* reply, const QString& serial,
                              const QByteArray& service, std::function<void(Channel*)> onOpen)
    {
        if (reply->isFinished())
            return;
        Channel* channel = newChannel(client, reply);
        channel->open([=] {
            channel->sendRequest(transportRequest(serial));
            channel->readStatus([=] {
                channel->sendRequest(service);
                channel->readStatus([=] { onOpen(channel); });
            });
        });
    }

    // Makes `child` part of `parent`: aborting the parent aborts the child, and a failed child
    // fails the parent. `onSuccess` runs when the child succeeds.
    static void chain(Reply* parent, Reply* child, std::function<void()> onSuccess)
    {
        child->setParent(parent);
        QPointer<Reply> guard(child);
        RA::setAbortHandler(parent, [guard] {
            if (guard)
                guard->abort();
        });
        QObject::connect(child, &Reply::finished, parent, [=] {
            child->deleteLater();
            if (parent->isFinished())
                return;
            if (!child->isSuccess()) {
                RA::fail(parent, child->error(), child->errorString());
                return;
            }
            onSuccess();
        });
    }

    // Calls `onFeatures` with the device's features, or fails `reply`.
    static void withFeatures(Client* client, Reply* reply, const QString& serial,
                             std::function<void(const QStringList&)> onFeatures)
    {
        FeaturesReply* child = client->features(serial);
        chain(reply, child, [=] { onFeatures(child->features()); });
    }

    static void finishSync(Channel* channel)
    {
        channel->write(protocol::syncHeader("QUIT", 0));
        channel->close();
    }

    // Reads a FAIL message's text and fails the reply with it.
    static void readSyncFailure(Channel* channel, const QByteArray& header)
    {
        const quint32 length = protocol::readLittleEndian32(header.constData() + 4);
        channel->readExact(length, [channel](const QByteArray& message) {
            channel->fail(Reply::Error::ServerError, QString::fromUtf8(message));
        });
    }

    static void unexpectedSync(Channel* channel, const QByteArray& header)
    {
        channel->fail(Reply::Error::ProtocolError,
                      QStringLiteral("Unexpected sync answer: %1")
                          .arg(QString::fromLatin1(header.left(4).toHex())));
    }

    static void push(Client* client, Reply* reply, const QString& serial,
                     std::function<std::shared_ptr<QIODevice>(QString*)> openSource,
                     const QString& remotePath, quint32 permissions, quint32 mtime)
    {
        detail::runLater(reply, [=] {
            if (reply->isFinished() || !checkRemotePath(reply, remotePath))
                return;
            QString error;
            const std::shared_ptr<QIODevice> source = openSource(&error);
            if (!source) {
                RA::fail(reply, Reply::Error::FileError, error);
                return;
            }
            const qint64 total = source->size();

            deviceService(client, reply, serial, "sync:", [=](Channel* channel) {
                const QByteArray spec = remotePath.toUtf8() + ','
                                        + QByteArray::number(0100000 | (permissions & 07777));
                channel->write(protocol::syncPacket("SEND", spec));

                auto finishedWriting = std::make_shared<bool>(false);
                // The device answers once, after DONE, or early with FAIL.
                channel->readExact(8, [=](const QByteArray& header) {
                    const QByteArray id = header.left(4);
                    if (id == "FAIL") {
                        readSyncFailure(channel, header);
                    } else if (id == "OKAY" && *finishedWriting) {
                        finishSync(channel);
                        RA::progress(reply, total, total);
                        RA::finish(reply);
                    } else {
                        unexpectedSync(channel, header);
                    }
                });

                pump(reply, channel, source, total,
                     [](const QByteArray& chunk) { return protocol::syncPacket("DATA", chunk); },
                     [=] {
                         *finishedWriting = true;
                         channel->write(protocol::syncHeader("DONE", mtime));
                     });
            });
        });
    }

    static void installStreamed(Client* client, Reply* reply, const QString& serial,
                                const QString& apkPath, const InstallOptions& options)
    {
        auto file = std::make_shared<QFile>(apkPath);
        if (!file->open(QIODevice::ReadOnly)) {
            RA::fail(reply, Reply::Error::FileError,
                     QStringLiteral("Cannot open %1: %2").arg(apkPath, file->errorString()));
            return;
        }
        const qint64 size = file->size();
        const QString command =
            protocol::shellCommand(QStringList{QStringLiteral("cmd"), QStringLiteral("package"),
                                               QStringLiteral("install"), QStringLiteral("-S"),
                                               QString::number(size)}
                                   + installFlags(options));

        deviceService(client, reply, serial, "exec:" + command.toUtf8(), [=](Channel* channel) {
            auto output = std::make_shared<QByteArray>();
            channel->readUntilClosed([=](const QByteArray& data) { *output += data; },
                                     [=] {
                                         RA::setOutput(reply, *output);
                                         if (isPackageManagerSuccess(*output)) {
                                             RA::finish(reply);
                                         } else {
                                             RA::fail(reply, Reply::Error::CommandFailed,
                                                      packageManagerError(
                                                          *output, QStringLiteral("Install failed")));
                                         }
                                     });
            pump(reply, channel, file, size, {}, [] {});
        });
    }

    static void installLegacy(Client* client, Reply* reply, const QString& serial,
                              const QString& apkPath, const InstallOptions& options)
    {
        static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9._-]"));
        QString name = QFileInfo(apkPath).fileName();
        name.replace(unsafe, QStringLiteral("_"));
        const QString remote = QStringLiteral("/data/local/tmp/qadb-") + name;

        Reply* upload = client->push(serial, apkPath, remote);
        QObject::connect(upload, &Reply::progress, reply,
                         [reply](qint64 done, qint64 total) { RA::progress(reply, done, total); });
        chain(reply, upload, [=] {
            ShellReply* pm = client->shell(
                serial, QStringList{QStringLiteral("pm"), QStringLiteral("install")}
                            + installFlags(options) + QStringList{remote});
            chain(reply, pm, [=] {
                const QByteArray output = pm->output() + pm->standardError();
                // Remove the copy whatever the result, then report.
                ShellReply* cleanup =
                    client->shell(serial, QStringList{QStringLiteral("rm"), QStringLiteral("-f"), remote});
                cleanup->setParent(reply);
                QObject::connect(cleanup, &Reply::finished, reply, [=] {
                    cleanup->deleteLater();
                    RA::setOutput(reply, output);
                    if (isPackageManagerSuccess(output)) {
                        RA::finish(reply);
                    } else {
                        RA::fail(reply, Reply::Error::CommandFailed,
                                 packageManagerError(output, QStringLiteral("Install failed")));
                    }
                });
            });
        });
    }
};

Client::Client(QObject* parent)
    : Client(ServerAddress::fromEnvironment(), parent)
{
}

Client::Client(const ServerAddress& server, QObject* parent)
    : QObject(parent)
    , m_server(server)
{
}

void Client::setServer(const ServerAddress& server)
{
    if (m_server == server)
        return;
    m_server = server;
    m_featureCache.clear();
}

Reply* Client::serverVersion()
{
    auto* reply = RA::create<Reply>(this);
    ClientOps::hostQuery(this, reply, "host:version", true, [reply](const QByteArray& answer) {
        bool ok = false;
        const int version = answer.toInt(&ok, 16);
        if (!ok) {
            RA::fail(reply, Reply::Error::ProtocolError,
                     QStringLiteral("Invalid version: %1").arg(QString::fromUtf8(answer)));
            return;
        }
        RA::setOutput(reply, QByteArray::number(version));
        RA::finish(reply);
    });
    return reply;
}

DevicesReply* Client::devices()
{
    auto* reply = RA::create<DevicesReply>(this);
    ClientOps::hostQuery(this, reply, "host:devices-l", true, [reply](const QByteArray& answer) {
        RA::setOutput(reply, answer);
        RA::setDevices(reply, protocol::parseDeviceList(answer));
        RA::finish(reply);
    });
    return reply;
}

Reply* Client::connectDevice(const QString& host, quint16 port)
{
    auto* reply = RA::create<Reply>(this);
    const QByteArray address = host.toUtf8() + ':' + QByteArray::number(port);
    ClientOps::hostQuery(this, reply, "host:connect:" + address, true,
                         [reply](const QByteArray& answer) {
                             RA::setOutput(reply, answer);
                             if (answer.startsWith("connected to")
                                 || answer.startsWith("already connected to")) {
                                 RA::finish(reply);
                             } else {
                                 RA::fail(reply, Reply::Error::CommandFailed,
                                          QString::fromUtf8(answer).trimmed());
                             }
                         });
    return reply;
}

Reply* Client::pair(const QString& address, const QString& code)
{
    auto* reply = RA::create<Reply>(this);
    const QString trimmedCode = code.trimmed();
    if (trimmedCode.isEmpty() || trimmedCode.contains(QLatin1Char(':'))
        || address.trimmed().isEmpty()) {
        detail::runLater(reply, [reply] {
            RA::fail(reply, Reply::Error::CommandFailed,
                     QStringLiteral("Pairing needs an address and a pairing code"));
        });
        return reply;
    }
    const QByteArray request =
        "host:pair:" + trimmedCode.toUtf8() + ':' + address.trimmed().toUtf8();
    const auto report = [reply](const QByteArray& answer) {
        RA::setOutput(reply, answer);
        if (answer.startsWith("Successfully paired")) {
            RA::finish(reply);
        } else {
            RA::fail(reply, Reply::Error::CommandFailed, QString::fromUtf8(answer).trimmed());
        }
    };
    detail::runLater(reply, [this, reply, request, report] {
        if (reply->isFinished())
            return;
        Channel* channel = ClientOps::newChannel(this, reply);
        channel->open([=] {
            channel->sendRequest(request);
            // The server answers OKAY, then (as of server version 41) a second status before the
            // message: "OKAY" plus FAIL and a hex-length message when pairing fails. Accept that,
            // a second OKAY, or the documented hex-length message right away.
            channel->readStatus([=] {
                channel->readExact(4, [=](const QByteArray& next) {
                    if (next == "FAIL" || next == "OKAY") {
                        channel->readHexString([=](const QByteArray& message) {
                            channel->close();
                            report(message);
                        });
                        return;
                    }
                    const int length = protocol::parseHexLength(next);
                    if (length < 0) {
                        channel->fail(Reply::Error::ProtocolError,
                                      QStringLiteral("Unexpected pairing answer: %1")
                                          .arg(QString::fromLatin1(next.toHex())));
                        return;
                    }
                    channel->readExact(length, [=](const QByteArray& message) {
                        channel->close();
                        report(message);
                    });
                });
            });
        });
    });
    return reply;
}

Reply* Client::disconnectDevice(const QString& address)
{
    auto* reply = RA::create<Reply>(this);
    ClientOps::hostQuery(this, reply, "host:disconnect:" + address.toUtf8(), true,
                         [reply](const QByteArray& answer) {
                             RA::setOutput(reply, answer);
                             if (answer.startsWith("error")) {
                                 RA::fail(reply, Reply::Error::CommandFailed,
                                          QString::fromUtf8(answer).trimmed());
                             } else {
                                 RA::finish(reply);
                             }
                         });
    return reply;
}

MdnsReply* Client::mdnsServices()
{
    auto* reply = RA::create<MdnsReply>(this);
    ClientOps::hostQuery(this, reply, "host:mdns:services", true,
                         [reply](const QByteArray& answer) {
                             RA::setOutput(reply, answer);
                             RA::setServices(reply, protocol::parseMdnsServices(answer));
                             RA::finish(reply);
                         });
    return reply;
}

Reply* Client::killServer()
{
    auto* reply = RA::create<Reply>(this);
    ClientOps::hostQuery(this, reply, "host:kill", false, [this, reply](const QByteArray&) {
        m_featureCache.clear();
        RA::finish(reply);
    });
    return reply;
}

FeaturesReply* Client::features(const QString& serial)
{
    auto* reply = RA::create<FeaturesReply>(this);
    if (!serial.isEmpty() && m_featureCache.contains(serial)) {
        const QStringList cached = m_featureCache.value(serial);
        detail::runLater(reply, [reply, cached] {
            if (reply->isFinished())
                return;
            RA::setFeatures(reply, cached);
            RA::setOutput(reply, cached.join(QLatin1Char(',')).toUtf8());
            RA::finish(reply);
        });
        return reply;
    }
    QPointer<Client> self(this);
    ClientOps::hostQuery(this, reply, hostPrefix(serial) + "features", true,
                         [self, reply, serial](const QByteArray& answer) {
                             const QStringList features = protocol::parseFeatures(answer);
                             if (self && !serial.isEmpty())
                                 self->m_featureCache.insert(serial, features);
                             RA::setFeatures(reply, features);
                             RA::setOutput(reply, answer);
                             RA::finish(reply);
                         });
    return reply;
}

void Client::clearFeatureCache(const QString& serial)
{
    if (serial.isEmpty())
        m_featureCache.clear();
    else
        m_featureCache.remove(serial);
}

ShellReply* Client::shell(const QString& serial, const QStringList& arguments)
{
    return shell(serial, protocol::shellCommand(arguments));
}

ShellReply* Client::shell(const QString& serial, const QString& command)
{
    auto* reply = RA::create<ShellReply>(this);
    detail::runLater(reply, [this, reply, serial, command] {
        if (reply->isFinished())
            return;
        ClientOps::withFeatures(this, reply, serial, [=](const QStringList& features) {
            const bool v2 = features.contains(QStringLiteral("shell_v2"));
            const QByteArray service =
                (v2 ? QByteArrayLiteral("shell,v2,raw:") : QByteArrayLiteral("shell:"))
                + command.toUtf8();

            ClientOps::deviceService(this, reply, serial, service, [=](Channel* channel) {
                if (!v2) {
                    auto output = std::make_shared<QByteArray>();
                    channel->readUntilClosed([=](const QByteArray& data) { *output += data; },
                                             [=] {
                                                 RA::setOutput(reply, *output);
                                                 RA::finish(reply);
                                             });
                    return;
                }

                // Shell protocol v2: packets of 1-byte id, 4-byte little-endian length, data.
                struct State
                {
                    QByteArray buffer;
                    QByteArray out;
                    QByteArray err;
                    int exitCode = -1;
                };
                auto state = std::make_shared<State>();
                // There is no input: tell the command so it does not wait for any.
                channel->write(protocol::shellPacket(protocol::ShellCloseStdin));
                channel->readUntilClosed(
                    [=](const QByteArray& data) {
                        state->buffer += data;
                        while (state->buffer.size() >= 5) {
                            const quint32 length =
                                protocol::readLittleEndian32(state->buffer.constData() + 1);
                            if (state->buffer.size() < 5 + qsizetype(length))
                                break;
                            const auto id = static_cast<quint8>(state->buffer.at(0));
                            const QByteArray payload = state->buffer.mid(5, length);
                            state->buffer.remove(0, 5 + length);
                            if (id == protocol::ShellStdout)
                                state->out += payload;
                            else if (id == protocol::ShellStderr)
                                state->err += payload;
                            else if (id == protocol::ShellExit && !payload.isEmpty())
                                state->exitCode = static_cast<quint8>(payload.at(0));
                        }
                    },
                    [=] {
                        RA::setOutput(reply, state->out);
                        RA::setShellResult(reply, state->err, state->exitCode);
                        if (state->exitCode < 0) {
                            RA::fail(reply, Reply::Error::ProtocolError,
                                     QStringLiteral("The shell closed without an exit code"));
                        } else {
                            RA::finish(reply);
                        }
                    });
            });
        });
    });
    return reply;
}

namespace {

// Starts `service` on the device and collects its output until it closes the connection.
Reply* runStreamService(Client* client, const QString& serial, const QByteArray& service)
{
    auto* reply = RA::create<Reply>(client);
    detail::runLater(reply, [=] {
        ClientOps::deviceService(client, reply, serial, service, [reply](Channel* channel) {
            auto output = std::make_shared<QByteArray>();
            channel->readUntilClosed([=](const QByteArray& data) { *output += data; },
                                     [=] {
                                         RA::setOutput(reply, *output);
                                         RA::finish(reply);
                                     });
        });
    });
    return reply;
}

} // namespace

Reply* Client::exec(const QString& serial, const QString& command)
{
    return runStreamService(this, serial, "exec:" + command.toUtf8());
}

Reply* Client::reboot(const QString& serial, const QString& target)
{
    return runStreamService(this, serial, "reboot:" + target.toUtf8());
}

Reply* Client::enableTcpip(const QString& serial, quint16 port)
{
    return runStreamService(this, serial, "tcpip:" + QByteArray::number(port));
}

namespace {

// After `tcpip`, adbd restarts and refuses connections for a moment.
constexpr int kWifiConnectAttempts = 10;
constexpr int kWifiConnectRetryMsecs = 1000;

// Connects `reply` to host:port, and tries again after a pause while attempts remain.
void connectWithRetry(Client* client, Reply* reply, const QString& host, quint16 port, int attemptsLeft)
{
    Reply* attempt = client->connectDevice(host, port);
    attempt->setParent(reply);
    QPointer<Reply> guard(attempt);
    RA::setAbortHandler(reply, [guard] {
        if (guard)
            guard->abort();
    });
    QObject::connect(attempt, &Reply::finished, reply, [=] {
        attempt->deleteLater();
        if (reply->isFinished())
            return;
        if (attempt->isSuccess()) {
            RA::setOutput(reply, (host + QLatin1Char(':') + QString::number(port)).toUtf8());
            RA::finish(reply);
            return;
        }
        if (attemptsLeft <= 1) {
            RA::fail(reply, attempt->error(), attempt->errorString());
            return;
        }
        RA::setAbortHandler(reply, {});
        QPointer<Client> self(client);
        QTimer::singleShot(kWifiConnectRetryMsecs, reply, [=] {
            if (reply->isFinished())
                return;
            if (!self) {
                RA::fail(reply, Reply::Error::ConnectionFailed, QStringLiteral("The client was deleted"));
                return;
            }
            connectWithRetry(self, reply, host, port, attemptsLeft - 1);
        });
    });
}

} // namespace

Reply* Client::switchToWifi(const QString& serial, quint16 port)
{
    auto* reply = RA::create<Reply>(this);
    detail::runLater(reply, [this, reply, serial, port] {
        if (reply->isFinished())
            return;
        ShellReply* ip = shell(serial, QStringLiteral("ip -f inet addr show wlan0"));
        ClientOps::chain(reply, ip, [=] {
            static const QRegularExpression inet(QStringLiteral("inet (\\d+\\.\\d+\\.\\d+\\.\\d+)"));
            const QRegularExpressionMatch match = inet.match(ip->outputText());
            if (!match.hasMatch()) {
                RA::fail(reply, Reply::Error::CommandFailed,
                         QStringLiteral("The device is not connected to Wi-Fi"));
                return;
            }
            const QString host = match.captured(1);
            Reply* tcpip = enableTcpip(serial, port);
            ClientOps::chain(reply, tcpip, [=] {
                connectWithRetry(this, reply, host, port, kWifiConnectAttempts);
            });
        });
    });
    return reply;
}

Reply* Client::push(const QString& serial, const QString& localPath, const QString& remotePath,
                    quint32 permissions)
{
    auto* reply = RA::create<Reply>(this);
    const QFileInfo info(localPath);
    const quint32 mtime = info.exists()
                              ? static_cast<quint32>(info.lastModified().toSecsSinceEpoch())
                              : static_cast<quint32>(QDateTime::currentSecsSinceEpoch());
    ClientOps::push(
        this, reply, serial,
        [localPath](QString* error) -> std::shared_ptr<QIODevice> {
            auto file = std::make_shared<QFile>(localPath);
            if (!file->open(QIODevice::ReadOnly)) {
                *error = QStringLiteral("Cannot open %1: %2").arg(localPath, file->errorString());
                return {};
            }
            return file;
        },
        remotePath, permissions, mtime);
    return reply;
}

Reply* Client::pushData(const QString& serial, const QByteArray& data, const QString& remotePath,
                        quint32 permissions)
{
    auto* reply = RA::create<Reply>(this);
    ClientOps::push(
        this, reply, serial,
        [data](QString*) -> std::shared_ptr<QIODevice> {
            auto buffer = std::make_shared<QBuffer>();
            buffer->setData(data);
            buffer->open(QIODevice::ReadOnly);
            return buffer;
        },
        remotePath, permissions, static_cast<quint32>(QDateTime::currentSecsSinceEpoch()));
    return reply;
}

Reply* Client::pull(const QString& serial, const QString& remotePath, const QString& localPath)
{
    auto* reply = RA::create<Reply>(this);
    detail::runLater(reply, [this, reply, serial, remotePath, localPath] {
        if (reply->isFinished() || !checkRemotePath(reply, remotePath))
            return;
        auto file = std::make_shared<QSaveFile>(localPath);
        if (!file->open(QIODevice::WriteOnly)) {
            RA::fail(reply, Reply::Error::FileError,
                     QStringLiteral("Cannot write %1: %2").arg(localPath, file->errorString()));
            return;
        }

        ClientOps::deviceService(this, reply, serial, "sync:", [=](Channel* channel) {
            const QByteArray path = remotePath.toUtf8();
            channel->write(protocol::syncPacket("STAT", path));
            channel->readExact(16, [=](const QByteArray& statData) {
                if (!statData.startsWith("STAT")) {
                    ClientOps::unexpectedSync(channel, statData);
                    return;
                }
                const RemoteFileInfo info = parseStat(statData, remotePath);
                if (!info.exists()) {
                    channel->abort();
                    RA::fail(reply, Reply::Error::ServerError,
                             QStringLiteral("No such file on the device: %1").arg(remotePath));
                    return;
                }
                if (info.isDir()) {
                    channel->abort();
                    RA::fail(reply, Reply::Error::CommandFailed,
                             QStringLiteral("%1 is a folder").arg(remotePath));
                    return;
                }
                const qint64 total = qint64(info.size);
                channel->write(protocol::syncPacket("RECV", path));

                auto received = std::make_shared<qint64>(0);
                auto step = std::make_shared<std::function<void()>>();
                std::weak_ptr<std::function<void()>> weakStep = step;
                *step = [=] {
                    channel->readExact(8, [=, keep = weakStep.lock()](const QByteArray& header) {
                        const QByteArray id = header.left(4);
                        if (id == "DATA") {
                            const quint32 length =
                                protocol::readLittleEndian32(header.constData() + 4);
                            channel->readExact(length, [=](const QByteArray& chunk) {
                                if (file->write(chunk) != chunk.size()) {
                                    channel->abort();
                                    RA::fail(reply, Reply::Error::FileError,
                                             QStringLiteral("Cannot write %1: %2")
                                                 .arg(localPath, file->errorString()));
                                    return;
                                }
                                *received += chunk.size();
                                // Queue the next read before the progress handler runs: it
                                // may delete the reply, and the channel with it.
                                if (keep)
                                    (*keep)();
                                RA::progress(reply, *received, total);
                            });
                        } else if (id == "DONE") {
                            ClientOps::finishSync(channel);
                            if (!file->commit()) {
                                RA::fail(reply, Reply::Error::FileError,
                                         QStringLiteral("Cannot write %1: %2")
                                             .arg(localPath, file->errorString()));
                                return;
                            }
                            RA::finish(reply);
                        } else if (id == "FAIL") {
                            ClientOps::readSyncFailure(channel, header);
                        } else {
                            ClientOps::unexpectedSync(channel, header);
                        }
                    });
                };
                (*step)();
            });
        });
    });
    return reply;
}

StatReply* Client::stat(const QString& serial, const QString& remotePath)
{
    auto* reply = RA::create<StatReply>(this);
    detail::runLater(reply, [this, reply, serial, remotePath] {
        if (reply->isFinished() || !checkRemotePath(reply, remotePath))
            return;
        ClientOps::deviceService(this, reply, serial, "sync:", [=](Channel* channel) {
            channel->write(protocol::syncPacket("STAT", remotePath.toUtf8()));
            channel->readExact(16, [=](const QByteArray& data) {
                if (!data.startsWith("STAT")) {
                    ClientOps::unexpectedSync(channel, data);
                    return;
                }
                ClientOps::finishSync(channel);
                RA::setInfo(reply, parseStat(data, remotePath));
                RA::finish(reply);
            });
        });
    });
    return reply;
}

ListReply* Client::list(const QString& serial, const QString& remotePath)
{
    auto* reply = RA::create<ListReply>(this);
    detail::runLater(reply, [this, reply, serial, remotePath] {
        if (reply->isFinished() || !checkRemotePath(reply, remotePath))
            return;
        ClientOps::deviceService(this, reply, serial, "sync:", [=](Channel* channel) {
            channel->write(protocol::syncPacket("LIST", remotePath.toUtf8()));
            auto entries = std::make_shared<QList<RemoteFileInfo>>();
            auto step = std::make_shared<std::function<void()>>();
            std::weak_ptr<std::function<void()>> weakStep = step;
            *step = [=] {
                // DENT, mode, size, mtime, name length; or DONE and 16 bytes of zeros.
                channel->readExact(20, [=, keep = weakStep.lock()](const QByteArray& header) {
                    const QByteArray id = header.left(4);
                    if (id == "DONE") {
                        ClientOps::finishSync(channel);
                        RA::setEntries(reply, *entries);
                        RA::finish(reply);
                    } else if (id == "DENT") {
                        const quint32 nameLength =
                            protocol::readLittleEndian32(header.constData() + 16);
                        channel->readExact(nameLength, [=](const QByteArray& name) {
                            if (name != "." && name != "..") {
                                RemoteFileInfo info = parseStat(header, QString::fromUtf8(name));
                                entries->append(info);
                            }
                            if (keep)
                                (*keep)();
                        });
                    } else if (id == "FAIL") {
                        // A FAIL header is 8 bytes; 12 of the 20 read belong to the message.
                        const quint32 length = protocol::readLittleEndian32(header.constData() + 4);
                        const QByteArray start = header.mid(8);
                        if (length <= quint32(start.size())) {
                            channel->fail(Reply::Error::ServerError,
                                          QString::fromUtf8(start.left(length)));
                        } else {
                            channel->readExact(length - start.size(), [=](const QByteArray& rest) {
                                channel->fail(Reply::Error::ServerError,
                                              QString::fromUtf8(start + rest));
                            });
                        }
                    } else {
                        ClientOps::unexpectedSync(channel, header);
                    }
                });
            };
            (*step)();
        });
    });
    return reply;
}

Reply* Client::install(const QString& serial, const QString& apkPath, const InstallOptions& options)
{
    auto* reply = RA::create<Reply>(this);
    detail::runLater(reply, [this, reply, serial, apkPath, options] {
        if (reply->isFinished())
            return;
        const QFileInfo info(apkPath);
        if (!info.isFile() || !info.isReadable()) {
            RA::fail(reply, Reply::Error::FileError,
                     QStringLiteral("Cannot read %1").arg(apkPath));
            return;
        }
        ClientOps::withFeatures(this, reply, serial, [=](const QStringList& features) {
            if (features.contains(QStringLiteral("cmd")))
                ClientOps::installStreamed(this, reply, serial, apkPath, options);
            else
                ClientOps::installLegacy(this, reply, serial, apkPath, options);
        });
    });
    return reply;
}

Reply* Client::uninstall(const QString& serial, const QString& packageName, bool keepData)
{
    auto* reply = RA::create<Reply>(this);
    detail::runLater(reply, [this, reply, serial, packageName, keepData] {
        if (reply->isFinished())
            return;
        QStringList arguments{QStringLiteral("pm"), QStringLiteral("uninstall")};
        if (keepData)
            arguments << QStringLiteral("-k");
        arguments << packageName;
        ShellReply* pm = shell(serial, arguments);
        ClientOps::chain(reply, pm, [=] {
            const QByteArray output = pm->output() + pm->standardError();
            RA::setOutput(reply, output);
            if (isPackageManagerSuccess(output)) {
                RA::finish(reply);
            } else {
                RA::fail(reply, Reply::Error::CommandFailed,
                         packageManagerError(output, QStringLiteral("Uninstall failed")));
            }
        });
    });
    return reply;
}

PackagesReply* Client::packages(const QString& serial, bool thirdPartyOnly)
{
    auto* reply = RA::create<PackagesReply>(this);
    detail::runLater(reply, [this, reply, serial, thirdPartyOnly] {
        if (reply->isFinished())
            return;
        QStringList arguments{QStringLiteral("pm"), QStringLiteral("list"), QStringLiteral("packages")};
        if (thirdPartyOnly)
            arguments << QStringLiteral("-3");
        arguments << QStringLiteral("--show-versioncode");
        ShellReply* pm = shell(serial, arguments);
        ClientOps::chain(reply, pm, [=] {
            RA::setOutput(reply, pm->output());
            if (pm->hasExitCode() && pm->exitCode() != 0) {
                RA::fail(reply, Reply::Error::CommandFailed,
                         QStringLiteral("pm list packages failed: %1").arg(QString::fromUtf8(pm->standardError()).trimmed()));
                return;
            }
            RA::setPackages(reply, protocol::parsePackageList(pm->output()));
            RA::finish(reply);
        });
    });
    return reply;
}

PackageInfoReply* Client::packageInfo(const QString& serial, const QString& packageName)
{
    auto* reply = RA::create<PackageInfoReply>(this);
    detail::runLater(reply, [this, reply, serial, packageName] {
        if (reply->isFinished())
            return;
        ShellReply* dump = shell(serial, QStringList{QStringLiteral("dumpsys"), QStringLiteral("package"), packageName});
        ClientOps::chain(reply, dump, [=] {
            RA::setOutput(reply, dump->output());
            const auto info = protocol::parsePackageDump(packageName, dump->output());
            RA::setPackageInfo(reply, info.has_value(), info.value_or(PackageInfo{packageName}));
            RA::finish(reply);
        });
    });
    return reply;
}

StorageReply* Client::storage(const QString& serial, const QString& path)
{
    auto* reply = RA::create<StorageReply>(this);
    detail::runLater(reply, [this, reply, serial, path] {
        if (reply->isFinished())
            return;
        ShellReply* stat = shell(serial, QStringList{QStringLiteral("stat"), QStringLiteral("-f"), QStringLiteral("-c"),
                                                     QStringLiteral("%S %b %a"), path});
        ClientOps::chain(reply, stat, [=] {
            RA::setOutput(reply, stat->output());
            const auto info = protocol::parseStatFs(stat->output());
            if (!info || (stat->hasExitCode() && stat->exitCode() != 0)) {
                RA::fail(reply, Reply::Error::CommandFailed,
                         QStringLiteral("Cannot read the free space of %1: %2")
                             .arg(path, QString::fromUtf8(stat->output() + stat->standardError()).trimmed()));
                return;
            }
            RA::setStorage(reply, *info);
            RA::finish(reply);
        });
    });
    return reply;
}

Reply* Client::getProperty(const QString& serial, const QString& name)
{
    auto* reply = RA::create<Reply>(this);
    detail::runLater(reply, [this, reply, serial, name] {
        if (reply->isFinished())
            return;
        ShellReply* prop = shell(serial, QStringList{QStringLiteral("getprop"), name});
        ClientOps::chain(reply, prop, [=] {
            RA::setOutput(reply, prop->output().trimmed());
            RA::finish(reply);
        });
    });
    return reply;
}

} // namespace qadb
