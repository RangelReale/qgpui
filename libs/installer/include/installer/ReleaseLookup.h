#pragma once

#include "installer/ApkFetcher.h"
#include "installer/Recipe.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <chrono>
#include <functional>
#include <optional>

class QNetworkAccessManager;

namespace installer {

// Finds the release that ApkFetcher would install from a github source, without downloading it.
// Asks GitHub once per repository and keeps the answer for a while, so that checking every game on
// a headset for updates, and opening a game afterwards, costs one request per repository.
class ReleaseLookup : public QObject
{
    Q_OBJECT

public:
    // Gets the release, or the error message when there is none.
    using Callback = std::function<void(const std::optional<ReleaseAsset>& asset, const QString& errorMessage)>;

    explicit ReleaseLookup(QObject* parent = nullptr);

    // For tests: the base of GitHub's API, by default https://api.github.com.
    void setApiBaseUrl(const QUrl& url) { m_apiBase = url; }
    // How long an answer is kept: 1 hour by default. A failed request is kept for 5 minutes, so
    // that a missing network or GitHub's rate limit is not asked again for every game.
    void setMaxAge(std::chrono::seconds answer, std::chrono::seconds failure);

    // Calls done once, on a later event-loop iteration, unless context is deleted before. A source
    // that is not on GitHub gets no release and an error message.
    void latest(const ApkSource& source, QObject* context, Callback done);

    // Forgets every answer, so that the next lookups ask GitHub again.
    void clear();

private:
    struct Waiting
    {
        ApkSource source;
        QPointer<QObject> context;
        Callback done;
    };
    struct Repo
    {
        bool pending = false;
        QJsonArray releases;
        QString error;
        QDateTime fetchedAt;
        QList<Waiting> waiting;
    };

    bool fresh(const Repo& repo) const;
    static void answer(const Repo& repo, const Waiting& waiting);

    QUrl m_apiBase;
    std::chrono::seconds m_maxAge{3600};
    std::chrono::seconds m_failureMaxAge{300};
    QNetworkAccessManager* m_network;
    QHash<QString, Repo> m_repos;
};

} // namespace installer
