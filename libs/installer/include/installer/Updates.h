#pragma once

#include <qadb/Types.h>

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace installer {

// --- Versions ---------------------------------------------------------------------------------

// The version numbers in a release tag or versionName: the longest dotted number in it, so
// "vcop2-v0.1.0" gives {0, 1, 0} and "quest-v0.3-beta" gives {0, 3}. A label with only a lone
// number, such as "b003" or "n64", gives nothing, because such numbers are build or platform
// names as often as versions.
QList<int> versionNumbers(const QString& label);

enum class VersionOrder { Older, Same, Newer, Unknown };

// How release tag `a` compares with version label `b`, by their version numbers. Missing parts
// count as 0, so 1.2 and 1.2.0 are the same. Unknown when either has no version numbers.
// Pre-release suffixes are ignored: 0.3-beta and 0.3 are the same.
VersionOrder compareVersions(const QString& a, const QString& b);

// --- Install records --------------------------------------------------------------------------

// What qgp installed on one headset, for one package.
struct InstallRecord
{
    QString packageName;
    QString slug;
    QString releaseTag; // Empty for an APK from a URL or picked by the user.
    QString asset;
    qint64 versionCode = -1; // As the headset reported it right after the install.
    QString versionName;
    QDateTime installedAt;
};

// The install records of every headset, keyed by the headset's hardware serial (ro.serialno),
// which stays the same over USB and Wi-Fi. Stored as one JSON file.
class InstallRecords
{
public:
    // Reads the file. A missing file gives an empty set; a file that cannot be parsed gives an
    // empty set and sets *errorMessage when it is given.
    static InstallRecords load(const QString& path, QString* errorMessage = nullptr);
    // Writes the file atomically, creating its folder if needed. Returns false, and sets
    // *errorMessage, on failure.
    bool save(QString* errorMessage = nullptr) const;

    QString path() const { return m_path; }
    std::optional<InstallRecord> find(const QString& hardwareSerial, const QString& packageName) const;
    QList<InstallRecord> records(const QString& hardwareSerial) const;
    void set(const QString& hardwareSerial, const InstallRecord& record);
    void remove(const QString& hardwareSerial, const QString& packageName);

private:
    QString m_path;
    QHash<QString, QHash<QString, InstallRecord>> m_records;
};

// --- Records on the headset -------------------------------------------------------------------

// The file on each headset that lists what qgp installed there. It is the source of truth; the
// PC's InstallRecords are a cache of it for when no headset is connected.
inline constexpr auto kHeadsetRecordsPath = "/sdcard/qgp/installs.json";

// The headset file's content: {"format":"qgp-headset-installs","version":1,"serial":...,
// "packages":{<package>: <record>}}.
QByteArray headsetRecordsJson(const QString& hardwareSerial, const QList<InstallRecord>& records);

// Reads the headset file. Returns nothing, and sets *errorMessage when it is given, when the
// content is not such a file.
std::optional<QList<InstallRecord>> parseHeadsetRecords(const QByteArray& json, QString* errorMessage = nullptr);

// Joins two sets of records for one headset: one record per package, the one installed last
// when both have it. Sorted by package name.
QList<InstallRecord> mergeRecords(const QList<InstallRecord>& a, const QList<InstallRecord>& b);

// --- Update status ----------------------------------------------------------------------------

enum class UpdateStatus {
    NotInstalled,
    UpToDate,
    UpdateAvailable,
    Unknown, // Installed, but its version cannot be compared with the release.
};

// Whether the newest release (latestTag, as pickRelease() chose it) is newer than what the
// headset has.
// - Not installed: NotInstalled.
// - qgp installed it and the app is unchanged since (same versionCode): the same tag is
//   UpToDate; any other tag is UpdateAvailable, because pickRelease() takes the newest release.
// - Otherwise (installed elsewhere, or the app updated itself): compares latestTag with the
//   installed versionName.
UpdateStatus updateStatus(const std::optional<qadb::PackageInfo>& installed,
                          const std::optional<InstallRecord>& record, const QString& latestTag);

} // namespace installer
