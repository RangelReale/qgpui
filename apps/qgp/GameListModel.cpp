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
    m_selling.clear();
    for (int row = 0; row < m_games.size(); ++row) {
        m_rowsByCover.insert(m_games[row].coverUrl, row);
        m_selling.append(storesSelling(m_games[row]) | selfContained(m_games[row]));
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
        const GameFlags flags = this->flags(index.row());
        QStringList lines{game.title};
        if (flags & GameFlag::InstalledByQgp)
            lines.append(tr("On your headset, installed by qgp"));
        else if (flags & GameFlag::InstalledOutsideQgp)
            lines.append(tr("On your headset, installed outside qgp"));
        if (flags & GameFlag::InSteamLibrary)
            lines.append(tr("In your Steam library"));
        if (flags & GameFlag::InGogLibrary)
            lines.append(tr("In your GOG library"));
        return lines.join(u'\n');
    }
    case FlagsRole:
        return flags(index.row()).toInt();
    case Qt::DecorationRole:
        return m_covers->cover(game.coverUrl);
    case SlugRole:
        return game.slug;
    default:
        return {};
    }
}

GameFlags GameListModel::flags(int row) const
{
    const QString& slug = m_games.at(row).slug;
    return m_selling.at(row) | m_holding.value(slug) | m_onHeadset.value(slug);
}

void GameListModel::setLibraries(const QHash<QString, GameFlags>& holding)
{
    m_holding = holding;
    flagsChanged();
}

void GameListModel::setHeadset(const QHash<QString, GameFlags>& onHeadset)
{
    m_onHeadset = onHeadset;
    flagsChanged();
}

void GameListModel::flagsChanged()
{
    if (!m_games.isEmpty())
        emit dataChanged(index(0), index(int(m_games.size()) - 1), {FlagsRole, Qt::ToolTipRole});
}

void GameListModel::coverReady(const QUrl& url)
{
    for (auto it = m_rowsByCover.constFind(url); it != m_rowsByCover.cend() && it.key() == url; ++it) {
        const QModelIndex changed = index(it.value());
        emit dataChanged(changed, changed, {Qt::DecorationRole});
    }
}
