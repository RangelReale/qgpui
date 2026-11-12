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
    for (int row = 0; row < m_games.size(); ++row)
        m_rowsByCover.insert(m_games[row].coverUrl, row);
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
    case Qt::ToolTipRole:
        return game.title;
    case Qt::DecorationRole:
        return m_covers->cover(game.coverUrl);
    case SlugRole:
        return game.slug;
    default:
        return {};
    }
}

void GameListModel::coverReady(const QUrl& url)
{
    for (auto it = m_rowsByCover.constFind(url); it != m_rowsByCover.cend() && it.key() == url; ++it) {
        const QModelIndex changed = index(it.value());
        emit dataChanged(changed, changed, {Qt::DecorationRole});
    }
}
