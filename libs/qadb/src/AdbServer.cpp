#include "qadb/AdbServer.h"

#include "ReplyAccess.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace qadb {

using RA = detail::ReplyAccess;

namespace {

QString executableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("adb.exe");
#else
    return QStringLiteral("adb");
#endif
}

QString inPlatformTools(const QString& sdk)
{
    if (sdk.isEmpty())
        return {};
    const QString path = QDir(sdk).filePath(QStringLiteral("platform-tools/") + executableName());
    return QFileInfo(path).isExecutable() ? QDir::toNativeSeparators(path) : QString();
}

} // namespace

QString AdbServer::findAdb()
{
    const QString fromEnv = qEnvironmentVariable("ADB");
    if (!fromEnv.isEmpty() && QFileInfo(fromEnv).isExecutable())
        return QDir::toNativeSeparators(fromEnv);

    // A copy shipped with the application.
    if (QCoreApplication::instance()) {
        const QString bundled = inPlatformTools(QCoreApplication::applicationDirPath());
        if (!bundled.isEmpty())
            return bundled;
    }

    for (const char* variable : {"ANDROID_HOME", "ANDROID_SDK_ROOT"}) {
        const QString path = inPlatformTools(qEnvironmentVariable(variable));
        if (!path.isEmpty())
            return path;
    }

    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("adb"));
    if (!onPath.isEmpty())
        return QDir::toNativeSeparators(onPath);

#if defined(Q_OS_WIN)
    const QString sdk = qEnvironmentVariable("LOCALAPPDATA") + QStringLiteral("/Android/Sdk");
#elif defined(Q_OS_MACOS)
    const QString sdk = QDir::homePath() + QStringLiteral("/Library/Android/sdk");
#else
    const QString sdk = QDir::homePath() + QStringLiteral("/Android/Sdk");
#endif
    return inPlatformTools(sdk);
}

Reply* AdbServer::start(const QString& adbPath, const ServerAddress& server, QObject* parent)
{
    auto* reply = RA::create<Reply>(parent);
    detail::runLater(reply, [reply, adbPath, server] {
        if (reply->isFinished())
            return;
        if (adbPath.isEmpty()) {
            RA::fail(reply, Reply::Error::FileError, QStringLiteral("adb was not found"));
            return;
        }

        auto* process = new QProcess(reply);
        process->setProcessChannelMode(QProcess::MergedChannels);
        if (server.port != ServerAddress::kDefaultPort) {
            QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
            environment.insert(QStringLiteral("ANDROID_ADB_SERVER_PORT"),
                               QString::number(server.port));
            process->setProcessEnvironment(environment);
        }

        QPointer<QProcess> guard(process);
        RA::setAbortHandler(reply, [guard] {
            if (guard)
                guard->kill();
        });
        QObject::connect(process, &QProcess::errorOccurred, reply,
                         [reply, process, adbPath](QProcess::ProcessError error) {
                             if (error != QProcess::FailedToStart)
                                 return;
                             RA::fail(reply, Reply::Error::FileError,
                                      QStringLiteral("Cannot run %1: %2")
                                          .arg(adbPath, process->errorString()));
                         });
        QObject::connect(process, &QProcess::finished, reply,
                         [reply, process](int exitCode, QProcess::ExitStatus status) {
                             const QByteArray output = process->readAll();
                             RA::setOutput(reply, output);
                             if (status == QProcess::NormalExit && exitCode == 0) {
                                 RA::finish(reply);
                             } else {
                                 const QString text = QString::fromLocal8Bit(output).trimmed();
                                 RA::fail(reply, Reply::Error::CommandFailed,
                                          text.isEmpty()
                                              ? QStringLiteral("adb start-server failed")
                                              : text);
                             }
                         });
        process->start(adbPath, {QStringLiteral("start-server")});
    });
    return reply;
}

} // namespace qadb
