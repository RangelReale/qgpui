#include "GameListModel.h"

#include "CoverCache.h"

#include <QPainter>

namespace {

struct Tag {
    QString text;
    QColor color;
};

// The tag for a QuestPorts port status, or no text for an unknown one.
Tag statusTag(const QString& status)
{
    if (status == QLatin1StringView("released"))
        return {GameListModel::tr("Released"), QColor(106, 27, 154, 235)};
    if (status == QLatin1StringView("playable_beta"))
        return {GameListModel::tr("Playable beta"), QColor(230, 81, 0, 235)};
    if (status == QLatin1StringView("in_development"))
        return {GameListModel::tr("In development"), QColor(97, 97, 97, 235)};
    return {};
}

// Draws rounded tags in a row from the cover's top-left corner, wrapping to a new row when one
// does not fit.
void markTags(QPixmap& cover, const QList<Tag>& tags)
{
    if (cover.isNull() || tags.isEmpty())
        return;
    QPainter painter(&cover);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    const QFontMetricsF metrics(font);
    const qreal height = metrics.height() * 1.3;
    const qreal margin = 6;
    const qreal coverWidth = cover.deviceIndependentSize().width();
    qreal left = margin;
    qreal top = margin;
    for (const Tag& tag : tags) {
        const qreal width = metrics.horizontalAdvance(tag.text) + height;
        if (left > margin && left + width > coverWidth - margin) {
            left = margin;
            top += height + margin / 2;
        }
        const QRectF pill(left, top, width, height);
        painter.setPen(Qt::NoPen);
        painter.setBrush(tag.color);
        painter.drawRoundedRect(pill, height / 2, height / 2);
        painter.setPen(Qt::white);
        painter.drawText(pill, Qt::AlignCenter, tag.text);
        left = pill.right() + margin / 2;
    }
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
        if (flags & GameFlag::InstalledByQgpui)
            lines.append(tr("On your headset, installed by QGPUI"));
        else if (flags & GameFlag::InstalledOutsideQgpui)
            lines.append(tr("On your headset, installed outside QGPUI"));
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
        const GameFlags flags = this->flags(index.row());
        QList<Tag> tags;
        if (flags & (GameFlag::InstalledByQgpui | GameFlag::InstalledOutsideQgpui))
            tags.append({tr("Installed"), QColor(46, 125, 50, 235)});
        if (flags & GameFlag::UpdateAvailable)
            tags.append({tr("Update"), QColor(21, 101, 192, 235)});
        if (const Tag status = statusTag(game.status); !status.text.isEmpty())
            tags.append(status);
        markTags(cover, tags);
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
