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
}

class QAction;
class QModelIndex;
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
    void showChanged();
    void findLibraries();
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
    QComboBox* m_show;
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
