#include "Channel.h"

#include "qadb/Protocol.h"

#include <QPointer>
#include <QTcpSocket>

#include <utility>

namespace qadb::detail {

Channel::Channel(const ServerAddress& server, QObject* parent)
    : QObject(parent)
    , m_server(server)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, [this] {
        m_connected = true;
        m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        if (auto handler = std::exchange(m_onConnected, {}))
            handler();
    });
    connect(m_socket, &QTcpSocket::readyRead, this, &Channel::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &Channel::onPeerClosed);
    connect(m_socket, &QTcpSocket::bytesWritten, this, [this] {
        if (!m_done)
            emit bytesWritten();
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        if (m_done)
            return;
        if (error == QAbstractSocket::RemoteHostClosedError) {
            onPeerClosed();
            return;
        }
        if (!m_connected) {
            fail(Reply::Error::ConnectionFailed,
                 QStringLiteral("Cannot reach the adb server at %1:%2: %3")
                     .arg(m_server.host)
                     .arg(m_server.port)
                     .arg(m_socket->errorString()));
        } else {
            fail(Reply::Error::ConnectionFailed, m_socket->errorString());
        }
    });
}

Channel::~Channel()
{
    // The channel may be destroyed from inside one of the socket's own signals, for example when
    // a progress handler deletes its reply. Deleting the socket there would crash once the
    // signal returns, so detach it and let the event loop delete it.
    m_socket->disconnect(this);
    m_socket->setParent(nullptr);
    m_socket->abort();
    m_socket->deleteLater();
}

void Channel::open(DoneHandler onConnected)
{
    m_onConnected = std::move(onConnected);
    m_socket->connectToHost(m_server.host, m_server.port);
}

void Channel::sendRequest(const QByteArray& payload)
{
    write(protocol::encodeRequest(payload));
}

void Channel::write(const QByteArray& data)
{
    if (m_done)
        return;
    m_socket->write(data);
}

qint64 Channel::bytesToWrite() const
{
    return m_socket->bytesToWrite();
}

void Channel::readExact(qint64 count, BytesHandler handler)
{
    if (m_done)
        return;
    Q_ASSERT(m_pending == Pending::None);
    m_pending = Pending::Exact;
    m_need = count;
    m_onBytes = std::move(handler);
    process();
}

void Channel::readStatus(DoneHandler onOkay)
{
    readExact(4, [this, onOkay = std::move(onOkay)](const QByteArray& status) {
        if (status == "OKAY") {
            onOkay();
        } else if (status == "FAIL") {
            readHexString([this](const QByteArray& message) {
                fail(Reply::Error::ServerError, QString::fromUtf8(message));
            });
        } else {
            fail(Reply::Error::ProtocolError,
                 QStringLiteral("Unexpected answer from the adb server: %1")
                     .arg(QString::fromLatin1(status.toHex())));
        }
    });
}

void Channel::readHexString(BytesHandler handler)
{
    readExact(4, [this, handler = std::move(handler)](const QByteArray& lengthText) {
        const int length = protocol::parseHexLength(lengthText);
        if (length < 0) {
            fail(Reply::Error::ProtocolError,
                 QStringLiteral("Invalid length from the adb server: %1")
                     .arg(QString::fromLatin1(lengthText.toHex())));
            return;
        }
        readExact(length, handler);
    });
}

void Channel::readUntilClosed(BytesHandler onData, DoneHandler onClosed)
{
    if (m_done)
        return;
    Q_ASSERT(m_pending == Pending::None);
    m_pending = Pending::Stream;
    m_onBytes = std::move(onData);
    m_onClosed = std::move(onClosed);
    process();
}

void Channel::fail(Reply::Error error, const QString& message)
{
    if (m_done)
        return;
    m_done = true;
    clearHandlers();
    m_socket->abort();
    emit failed(error, message);
}

void Channel::close()
{
    if (m_done)
        return;
    m_done = true;
    clearHandlers();
    m_socket->disconnectFromHost();
}

void Channel::abort()
{
    if (m_done)
        return;
    m_done = true;
    clearHandlers();
    m_socket->abort();
}

void Channel::clearHandlers()
{
    m_pending = Pending::None;
    m_onBytes = {};
    m_onClosed = {};
    m_onConnected = {};
}

void Channel::onReadyRead()
{
    m_buffer += m_socket->readAll();
    process();
}

void Channel::onPeerClosed()
{
    if (m_peerClosed || m_done)
        return;
    if (m_socket->isOpen())
        m_buffer += m_socket->readAll();
    m_peerClosed = true;
    process();
}

void Channel::process()
{
    if (m_processing)
        return;
    m_processing = true;
    QPointer<Channel> self(this);

    while (!m_done) {
        if (m_pending == Pending::Exact) {
            if (m_buffer.size() >= m_need) {
                const QByteArray data = m_buffer.left(m_need);
                m_buffer.remove(0, m_need);
                m_pending = Pending::None;
                const BytesHandler handler = std::exchange(m_onBytes, {});
                handler(data);
                if (!self)
                    return;
                continue;
            }
            if (m_peerClosed) {
                fail(Reply::Error::ProtocolError,
                     QStringLiteral("The adb server closed the connection before the answer "
                                    "was complete"));
                if (!self)
                    return;
            }
            break;
        }
        if (m_pending == Pending::Stream) {
            if (!m_buffer.isEmpty()) {
                const QByteArray data = std::exchange(m_buffer, {});
                const BytesHandler handler = m_onBytes;
                handler(data);
                if (!self)
                    return;
                continue;
            }
            if (m_peerClosed) {
                const DoneHandler handler = std::exchange(m_onClosed, {});
                m_done = true;
                clearHandlers();
                handler();
                if (!self)
                    return;
            }
            break;
        }
        break;
    }
    m_processing = false;
}

} // namespace qadb::detail
