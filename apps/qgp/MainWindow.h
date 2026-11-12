#pragma once

#include <QMainWindow>

class CoverCache;
class GameListModel;
class GameSortModel;
class HeadsetConnection;
class HeadsetDialog;
namespace questports {
struct Game;
}

class QAction;
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
    void showHeadsetDialog();
    void updateHeadsetStatus();
    void chooseFilesFolder();
    void createFilesFolder();
    void updateGameActions();
    // The selected game, or nullptr.
    const questports::Game* selectedGame() const;

    CoverCache* m_covers;
    GameListModel* m_model;
    GameSortModel* m_sorted;
    QComboBox* m_order;
    QListView* m_view;
    QLabel* m_status;
    HeadsetConnection* m_headset;
    HeadsetDialog* m_headsetDialog = nullptr;
    QAction* m_headsetAction = nullptr;
    QAction* m_filesFolderAction = nullptr;
    QToolButton* m_headsetStatus;
    QString m_catalogPath;
};
