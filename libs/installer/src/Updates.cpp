#include "installer/Updates.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace installer {

namespace {

constexpr auto kFormat = "qgp-install-records";
constexpr int kVersion = 1;

QJsonObject toJson(const InstallRecord& r)
{
    QJsonObject o;
    o.insert(QStringLiteral("slug"), r.slug);
    o.insert(QStringLiteral("releaseTag"), r.releaseTag);
    o.insert(QStringLiteral("asset"), r.asset);
    o.insert(QStringLiteral("versionCode"), r.versionCode);
    o.insert(QStringLiteral("versionName"), r.versionName);
    o.insert(QStringLiteral("installedAt"), r.installedAt.toUTC().toString(Qt::ISODate));
    return o;
}

InstallRecord fromJson(const QString& packageName, const QJsonObject& o)
{
    InstallRecord r;
    r.packageName = packageName;
    r.slug = o.value(QLatin1StringView("slug")).toString();
    r.releaseTag = o.value(QLatin1StringView("releaseTag")).toString();
    r.asset = o.value(QLatin1StringView("asset")).toString();
    r.versionCode = o.value(QLatin1StringView("versionCode")).toInteger(-1);
    r.versionName = o.value(QLatin1StringView("versionName")).toString();
    r.installedAt = QDateTime::fromString(o.value(QLatin1StringView("installedAt")).toString(), Qt::ISODate);
    return r;
}

} // namespace

QList<int> versionNumbers(const QString& label)
{
    static const QRegularExpression dotted(QStringLiteral("\\d+(?:\\.\\d+)+"));
    QString best;
    for (auto it = dotted.globalMatch(label); it.hasNext();) {
        const QString found = it.next().captured();
        if (found.count(u'.') > best.count(u'.'))
            best = found;
    }
    QList<int> numbers;
    if (best.isEmpty())
        return numbers;
    for (const QString& part : best.split(u'.'))
        numbers.append(part.toInt());
    return numbers;
}

VersionOrder compareVersions(const QString& a, const QString& b)
{
    const QList<int> x = versionNumbers(a);
    const QList<int> y = versionNumbers(b);
    if (x.isEmpty() || y.isEmpty())
        return VersionOrder::Unknown;
    for (qsizetype i = 0; i < std::max(x.size(), y.size()); ++i) {
        const int p = x.value(i, 0);
        const int q = y.value(i, 0);
        if (p != q)
            return p < q ? VersionOrder::Older : VersionOrder::Newer;
    }
    return VersionOrder::Same;
}

InstallRecords InstallRecords::load(const QString& path, QString* errorMessage)
{
    InstallRecords records;
    records.m_path = path;
    QFile file(path);
    if (!file.exists())
        return records;
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot read %1: %2").arg(path, file.errorString());
        return records;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value(QLatin1StringView("format")).toString() != QLatin1StringView(kFormat)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("%1 is not a qgp install record file.").arg(path);
        return records;
    }
    const QJsonObject headsets = root.value(QLatin1StringView("headsets")).toObject();
    for (auto h = headsets.begin(); h != headsets.end(); ++h) {
        const QJsonObject packages = h.value().toObject();
        for (auto p = packages.begin(); p != packages.end(); ++p)
            records.m_records[h.key()].insert(p.key(), fromJson(p.key(), p.value().toObject()));
    }
    return records;
}

bool InstallRecords::save(QString* errorMessage) const
{
    QJsonObject headsets;
    for (auto h = m_records.cbegin(); h != m_records.cend(); ++h) {
        QJsonObject packages;
        for (auto p = h->cbegin(); p != h->cend(); ++p)
            packages.insert(p.key(), toJson(*p));
        headsets.insert(h.key(), packages);
    }
    QJsonObject root;
    root.insert(QStringLiteral("format"), QLatin1StringView(kFormat));
    root.insert(QStringLiteral("version"), kVersion);
    root.insert(QStringLiteral("headsets"), headsets);

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot write %1: %2").arg(m_path, file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson());
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot write %1: %2").arg(m_path, file.errorString());
        return false;
    }
    return true;
}

std::optional<InstallRecord> InstallRecords::find(const QString& hardwareSerial, const QString& packageName) const
{
    const auto headset = m_records.constFind(hardwareSerial);
    if (headset == m_records.cend())
        return std::nullopt;
    const auto record = headset->constFind(packageName);
    if (record == headset->cend())
        return std::nullopt;
    return *record;
}

void InstallRecords::set(const QString& hardwareSerial, const InstallRecord& record)
{
    m_records[hardwareSerial].insert(record.packageName, record);
}

void InstallRecords::remove(const QString& hardwareSerial, const QString& packageName)
{
    auto headset = m_records.find(hardwareSerial);
    if (headset == m_records.end())
        return;
    headset->remove(packageName);
    if (headset->isEmpty())
        m_records.erase(headset);
}

UpdateStatus updateStatus(const std::optional<qadb::PackageInfo>& installed, const std::optional<InstallRecord>& record,
                          const QString& latestTag)
{
    if (!installed)
        return UpdateStatus::NotInstalled;
    if (record && !record->releaseTag.isEmpty() && record->versionCode == installed->versionCode)
        return record->releaseTag == latestTag ? UpdateStatus::UpToDate : UpdateStatus::UpdateAvailable;
    switch (compareVersions(latestTag, installed->versionName)) {
    case VersionOrder::Newer:
        return UpdateStatus::UpdateAvailable;
    case VersionOrder::Same:
    case VersionOrder::Older:
        return UpdateStatus::UpToDate;
    case VersionOrder::Unknown:
        break;
    }
    return UpdateStatus::Unknown;
}

} // namespace installer
