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
namespace questports {
struct Game;
struct OriginPlatform;
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
    void orderChanged();
    void filterChanged();
    // Fills the Source box from the catalog's platforms, with the number of games of each.
    void fillSourceBox(const QList<questports::OriginPlatform>& platforms);
    void findLibraries();
    // Reads the current headset's packages and install records, and marks the games on it.
    void findHeadsetGames();
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
    int m_headsetQuery = 0; // Counts headset queries, so that an older answer is dropped.
    QString m_headsetQuerySerial;
    QListView* m_view;
    QLabel* m_status;
    HeadsetConnection* m_headset;
    HeadsetDialog* m_headsetDialog = nullptr;
    QAction* m_headsetAction = nullptr;
    QAction* m_openGameAction = nullptr;
    QHash<QString, QPointer<GameDialog>> m_gameDialogs;
    QToolButton* m_headsetStatus;
    QString m_catalogPath;
};
