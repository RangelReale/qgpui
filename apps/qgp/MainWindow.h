#pragma once

#include <QHash>
#include <QMainWindow>
#include <QPointer>

class CoverCache;
class GameDialog;
class GameListModel;
class GameSortModel;
class HeadsetConnection;
class HeadsetDialog;
namespace installer {
struct InstallRecord;
class ReleaseLookup;
} // namespace installer
namespace qadb {
struct PackageInfo;
}
namespace questports {
struct Catalog;
struct Game;
} // namespace questports

class QAction;
class QModelIndex;
class QCheckBox;
class QComboBox;
class QLabel;
class QListView;
class QToolButton;

// The main window: every game of the qgp catalog as a cover with its title below.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Loads the catalog at path and shows its games. Returns false, and shows why in the status bar,
    // when the file cannot be read.
    bool loadCatalog(const QString& path);

private:
    void openCatalog();
    void reloadCatalog();
    void createToolBar();
    // The second row: the site's filters, with Type, Developer and Status behind More filters.
    void createFilterBar();
    void orderChanged();
    void filterChanged();
    // Clears every filter. The order stays.
    void resetFilters();
    // Shows or hides the boxes behind More filters, and counts the ones in use on its button.
    void updateMoreFilters();
    // Fills the boxes whose choices come from the catalog, with the number of games of each.
    void fillCatalogBoxes(const questports::Catalog& catalog);
    void findLibraries();
    // Reads the current headset's packages and install records, and marks the games on it.
    void findHeadsetGames();
    // Marks the game when its APK has a newer release than the headset's package. Does nothing when
    // the headset query has changed by the time the answers come.
    void checkForUpdate(const questports::Game& game, const QString& serial, const qadb::PackageInfo& package,
                        const QHash<QString, installer::InstallRecord>& recorded, int query);
    void showHeadsetDialog();
    void updateHeadsetStatus();
    void chooseFilesFolder();
    void openGame();
    void openGameAt(const QModelIndex& index);
    void updateGameActions();
    // The selected game, or nullptr.
    const questports::Game* selectedGame() const;

    CoverCache* m_covers;
    GameListModel* m_model;
    GameSortModel* m_sorted;
    QComboBox* m_order;
    QComboBox* m_source;
    QComboBox* m_store;
    QComboBox* m_onHeadset;
    QCheckBox* m_selfContained;
    QComboBox* m_runsOn;
    QComboBox* m_vrStyle;
    QCheckBox* m_noPcNeeded;
    QComboBox* m_category;
    QComboBox* m_developer;
    QComboBox* m_portStatus;
    QAction* m_moreFiltersAction = nullptr;
    QList<QAction*> m_moreFilters; // The toolbar actions of the boxes behind More filters.
    QAction* m_resetFiltersAction = nullptr;
    int m_headsetQuery = 0; // Counts headset queries, so that an older answer is dropped.
    QString m_headsetQuerySerial;
    QListView* m_view;
    QLabel* m_status;
    HeadsetConnection* m_headset;
    installer::ReleaseLookup* m_releases; // Shared with the game dialogs.
    HeadsetDialog* m_headsetDialog = nullptr;
    QAction* m_headsetAction = nullptr;
    QAction* m_openGameAction = nullptr;
    QHash<QString, QPointer<GameDialog>> m_gameDialogs;
    QToolButton* m_headsetStatus;
    QString m_catalogPath;
};
