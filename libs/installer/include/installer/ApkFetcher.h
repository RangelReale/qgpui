#pragma once

#include "installer/Recipe.h"

#include <QJsonArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;

namespace installer {

struct ReleaseAsset
{
    QString tag;
    QString name;
    QUrl url; // browser_download_url
    qint64 size = 0;
};

// Picks the APK of a github source from a GitHub "list releases" answer: the newest release that
// is not a draft, is not a pre-release unless the source allows them, has a tag matching the
// source's tag pattern, and has an asset matching its asset pattern. Returns that asset.
std::optional<ReleaseAsset> pickRelease(const QJsonArray& releases, const ApkSource& source);

// Gets an APK onto the PC: looks up a github source's release, or takes a url source's URL, and
// downloads it into a cache folder. A cached file of the same name and size is reused.
// GITHUB_TOKEN, when set, raises GitHub's rate limit (60 requests an hour without it).
class ApkFetcher : public QObject
{
    Q_OBJECT

public:
    explicit ApkFetcher(const QString& cacheDir, QObject* parent = nullptr);

    // For tests: the base of GitHub's API, by default https://api.github.com.
    void setApiBaseUrl(const QUrl& url) { m_apiBase = url; }

    // Starts fetching. Emits finished() once. A manual source, or one whose APK is inside a zip,
    // fails at once with a message that says what the user must do.
    void fetch(const ApkSource& source);
    void abort();

signals:
    void resolved(const QString& tag, const QString& fileName);
    void progress(qint64 done, qint64 total);
    void finished(bool success, const QString& apkPath, const QString& errorMessage);

private:
    void download(const QUrl& url, const QString& folder, const QString& fileName, qint64 expectedSize);
    void fail(const QString& message);

    QString m_cacheDir;
    QUrl m_apiBase;
    QNetworkAccessManager* m_network;
    QPointer<QNetworkReply> m_reply;
    QSaveFile* m_file = nullptr;
};

} // namespace installer
