#include "installer/KeepAwake.h"

#include <qadb/Client.h>

#include <QEventLoop>
#include <QList>
#include <QPointer>
#include <QSet>

namespace installer {

namespace {
// Meta Quest Developer Hub's override lapses about 10 minutes after its last renewal.
constexpr int kDefaultRenewMsec = 60 * 1000;

// The started KeepAwake objects, for releaseAll(). Used from the GUI thread only.
QSet<KeepAwake*>& activeKeepAwakes()
{
    static QSet<KeepAwake*> active;
    return active;
}
} // namespace

bool isProximityOverridden(const QByteArray& dumpsysOutput)
{
    const QByteArray key = QByteArrayLiteral("Virtual proximity state:");
    const qsizetype at = dumpsysOutput.indexOf(key);
    if (at < 0)
        return false;
    qsizetype end = dumpsysOutput.indexOf('\n', at);
    if (end < 0)
        end = dumpsysOutput.size();
    return dumpsysOutput.mid(at + key.size(), end - at - key.size()).trimmed() == "CLOSE";
}

KeepAwake::KeepAwake(qadb::Client* client, const QString& serial, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_serial(serial)
{
    m_timer.setInterval(kDefaultRenewMsec);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_state == State::Holding)
            broadcast(kKeepAwakeAction);
        else if (m_state == State::Deferring)
            check();
    });
}

KeepAwake::~KeepAwake()
{
    stop();
    activeKeepAwakes().remove(this);
}

void KeepAwake::releaseAll(int waitMsecs)
{
    QList<QPointer<qadb::Reply>> replies;
    const QSet<KeepAwake*> active = activeKeepAwakes();
    for (KeepAwake* keepAwake : active) {
        if (qadb::Reply* reply = keepAwake->stop())
            replies.append(reply);
    }
    if (replies.isEmpty())
        return;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    auto quitWhenAnswered = [&loop, &replies] {
        for (const QPointer<qadb::Reply>& reply : std::as_const(replies)) {
            if (reply && !reply->isFinished())
                return;
        }
        loop.quit();
    };
    for (const QPointer<qadb::Reply>& reply : std::as_const(replies))
        connect(reply, &qadb::Reply::finished, &loop, quitWhenAnswered, Qt::QueuedConnection);
    timeout.start(waitMsecs);
    loop.exec();
}

void KeepAwake::start()
{
    if (isActive())
        return;
    m_reportedFailure = false;
    activeKeepAwakes().insert(this);
    setState(State::Checking);
    check();
    m_timer.start();
}

qadb::Reply* KeepAwake::stop()
{
    if (!isActive())
        return nullptr;
    const bool holding = m_state == State::Holding;
    abandon();
    return holding ? broadcast(kReleaseAwakeAction) : nullptr;
}

void KeepAwake::abandon()
{
    m_timer.stop();
    ++m_generation;
    activeKeepAwakes().remove(this);
    setState(State::Off);
}

void KeepAwake::check()
{
    if (!m_client)
        return;
    auto* reply = m_client->shell(m_serial, {QStringLiteral("dumpsys"), QStringLiteral("vrpowermanager")});
    QPointer<KeepAwake> self(this);
    const int generation = m_generation;
    connect(reply, &qadb::Reply::finished, reply, [self, reply, generation] {
        reply->deleteLater();
        if (!self || self->m_generation != generation || self->m_state == State::Holding || !self->isActive())
            return;
        // A failed read (not a Quest, or an older system) counts as no override.
        if (reply->isSuccess() && isProximityOverridden(reply->output())) {
            self->setState(State::Deferring);
            return;
        }
        self->setState(State::Holding);
        self->broadcast(kKeepAwakeAction);
    });
}

void KeepAwake::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged(state);
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
