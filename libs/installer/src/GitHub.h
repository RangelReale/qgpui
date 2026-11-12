#pragma once

#include <QNetworkRequest>
#include <QString>
#include <QUrl>

namespace installer::detail {

// A request with qgp's user agent that follows redirects which stay as safe.
QNetworkRequest request(const QUrl& url);

// GitHub's "list releases" request for owner/name: the newest 100 releases. Sends GITHUB_TOKEN,
// when set, which raises GitHub's rate limit (60 requests an hour without it).
QNetworkRequest releasesRequest(const QUrl& apiBase, const QString& repo);

} // namespace installer::detail
