#include "GameSortModel.h"

#include "GameListModel.h"

GameSortModel::GameSortModel(GameListModel* source, QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_source(source)
{
    setSourceModel(source);
    // Covers arriving change only the decoration; do not re-sort for them.
    setDynamicSortFilter(false);
    sort(0);
    // A reset (a new catalog) clears the sort; apply it again.
    connect(source, &QAbstractItemModel::modelReset, this, [this] { sort(0); });
}

void GameSortModel::setOrder(questports::GameOrder order)
{
    if (order == m_order)
        return;
    m_order = order;
    m_comparator = questports::GameComparator(order);
    invalidate();
    sort(0);
}

bool GameSortModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    return m_comparator.lessThan(m_source->game(left.row()), m_source->game(right.row()));
}

void GameSortModel::setFilter(const GameFilter& filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    refilter();
}

void GameSortModel::refilter()
{
    invalidateFilter();
    sort(0);
}

bool GameSortModel::filterAcceptsRow(int sourceRow, const QModelIndex&) const
{
    return accepts(m_source->game(sourceRow), m_source->flags(sourceRow));
}

bool GameSortModel::accepts(const questports::Game& game, GameFlags flags) const
{
    if (!m_filter.source.isEmpty() && game.originPlatform != m_filter.source)
        return false;
    if (m_filter.store && !(flags & m_filter.store))
        return false;
    if (m_filter.selfContainedOnly && !(flags & GameFlag::SelfContained))
        return false;
    if (!m_filter.runsOn.isEmpty() && !game.headsets.contains(m_filter.runsOn))
        return false;
    if (!m_filter.vrStyle.isEmpty() && game.vrStyle != m_filter.vrStyle)
        return false;
    if (m_filter.noPcNeededOnly && !game.noPcNeeded)
        return false;
    if (!m_filter.category.isEmpty() && game.category != m_filter.category)
        return false;
    if (!m_filter.developer.isEmpty() && game.developer.compare(m_filter.developer, Qt::CaseInsensitive) != 0)
        return false;
    if (!m_filter.status.isEmpty() && game.status != m_filter.status)
        return false;
    switch (m_filter.headset) {
    case GameFilter::Headset::Any:
        return true;
    case GameFilter::Headset::OnHeadset:
        return flags.testFlag(GameFlag::OnHeadset);
    case GameFilter::Headset::InstalledByQgp:
        return flags.testFlag(GameFlag::InstalledByQgp);
    case GameFilter::Headset::InstalledOutsideQgp:
        return flags.testFlag(GameFlag::InstalledOutsideQgp);
    case GameFilter::Headset::NotOnHeadset:
        return !flags.testFlag(GameFlag::OnHeadset);
    }
    return true;
}
