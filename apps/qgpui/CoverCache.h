#pragma once

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QSize>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

// Downloads game covers and keeps them scaled to one size. Downloads go through a disk cache, so
// covers seen before show without a network connection.
class CoverCache : public QObject
{
    Q_OBJECT

public:
    // size is in device-independent pixels. Every cover is fitted into it, keeping its aspect
    // ratio, and centred.
    explicit CoverCache(QSize size, QObject* parent = nullptr);

    QSize size() const { return m_size; }

    // The cover for url. Returns a placeholder, and starts the download, when the cover is not
    // here yet; coverReady() is emitted once it is.
    QPixmap cover(const QUrl& url);

signals:
    void coverReady(const QUrl& url);

private:
    void finished(QNetworkReply* reply);
    QPixmap fitted(const QImage& image) const;
    QPixmap placeholder(const QString& text) const;

    QSize m_size;
    QNetworkAccessManager* m_network;
    QHash<QUrl, QPixmap> m_covers;
    QSet<QUrl> m_pending;
    QPixmap m_loading;
    QPixmap m_missing;
};
