#include "installer/ReleaseLookup.h"

#include "GitHub.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

namespace installer {

ReleaseLookup::ReleaseLookup(QObject* parent)
    : QObject(parent)
    , m_apiBase(QStringLiteral("https://api.github.com"))
    , m_network(new QNetworkAccessManager(this))
{}

void ReleaseLookup::setMaxAge(std::chrono::seconds answer, std::chrono::seconds failure)
{
    m_maxAge = answer;
    m_failureMaxAge = failure;
}

void ReleaseLookup::clear()
{
    // Keep requests that are still running, so that their callers get an answer.
    for (auto it = m_repos.begin(); it != m_repos.end();) {
        if (it->pending)
            ++it;
        else
            it = m_repos.erase(it);
    }
}

bool ReleaseLookup::fresh(const Repo& repo) const
{
    if (!repo.fetchedAt.isValid())
        return false;
    const auto age = repo.error.isEmpty() ? m_maxAge : m_failureMaxAge;
    return repo.fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) < age.count();
}

void ReleaseLookup::answer(const Repo& repo, const Waiting& waiting)
{
    if (!waiting.context)
        return;
    if (!repo.error.isEmpty()) {
        waiting.done(std::nullopt, repo.error);
        return;
    }
    const auto asset = pickRelease(repo.releases, waiting.source);
    if (asset)
        waiting.done(asset, {});
    else
        waiting.done(std::nullopt, QStringLiteral("No release of %1 has a file matching %2.")
                                       .arg(waiting.source.repo, waiting.source.asset));
}

void ReleaseLookup::latest(const ApkSource& source, QObject* context, Callback done)
{
    Waiting waiting{source, context, std::move(done)};
    if (source.kind != ApkSource::Kind::GitHub) {
        QTimer::singleShot(0, context, [waiting] {
            waiting.done(std::nullopt, QStringLiteral("The APK does not come from a GitHub release."));
        });
        return;
    }

    Repo& repo = m_repos[source.repo];
    if (repo.pending) {
        repo.waiting.append(std::move(waiting));
        return;
    }
    if (fresh(repo)) {
        QTimer::singleShot(0, context, [this, key = source.repo, waiting] { answer(m_repos.value(key), waiting); });
        return;
    }

    repo.pending = true;
    repo.waiting.append(std::move(waiting));
    QNetworkReply* reply = m_network->get(detail::releasesRequest(m_apiBase, source.repo));
    connect(reply, &QNetworkReply::finished, this, [this, reply, key = source.repo] {
        reply->deleteLater();
        Repo& repo = m_repos[key];
        repo.pending = false;
        repo.fetchedAt = QDateTime::currentDateTimeUtc();
        repo.releases = {};
        repo.error.clear();
        if (reply->error() != QNetworkReply::NoError)
            repo.error = QStringLiteral("Could not list the releases of %1: %2").arg(key, reply->errorString());
        else
            repo.releases = QJsonDocument::fromJson(reply->readAll()).array();
        // A callback may look up again, which may change m_repos: answer from copies.
        const Repo answered = repo;
        repo.waiting.clear();
        for (const Waiting& waiting : answered.waiting)
            answer(answered, waiting);
    });
}

} // namespace installer
