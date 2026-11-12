#pragma once

#include "qadb/Types.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace qadb {

namespace detail {
struct ReplyAccess;
}

// The result of one asynchronous request, in the style of QNetworkReply.
//
// Requests start on the next event-loop iteration, so connect to finished() right after the call
// that returned the reply. The reply belongs to the caller: delete it, or call deleteLater() from
// the finished() handler.
class Reply : public QObject
{
    Q_OBJECT

public:
    enum class Error {
        None,
        ConnectionFailed, // The adb server could not be reached.
        ServerError,      // The server or device answered FAIL; errorString() holds its message.
        ProtocolError,    // An unexpected answer, or the connection closed too early.
        CommandFailed,    // The request ran but reported failure (install, connect, exit code).
        FileError,        // A local file could not be read or written.
        Aborted,
        Timeout,
    };
    Q_ENUM(Error)

    ~Reply() override;

    bool isFinished() const { return m_finished; }
    bool isSuccess() const { return m_finished && m_error == Error::None; }
    Error error() const { return m_error; }
    QString errorString() const { return m_errorString; }

    // The request's main output: the text a host query returned, or the output of a command.
    QByteArray output() const { return m_output; }
    QString outputText() const { return QString::fromUtf8(m_output); }

    // Stops the request. finished() is emitted with Error::Aborted unless it already finished.
    void abort();

    // Runs a local event loop until the reply finishes. Returns false on timeout, after aborting
    // the request with Error::Timeout. Meant for tools and tests; GUI code should use finished().
    bool waitForFinished(int msecs = 30000);

signals:
    void finished();
    // Bytes transferred so far and the total, or -1 when the total is unknown.
    void progress(qint64 done, qint64 total);

protected:
    explicit Reply(QObject* parent = nullptr);

private:
    friend struct detail::ReplyAccess;

    bool m_finished = false;
    Error m_error = Error::None;
    QString m_errorString;
    QByteArray m_output;
    std::function<void()> m_abortHandler;
};

// host:devices-l
class DevicesReply : public Reply
{
    Q_OBJECT
public:
    QList<DeviceInfo> devices() const { return m_devices; }

private:
    friend struct detail::ReplyAccess;
    using Reply::Reply;
    QList<DeviceInfo> m_devices;
};

// host:mdns:services
class MdnsReply : public Reply
{
    Q_OBJECT
public:
    QList<MdnsService> services() const { return m_services; }

private:
    friend struct detail::ReplyAccess;
    using Reply::Reply;
    QList<MdnsService> m_services;
};

// <host-prefix>:features
class FeaturesReply : public Reply
{
    Q_OBJECT
public:
    QStringList features() const { return m_features; }
    bool hasFeature(const QString& name) const { return m_features.contains(name); }

private:
    friend struct detail::ReplyAccess;
    using Reply::Reply;
    QStringList m_features;
};

// A shell command. output() holds stdout.
//
// With the device's shell protocol v2 (Android 7 and later, including every Quest), stderr is
// separate and the exit code is known. Without it, stderr is mixed into stdout and exitCode() is -1.
// A non-zero exit code is not an error: check exitCode() yourself.
class ShellReply : public Reply
{
    Q_OBJECT
public:
    QByteArray standardError() const { return m_stderr; }
    int exitCode() const { return m_exitCode; }
    bool hasExitCode() const { return m_exitCode >= 0; }

private:
    friend struct detail::ReplyAccess;
    using Reply::Reply;
    QByteArray m_stderr;
    int m_exitCode = -1;
};

// The sync STAT request.
class StatReply : public Reply
{
    Q_OBJECT
public:
    // info().exists() is false when the path does not exist; that is not an error.
    RemoteFileInfo info() const { return m_info; }

private:
    friend struct detail::ReplyAccess;
    using Reply::Reply;
    RemoteFileInfo m_info;
};

// The installed packages, from `pm list packages --show-versioncode`.
class PackagesReply : public Reply
{
    Q_OBJECT
public:
    QList<PackageInfo> packages() const { return m_packages; }

private:
    friend struct detail::ReplyAccess;
    using Reply::Reply;
    QList<PackageInfo> m_packages;
};

// One package, from `dumpsys package`. A package that is not installed is not an error:
// isInstalled() is false.
class PackageInfoReply : public Reply
{
    Q_OBJECT
public:
    bool isInstalled() const { return m_installed; }
    PackageInfo info() const { return m_info; }

private:
    friend struct detail::ReplyAccess;
    using Reply::Reply;
    bool m_installed = false;
    PackageInfo m_info;
};

// A file system's size, from `stat -f`.
class StorageReply : public Reply
{
    Q_OBJECT
public:
    StorageInfo info() const { return m_info; }

private:
    friend struct detail::ReplyAccess;
    using Reply::Reply;
    StorageInfo m_info;
};

// The sync LIST request. The "." and ".." entries are left out.
class ListReply : public Reply
{
    Q_OBJECT
public:
    QList<RemoteFileInfo> entries() const { return m_entries; }

private:
    friend struct detail::ReplyAccess;
    using Reply::Reply;
    QList<RemoteFileInfo> m_entries;
};

} // namespace qadb
