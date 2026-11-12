#include "GameListModel.h"

#include "CoverCache.h"

GameListModel::GameListModel(CoverCache* covers, QObject* parent)
    : QAbstractListModel(parent)
    , m_covers(covers)
{
    connect(m_covers, &CoverCache::coverReady, this, &GameListModel::coverReady);
}

void GameListModel::setGames(QList<questports::Game> games)
{
    beginResetModel();
    m_games = std::move(games);
    m_rowsByCover.clear();
    m_stores.clear();
    for (int row = 0; row < m_games.size(); ++row) {
        m_rowsByCover.insert(m_games[row].coverUrl, row);
        m_stores.append(storesSelling(m_games[row]));
    }
    endResetModel();
}

int GameListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_games.size());
}

QVariant GameListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const questports::Game& game = m_games[index.row()];
    switch (role) {
    case Qt::DisplayRole:
        return game.title;
    case Qt::ToolTipRole: {
        const StoreFlags stores = m_stores[index.row()];
        QStringList lines{game.title};
        if (stores & StoreFlag::InSteamLibrary)
            lines.append(tr("In your Steam library"));
        if (stores & StoreFlag::InGogLibrary)
            lines.append(tr("In your GOG library"));
        return lines.join(u'\n');
    }
    case StoresRole:
        return int(m_stores[index.row()].toInt());
    case Qt::DecorationRole:
        return m_covers->cover(game.coverUrl);
    case SlugRole:
        return game.slug;
    default:
        return {};
    }
}

void GameListModel::setLibraries(const QHash<QString, StoreFlags>& holding)
{
    for (int row = 0; row < m_games.size(); ++row)
        m_stores[row] = storesSelling(m_games[row]) | holding.value(m_games[row].slug);
    if (!m_games.isEmpty())
        emit dataChanged(index(0), index(int(m_games.size()) - 1), {StoresRole, Qt::ToolTipRole});
}

void GameListModel::coverReady(const QUrl& url)
{
    for (auto it = m_rowsByCover.constFind(url); it != m_rowsByCover.cend() && it.key() == url; ++it) {
        const QModelIndex changed = index(it.value());
        emit dataChanged(changed, changed, {Qt::DecorationRole});
    }
}
