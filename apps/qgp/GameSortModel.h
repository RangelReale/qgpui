#pragma once

#include "GameFlags.h"

#include "questports/GameOrder.h"

#include <QSortFilterProxyModel>

class GameListModel;

// Which games to show. A game must match every part.
struct GameFilter
{
    enum class Headset { Any, OnHeadset, InstalledByQgp, InstalledOutsideQgp, NotOnHeadset, UpdateAvailable };

    QString source;        // An origin platform id; empty for any.
    GameFlags store;       // One of the store flags; empty for any.
    Headset headset = Headset::Any;
    bool selfContainedOnly = false;
    // The site's filters.
    QString runsOn;        // A headset label the game must list, such as "Quest 3"; empty for any.
    QString vrStyle;       // A VR style id; empty for any.
    bool noPcNeededOnly = false;
    QString category;      // A port type, such as "emulator"; empty for any.
    QString developer;     // Compared without case; empty for any.
    QString status;        // "released", "playable_beta" or "in_development"; empty for any.

    bool operator==(const GameFilter&) const = default;
};

// Shows a GameListModel in one of the questports::GameOrder orders, filtered by a GameFilter.
class GameSortModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit GameSortModel(GameListModel* source, QObject* parent = nullptr);

    questports::GameOrder order() const { return m_order; }
    void setOrder(questports::GameOrder order);

    const GameFilter& filter() const { return m_filter; }
    void setFilter(const GameFilter& filter);
    bool accepts(const questports::Game& game, GameFlags flags) const;
    // Applies the filter again, after the source's store flags changed.
    void refilter();

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    GameListModel* m_source;
    questports::GameOrder m_order = questports::GameOrder::Title;
    questports::GameComparator m_comparator{m_order};
    GameFilter m_filter;
};
