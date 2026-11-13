#pragma once

#include "installer/Updates.h"

#include <QList>
#include <QObject>
#include <QString>

#include <optional>

namespace qadb {
class Client;
}

namespace installer {

// Keeps a headset's install list (kHeadsetRecordsPath) and the PC's cache of it in step.
//
// run() reads the headset's list, joins it with the cache's records for that headset (one record
// per package, the one installed last) and, when given, a record to add, which always replaces
// the package's old record. remove() does the same but drops a package's record instead, after
// the package was uninstalled. Either writes the result to the headset when it differs from what was
// there, then to the cache, and saves the cache. A list on the headset that cannot be parsed is
// never overwritten: run() fails instead. Without a headset run() fails like any device request;
// the cache keeps its last known state.
class HeadsetRecordSync : public QObject
{
    Q_OBJECT

public:
    HeadsetRecordSync(qadb::Client* client, const QString& adbSerial, const QString& hardwareSerial,
                      InstallRecords* cache, QObject* parent = nullptr);

    void run(const std::optional<InstallRecord>& added = std::nullopt);
    void remove(const QString& packageName);

    // The joined records, once finished() reported success.
    QList<InstallRecord> records() const { return m_records; }

signals:
    void finished(bool success, const QString& errorMessage);

private:
    void start();
    void merge(const QList<InstallRecord>& onHeadset);
    void saveCache();

    qadb::Client* m_client;
    QString m_adbSerial;
    QString m_hardwareSerial;
    InstallRecords* m_cache;
    std::optional<InstallRecord> m_added;
    QString m_removed;
    QList<InstallRecord> m_records;
};

} // namespace installer
