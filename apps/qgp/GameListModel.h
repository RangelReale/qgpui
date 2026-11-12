#pragma once

#include "StoreAvailability.h"

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
        StoresRole, // StoreFlags, as int.
    };

    explicit GameListModel(CoverCache* covers, QObject* parent = nullptr);

    void setGames(QList<questports::Game> games);
    const questports::Game& game(int row) const { return m_games.at(row); }
    StoreFlags stores(int row) const { return m_stores.at(row); }
    // Sets what this PC's Steam and GOG libraries hold, by slug, next to what the recipes say
    // the stores sell.
    void setLibraries(const QHash<QString, StoreFlags>& holding);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    void coverReady(const QUrl& url);

    CoverCache* m_covers;
    QList<questports::Game> m_games;
    QList<StoreFlags> m_stores;
    QMultiHash<QUrl, int> m_rowsByCover;
};
