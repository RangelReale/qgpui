#include "GameListModel.h"

#include "CoverCache.h"

#include <QPainter>

namespace {

// Draws "Update" in a band across the bottom of a cover.
void markUpdate(QPixmap& cover)
{
    if (cover.isNull())
        return;
    QPainter painter(&cover);
    const QRectF bounds(QPointF(0, 0), cover.deviceIndependentSize());
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    const qreal height = painter.fontMetrics().height() * 1.5;
    const QRectF band(bounds.left(), bounds.bottom() - height, bounds.width(), height);
    painter.fillRect(band, QColor(21, 101, 192, 225));
    painter.setPen(Qt::white);
    painter.drawText(band, Qt::AlignCenter, GameListModel::tr("Update"));
}

} // namespace

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
        if (flags & GameFlag::UpdateAvailable)
            lines.append(tr("An update is available"));
        if (flags & GameFlag::InSteamLibrary)
            lines.append(tr("In your Steam library"));
        if (flags & GameFlag::InGogLibrary)
            lines.append(tr("In your GOG library"));
        return lines.join(u'\n');
    }
    case FlagsRole:
        return flags(index.row()).toInt();
    case Qt::DecorationRole: {
        QPixmap cover = m_covers->cover(game.coverUrl);
        if (flags(index.row()) & GameFlag::UpdateAvailable)
            markUpdate(cover);
        return cover;
    }
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

void GameListModel::setUpdateAvailable(const QString& slug)
{
    auto it = m_onHeadset.find(slug);
    if (it == m_onHeadset.end() || it->testFlag(GameFlag::UpdateAvailable))
        return;
    *it |= GameFlag::UpdateAvailable;
    for (int row = 0; row < m_games.size(); ++row) {
        if (m_games[row].slug == slug)
            emit dataChanged(index(row), index(row), {FlagsRole, Qt::ToolTipRole, Qt::DecorationRole});
    }
}

void GameListModel::flagsChanged()
{
    if (!m_games.isEmpty())
        emit dataChanged(index(0), index(int(m_games.size()) - 1), {FlagsRole, Qt::ToolTipRole, Qt::DecorationRole});
}

void GameListModel::coverReady(const QUrl& url)
{
    for (auto it = m_rowsByCover.constFind(url); it != m_rowsByCover.cend() && it.key() == url; ++it) {
        const QModelIndex changed = index(it.value());
        emit dataChanged(changed, changed, {Qt::DecorationRole});
    }
}
