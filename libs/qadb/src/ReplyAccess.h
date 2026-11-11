#pragma once

#include "qadb/Reply.h"

#include <QMetaObject>

#include <functional>
#include <utility>

namespace qadb::detail {

// The library's write access to replies. Not part of the public API.
struct ReplyAccess
{
    template <typename T>
    static T* create(QObject* parent)
    {
        return new T(parent);
    }

    static void setOutput(Reply* reply, const QByteArray& output) { reply->m_output = output; }
    static void setAbortHandler(Reply* reply, std::function<void()> handler)
    {
        reply->m_abortHandler = std::move(handler);
    }

    static void finish(Reply* reply)
    {
        if (reply->m_finished)
            return;
        reply->m_finished = true;
        reply->m_abortHandler = {};
        emit reply->finished();
    }

    static void fail(Reply* reply, Reply::Error error, const QString& message)
    {
        if (reply->m_finished)
            return;
        reply->m_error = error;
        reply->m_errorString = message;
        finish(reply);
    }

    // Stops the request through its abort handler, then fails it.
    static void cancel(Reply* reply, Reply::Error error, const QString& message)
    {
        if (reply->m_finished)
            return;
        const auto handler = std::exchange(reply->m_abortHandler, {});
        fail(reply, error, message);
        if (handler)
            handler();
    }

    static void progress(Reply* reply, qint64 done, qint64 total)
    {
        if (!reply->m_finished)
            emit reply->progress(done, total);
    }

    static void setDevices(DevicesReply* reply, const QList<DeviceInfo>& devices)
    {
        reply->m_devices = devices;
    }
    static void setServices(MdnsReply* reply, const QList<MdnsService>& services)
    {
        reply->m_services = services;
    }
    static void setFeatures(FeaturesReply* reply, const QStringList& features)
    {
        reply->m_features = features;
    }
    static void setShellResult(ShellReply* reply, const QByteArray& standardError, int exitCode)
    {
        reply->m_stderr = standardError;
        reply->m_exitCode = exitCode;
    }
    static void setInfo(StatReply* reply, const RemoteFileInfo& info) { reply->m_info = info; }
    static void setEntries(ListReply* reply, const QList<RemoteFileInfo>& entries)
    {
        reply->m_entries = entries;
    }
};

// Runs `function` on the next event-loop iteration, unless `context` is deleted first.
template <typename F>
void runLater(QObject* context, F&& function)
{
    QMetaObject::invokeMethod(context, std::forward<F>(function), Qt::QueuedConnection);
}

} // namespace qadb::detail
