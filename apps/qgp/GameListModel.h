#pragma once

#include "GameFlags.h"

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
        FlagsRole, // GameFlags, as int.
    };

    explicit GameListModel(CoverCache* covers, QObject* parent = nullptr);

    void setGames(QList<questports::Game> games);
    const questports::Game& game(int row) const { return m_games.at(row); }
    GameFlags flags(int row) const;
    // Sets what this PC's Steam and GOG libraries hold, by slug, next to what the recipes say
    // the stores sell.
    void setLibraries(const QHash<QString, GameFlags>& holding);
    // Sets which games are on the current headset, by slug. Empty when there is no headset.
    void setHeadset(const QHash<QString, GameFlags>& onHeadset);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    void coverReady(const QUrl& url);

    CoverCache* m_covers;
    QList<questports::Game> m_games;
    void flagsChanged();

    QList<GameFlags> m_selling; // By row.
    QHash<QString, GameFlags> m_holding;
    QHash<QString, GameFlags> m_onHeadset;
    QMultiHash<QUrl, int> m_rowsByCover;
};
