#include "qadb/Reply.h"

#include "ReplyAccess.h"

#include <QEventLoop>
#include <QTimer>

namespace qadb {

Reply::Reply(QObject* parent)
    : QObject(parent)
{
}

Reply::~Reply() = default;

void Reply::abort()
{
    detail::ReplyAccess::cancel(this, Error::Aborted, QStringLiteral("Aborted"));
}

bool Reply::waitForFinished(int msecs)
{
    if (m_finished)
        return true;

    QEventLoop loop;
    connect(this, &Reply::finished, &loop, &QEventLoop::quit);
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    if (msecs >= 0)
        timer.start(msecs);
    loop.exec();

    if (m_finished)
        return true;
    detail::ReplyAccess::cancel(this, Error::Timeout,
                                QStringLiteral("Timed out after %1 ms").arg(msecs));
    return false;
}

} // namespace qadb
