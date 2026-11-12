#include "installer/KeepAwake.h"

#include <qadb/Client.h>

#include <QPointer>

namespace installer {

namespace {
// Meta Quest Developer Hub's override lapses about 10 minutes after its last renewal.
constexpr int kDefaultRenewMsec = 60 * 1000;
} // namespace

KeepAwake::KeepAwake(qadb::Client* client, const QString& serial, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_serial(serial)
{
    m_timer.setInterval(kDefaultRenewMsec);
    connect(&m_timer, &QTimer::timeout, this, [this] { broadcast(kKeepAwakeAction); });
}

KeepAwake::~KeepAwake()
{
    stop();
}

void KeepAwake::start()
{
    if (m_active)
        return;
    m_active = true;
    m_reportedFailure = false;
    broadcast(kKeepAwakeAction);
    m_timer.start();
}

qadb::Reply* KeepAwake::stop()
{
    if (!m_active)
        return nullptr;
    m_active = false;
    m_timer.stop();
    return broadcast(kReleaseAwakeAction);
}

void KeepAwake::abandon()
{
    m_active = false;
    m_timer.stop();
}

qadb::Reply* KeepAwake::broadcast(const char* action)
{
    if (!m_client)
        return nullptr;
    auto* reply = m_client->shell(m_serial, {QStringLiteral("am"), QStringLiteral("broadcast"), QStringLiteral("-a"),
                                             QString::fromLatin1(action)});
    // The reply belongs to the client, so the release still goes out when this object is gone.
    QPointer<KeepAwake> self(this);
    connect(reply, &qadb::Reply::finished, reply, [self, reply] {
        reply->deleteLater();
        if (!self || self->m_reportedFailure)
            return;
        QString error;
        if (!reply->isSuccess())
            error = reply->errorString();
        else if (reply->hasExitCode() && reply->exitCode() != 0)
            error = QString::fromUtf8(reply->standardError()).trimmed();
        if (error.isNull())
            return;
        self->m_reportedFailure = true;
        emit self->failed(error.isEmpty() ? tr("am broadcast failed") : error);
    });
    return reply;
}

} // namespace installer
