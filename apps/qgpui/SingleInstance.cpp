#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QThread>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

// One name per user, so that two users on one PC each get their own QGPUI. Windows named pipes are
// visible to every user, hence the user name in it; hashing keeps it free of odd characters.
QString instanceName()
{
    QString user = qEnvironmentVariable("USERNAME");
    if (user.isEmpty())
        user = qEnvironmentVariable("USER");
    const QByteArray hash = QCryptographicHash::hash(user.toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return QStringLiteral("qgpui-") + QString::fromLatin1(hash);
}

QString lockPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(instanceName() + QStringLiteral(".lock"));
}

// Tells the running instance that QGPUI was started again. It may still be starting, so keep trying
// for a while.
void notifyRunningInstance()
{
#ifdef Q_OS_WIN
    // Windows lets only the foreground process bring a window to the front. This process was just
    // started by the user and may pass that right on.
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    const QDeadlineTimer deadline(3000);
    while (!deadline.hasExpired()) {
        QLocalSocket socket;
        socket.connectToServer(instanceName());
        if (socket.waitForConnected(500)) {
            socket.disconnectFromServer();
            if (socket.state() != QLocalSocket::UnconnectedState)
                socket.waitForDisconnected(1000);
            return;
        }
        QThread::msleep(100);
    }
}

} // namespace

SingleInstance::SingleInstance(QObject* parent)
    : QObject(parent)
    , m_lock(lockPath())
{
    // A lock is stale only when its process is gone, never because it is old.
    m_lock.setStaleLockTime(0);
}

SingleInstance::~SingleInstance() = default;

bool SingleInstance::claim()
{
    if (!m_lock.tryLock(0)) {
        if (m_lock.error() == QLockFile::LockFailedError) {
            notifyRunningInstance();
            return false;
        }
        // The lock file cannot be written at all. Running twice is better than not running.
        return true;
    }

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    // On Unix a crashed instance leaves its socket file behind; the lock says it is gone.
    QLocalServer::removeServer(instanceName());
    if (!m_server->listen(instanceName()))
        qWarning("qgpui: cannot listen for other instances: %s", qPrintable(m_server->errorString()));
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::readConnection);
    return true;
}

void SingleInstance::readConnection()
{
    while (QLocalSocket* socket = m_server->nextPendingConnection()) {
        // The other instance connects and disconnects; act once it has.
        const auto finish = [this, socket] {
            socket->disconnect(this);
            socket->deleteLater();
            emit activated();
        };
        if (socket->state() == QLocalSocket::UnconnectedState)
            finish();
        else
            connect(socket, &QLocalSocket::disconnected, this, finish);
    }
}
