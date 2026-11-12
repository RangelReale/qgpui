#pragma once

#include <QMainWindow>

class CoverCache;
class GameListModel;
class GameSortModel;
class QComboBox;
class QLabel;
class QListView;

// The main window: every game of the QuestPorts payload as a cover with its title below.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Loads the payload at path and shows its games. Returns false, and shows why in the status bar,
    // when the file cannot be read.
    bool loadPayload(const QString& path);

private:
    void openPayload();
    void reloadPayload();
    void createToolBar();
    void orderChanged();

    CoverCache* m_covers;
    GameListModel* m_model;
    GameSortModel* m_sorted;
    QComboBox* m_order;
    QListView* m_view;
    QLabel* m_status;
    QString m_payloadPath;
};
