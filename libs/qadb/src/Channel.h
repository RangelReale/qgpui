#pragma once

#include "qadb/Reply.h"
#include "qadb/Types.h"

#include <QByteArray>
#include <QObject>

#include <functional>

class QTcpSocket;

namespace qadb::detail {

// One connection to the adb server, with callback-style reads.
//
// At most one read is pending at a time. Each read's handler usually starts the next read, so a
// request reads like a sequence of steps. Writes are independent of reads. Handlers run from the
// event loop; a handler may delete the channel's owner.
//
// Any error, a FAIL answer to readStatus(), or the connection closing during readExact() ends the
// channel and emits failed() once. close() and abort() end it without a signal.
class Channel : public QObject
{
    Q_OBJECT

public:
    using BytesHandler = std::function<void(const QByteArray&)>;
    using DoneHandler = std::function<void()>;

    Channel(const ServerAddress& server, QObject* parent);
    ~Channel() override;

    void open(DoneHandler onConnected);

    // Writes a host request: 4 hex digits of length, then the payload.
    void sendRequest(const QByteArray& payload);
    void write(const QByteArray& data);
    qint64 bytesToWrite() const;

    void readExact(qint64 count, BytesHandler handler);
    // Reads OKAY, or FAIL and its message, which fails the channel with Reply::Error::ServerError.
    void readStatus(DoneHandler onOkay);
    // Reads 4 hex digits of length, then that many bytes.
    void readHexString(BytesHandler handler);
    // Delivers everything that arrives until the peer closes the connection.
    void readUntilClosed(BytesHandler onData, DoneHandler onClosed);

    void fail(Reply::Error error, const QString& message);
    // Closes after sending what is still buffered.
    void close();
    void abort();
    bool isDone() const { return m_done; }

signals:
    void failed(qadb::Reply::Error error, const QString& message);
    void bytesWritten();

private:
    enum class Pending { None, Exact, Stream };

    void onReadyRead();
    void onPeerClosed();
    void process();
    void clearHandlers();

    ServerAddress m_server;
    QTcpSocket* m_socket = nullptr;
    QByteArray m_buffer;
    Pending m_pending = Pending::None;
    qint64 m_need = 0;
    BytesHandler m_onBytes;
    DoneHandler m_onClosed;
    DoneHandler m_onConnected;
    bool m_connected = false;
    bool m_peerClosed = false;
    bool m_done = false;
    bool m_processing = false;
};

} // namespace qadb::detail
