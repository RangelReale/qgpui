#include "installer/ApkFetcher.h"

#include "GitHub.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>
#include <utility>

namespace installer {

namespace {

// A file system name for a repository, tag or downloaded file. These come from the catalog and
// from GitHub's answers, so nothing in them may name another folder: no separator survives, and
// a name of only dots is replaced.
QString safeName(QString s, const QString& fallback = QStringLiteral("_"))
{
    static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9._-]"));
    s.replace(unsafe, QStringLiteral("_"));
    if (s.isEmpty() || std::all_of(s.cbegin(), s.cend(), [](QChar c) { return c == u'.'; }))
        return fallback;
    return s;
}

} // namespace

namespace detail {

QNetworkRequest request(const QUrl& url)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("qgp"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

QNetworkRequest releasesRequest(const QUrl& apiBase, const QString& repo)
{
    QUrl url = apiBase;
    url.setPath(url.path() + QStringLiteral("/repos/%1/releases").arg(repo));
    url.setQuery(QStringLiteral("per_page=100"));
    QNetworkRequest req = request(url);
    req.setRawHeader("Accept", "application/vnd.github+json");
    const QByteArray token = qgetenv("GITHUB_TOKEN");
    if (!token.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + token);
    return req;
}

} // namespace detail

using detail::request;

std::optional<ReleaseAsset> pickRelease(const QJsonArray& releases, const ApkSource& source)
{
    const QRegularExpression tag(source.tag);
    const QRegularExpression asset(source.asset);
    std::optional<ReleaseAsset> best;
    QString bestDate;
    for (const QJsonValue& value : releases) {
        const QJsonObject release = value.toObject();
        if (release.value(QLatin1StringView("draft")).toBool())
            continue;
        if (release.value(QLatin1StringView("prerelease")).toBool() && !source.prerelease)
            continue;
        const QString tagName = release.value(QLatin1StringView("tag_name")).toString();
        if (!source.tag.isEmpty() && !tag.match(tagName).hasMatch())
            continue;
        const QString published = release.value(QLatin1StringView("published_at")).toString();
        if (best && published <= bestDate)
            continue;
        for (const QJsonValue& a : release.value(QLatin1StringView("assets")).toArray()) {
            const QJsonObject o = a.toObject();
            const QString name = o.value(QLatin1StringView("name")).toString();
            if (!asset.match(name).hasMatch())
                continue;
            best = ReleaseAsset{tagName, name, QUrl(o.value(QLatin1StringView("browser_download_url")).toString()),
                                o.value(QLatin1StringView("size")).toInteger()};
            bestDate = published;
            break;
        }
    }
    return best;
}

ApkFetcher::ApkFetcher(const QString& cacheDir, QObject* parent)
    : QObject(parent)
    , m_cacheDir(cacheDir)
    , m_apiBase(QStringLiteral("https://api.github.com"))
    , m_network(new QNetworkAccessManager(this))
{}

void ApkFetcher::fail(const QString& message)
{
    emit finished(false, {}, message);
}

void ApkFetcher::abort()
{
    if (m_reply)
        m_reply->abort();
}

void ApkFetcher::fetch(const ApkSource& source)
{
    if (source.kind == ApkSource::Kind::Manual) {
        fail(QStringLiteral("Download the APK from %1 and pick the file.").arg(source.page));
        return;
    }
    if (!source.inZip.isEmpty()) {
        fail(QStringLiteral("This APK comes inside a zip file, which qgp cannot unpack yet."));
        return;
    }
    if (source.kind == ApkSource::Kind::Url) {
        const QUrl url(source.url);
        download(url, QDir(m_cacheDir).filePath(QStringLiteral("url")), safeName(QFileInfo(url.path()).fileName(), QStringLiteral("download.apk")), 0);
        return;
    }

    m_reply = m_network->get(detail::releasesRequest(m_apiBase, source.repo));
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply, source] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            fail(QStringLiteral("Could not list the releases of %1: %2").arg(source.repo, reply->errorString()));
            return;
        }
        const auto asset = pickRelease(QJsonDocument::fromJson(reply->readAll()).array(), source);
        if (!asset) {
            fail(QStringLiteral("No release of %1 has a file matching %2.").arg(source.repo, source.asset));
            return;
        }
        emit resolved(asset->tag, asset->name);
        const QString folder = QDir(m_cacheDir).filePath(safeName(source.repo) + u'/' + safeName(asset->tag));
        download(asset->url, folder, safeName(asset->name, QStringLiteral("download.apk")), asset->size);
    });
}

void ApkFetcher::download(const QUrl& url, const QString& folder, const QString& fileName, qint64 expectedSize)
{
    const QString path = QDir(folder).filePath(fileName);
    if (expectedSize > 0 && QFileInfo(path).size() == expectedSize) {
        emit progress(expectedSize, expectedSize);
        emit finished(true, path, {});
        return;
    }
    if (!QDir().mkpath(folder)) {
        fail(QStringLiteral("Cannot create %1.").arg(folder));
        return;
    }
    m_file = new QSaveFile(path, this);
    if (!m_file->open(QIODevice::WriteOnly)) {
        fail(QStringLiteral("Cannot write %1: %2").arg(path, m_file->errorString()));
        delete m_file;
        m_file = nullptr;
        return;
    }
    m_reply = m_network->get(request(url));
    connect(m_reply, &QNetworkReply::readyRead, this, [this, reply = m_reply] { m_file->write(reply->readAll()); });
    connect(m_reply, &QNetworkReply::downloadProgress, this, &ApkFetcher::progress);
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply, path, url] {
        reply->deleteLater();
        QSaveFile* file = std::exchange(m_file, nullptr);
        file->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            file->cancelWriting();
            fail(QStringLiteral("Could not download %1: %2").arg(url.toString(), reply->errorString()));
            return;
        }
        file->write(reply->readAll());
        if (!file->commit()) {
            fail(QStringLiteral("Cannot write %1: %2").arg(path, file->errorString()));
            return;
        }
        emit finished(true, path, {});
    });
}

} // namespace installer
