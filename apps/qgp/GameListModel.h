#pragma once

#include "questports/Catalog.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QUrl>

class CoverCache;

// The games of a catalog, one row each. The decoration is the game's cover, from a CoverCache.
class GameListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        SlugRole = Qt::UserRole,
    };

    explicit GameListModel(CoverCache* covers, QObject* parent = nullptr);

    void setGames(QList<questports::Game> games);
    const questports::Game& game(int row) const { return m_games.at(row); }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    void coverReady(const QUrl& url);

    CoverCache* m_covers;
    QList<questports::Game> m_games;
    QMultiHash<QUrl, int> m_rowsByCover;
};
