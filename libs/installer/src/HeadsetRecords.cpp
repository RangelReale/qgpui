#include "installer/HeadsetRecords.h"

#include <qadb/Client.h>
#include <qadb/Reply.h>

#include <QFile>
#include <QTemporaryDir>

#include <memory>

namespace installer {

namespace {

QString remotePath()
{
    return QString::fromLatin1(kHeadsetRecordsPath);
}

} // namespace

HeadsetRecordSync::HeadsetRecordSync(qadb::Client* client, const QString& adbSerial, const QString& hardwareSerial,
                                     InstallRecords* cache, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_adbSerial(adbSerial)
    , m_hardwareSerial(hardwareSerial)
    , m_cache(cache)
{}

void HeadsetRecordSync::run(const std::optional<InstallRecord>& added)
{
    m_added = added;
    m_removed.clear();
    start();
}

void HeadsetRecordSync::remove(const QString& packageName)
{
    m_added.reset();
    m_removed = packageName;
    start();
}

void HeadsetRecordSync::start()
{
    m_records.clear();
    auto* stat = m_client->stat(m_adbSerial, remotePath());
    connect(stat, &qadb::Reply::finished, this, [this, stat] {
        stat->deleteLater();
        if (!stat->isSuccess()) {
            emit finished(false, stat->errorString());
            return;
        }
        if (!stat->info().exists()) {
            merge({});
            return;
        }
        auto dir = std::make_shared<QTemporaryDir>();
        const QString local = dir->filePath(QStringLiteral("installs.json"));
        auto* pull = m_client->pull(m_adbSerial, remotePath(), local);
        connect(pull, &qadb::Reply::finished, this, [this, pull, dir, local] {
            pull->deleteLater();
            if (!pull->isSuccess()) {
                emit finished(false, pull->errorString());
                return;
            }
            QFile file(local);
            if (!file.open(QIODevice::ReadOnly)) {
                emit finished(false, file.errorString());
                return;
            }
            QString error;
            const auto onHeadset = parseHeadsetRecords(file.readAll(), &error);
            if (!onHeadset) {
                emit finished(false, error);
                return;
            }
            merge(*onHeadset);
        });
    });
}

void HeadsetRecordSync::merge(const QList<InstallRecord>& onHeadset)
{
    QList<InstallRecord> merged = mergeRecords(onHeadset, m_cache->records(m_hardwareSerial));
    if (m_added) {
        merged.removeIf([this](const InstallRecord& r) { return r.packageName == m_added->packageName; });
        merged = mergeRecords(merged, {*m_added});
    }
    if (!m_removed.isEmpty())
        merged.removeIf([this](const InstallRecord& r) { return r.packageName == m_removed; });
    m_records = merged;

    const QByteArray before = headsetRecordsJson(m_hardwareSerial, mergeRecords(onHeadset, {}));
    const QByteArray after = headsetRecordsJson(m_hardwareSerial, merged);
    if (before == after) {
        saveCache();
        return;
    }
    // Outside Android/data, adb creates /sdcard/qgpui itself and the file is ordinary shared storage.
    auto* push = m_client->pushData(m_adbSerial, after, remotePath(), 0666);
    connect(push, &qadb::Reply::finished, this, [this, push] {
        push->deleteLater();
        if (!push->isSuccess()) {
            emit finished(false, push->errorString());
            return;
        }
        saveCache();
    });
}

void HeadsetRecordSync::saveCache()
{
    if (!m_removed.isEmpty())
        m_cache->remove(m_hardwareSerial, m_removed);
    for (const InstallRecord& r : std::as_const(m_records))
        m_cache->set(m_hardwareSerial, r);
    QString error;
    if (!m_cache->save(&error)) {
        emit finished(false, error);
        return;
    }
    emit finished(true, {});
}

} // namespace installer
