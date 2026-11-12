#include "CoverCache.h"

#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPalette>
#include <QStandardPaths>

namespace {

constexpr qint64 kDiskCacheBytes = 200 * 1024 * 1024;

} // namespace

CoverCache::CoverCache(QSize size, QObject* parent)
    : QObject(parent)
    , m_size(size)
    , m_network(new QNetworkAccessManager(this))
{
    auto* diskCache = new QNetworkDiskCache(this);
    diskCache->setCacheDirectory(
        QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(QStringLiteral("covers")));
    diskCache->setMaximumCacheSize(kDiskCacheBytes);
    m_network->setCache(diskCache);
    m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    connect(m_network, &QNetworkAccessManager::finished, this, &CoverCache::finished);

    m_loading = placeholder(QString());
    m_missing = placeholder(tr("No cover"));
}

QPixmap CoverCache::cover(const QUrl& url)
{
    if (!url.isValid() || url.isEmpty())
        return m_missing;
    if (const auto it = m_covers.constFind(url); it != m_covers.cend())
        return *it;
    if (!m_pending.contains(url)) {
        m_pending.insert(url);
        QNetworkRequest request(url);
        // Covers rarely change; use a cached copy even when the server says it is stale.
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("qgpui"));
        m_network->get(request);
    }
    return m_loading;
}

void CoverCache::finished(QNetworkReply* reply)
{
    reply->deleteLater();
    const QUrl url = reply->request().url();
    m_pending.remove(url);

    QImage image;
    if (reply->error() == QNetworkReply::NoError)
        image.loadFromData(reply->readAll());
    if (image.isNull())
        qWarning("Cover %s: %s", qUtf8Printable(url.toString()),
                 reply->error() == QNetworkReply::NoError ? "not an image" : qUtf8Printable(reply->errorString()));

    m_covers.insert(url, image.isNull() ? m_missing : fitted(image));
    emit coverReady(url);
}

QPixmap CoverCache::fitted(const QImage& image) const
{
    const qreal ratio = qApp->devicePixelRatio();
    const QSize pixels = m_size * ratio;
    const QImage scaled = image.scaled(pixels, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap result(pixels);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.drawImage((pixels.width() - scaled.width()) / 2, (pixels.height() - scaled.height()) / 2, scaled);
    painter.end();
    result.setDevicePixelRatio(ratio);
    return result;
}

QPixmap CoverCache::placeholder(const QString& text) const
{
    const qreal ratio = qApp->devicePixelRatio();
    QPixmap result(m_size * ratio);
    result.setDevicePixelRatio(ratio);
    result.fill(Qt::transparent);

    const QPalette palette = QGuiApplication::palette();
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.color(QPalette::Mid));
    const QRectF rect(QPointF(0, 0), QSizeF(m_size));
    painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 6, 6);
    if (!text.isEmpty()) {
        painter.setPen(palette.color(QPalette::Light));
        painter.drawText(rect, Qt::AlignCenter, text);
    }
    return result;
}
