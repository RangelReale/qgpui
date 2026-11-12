#include "MainWindow.h"

#include "CoverCache.h"
#include "GameDialog.h"
#include "GameListModel.h"
#include "GameSortModel.h"
#include "HeadsetConnection.h"
#include "HeadsetDialog.h"

#include "questports/Catalog.h"
#include "questports/GameOrder.h"


#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QKeySequence>
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>

#include <algorithm>

namespace {

const QString kOrderSetting = QStringLiteral("order");
QString orderLabel(questports::GameOrder order)
{
    switch (order) {
    case questports::GameOrder::Title:
        return MainWindow::tr("Title");
    case questports::GameOrder::RecentlyAdded:
        return MainWindow::tr("Recently added");
    case questports::GameOrder::RecentlyReleased:
        return MainWindow::tr("Recently released");
    }
    return {};
}

// Covers are mostly Steam header images (460x215) or 16:9; this fits both without much border.
constexpr QSize kCoverSize(300, 140);
constexpr int kCellPadding = 16;
constexpr int kTitleLines = 2;

// Scrolling per pixel in icon mode makes one scroll step a whole row, and a wheel notch is several
// steps, so a notch jumps about three rows. This view sets the step so that a notch moves half a row.
class GameListView : public QListView
{
public:
    using QListView::QListView;

protected:
    void updateGeometries() override
    {
        QListView::updateGeometries(); // Resets the single step.
        const int linesPerNotch = qMax(1, QApplication::wheelScrollLines());
        verticalScrollBar()->setSingleStep(qMax(1, gridSize().height() / 2 / linesPerNotch));
    }
};

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_covers(new CoverCache(kCoverSize, this))
    , m_model(new GameListModel(m_covers, this))
    , m_sorted(new GameSortModel(m_model, this))
    , m_order(new QComboBox(this))
    , m_view(new GameListView(this))
    , m_status(new QLabel(this))
    , m_headset(new HeadsetConnection(this))
    , m_headsetStatus(new QToolButton(this))
{
    setWindowTitle(QStringLiteral("qgp"));

    m_view->setModel(m_sorted);
    m_view->setViewMode(QListView::IconMode);
    m_view->setIconSize(kCoverSize);
    m_view->setGridSize(QSize(kCoverSize.width() + kCellPadding,
                              kCoverSize.height() + kTitleLines * m_view->fontMetrics().lineSpacing() + kCellPadding));
    m_view->setResizeMode(QListView::Adjust);
    m_view->setMovement(QListView::Static);
    m_view->setWrapping(true);
    m_view->setWordWrap(true);
    m_view->setTextElideMode(Qt::ElideRight);
    m_view->setUniformItemSizes(true);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setCentralWidget(m_view);

    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&Open Catalog..."), QKeySequence::Open, this, &MainWindow::openCatalog);
    file->addAction(tr("&Reload"), QKeySequence::Refresh, this, &MainWindow::reloadCatalog);
    file->addSeparator();
    file->addAction(tr("Files &Folder..."), this, &MainWindow::chooseFilesFolder);
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* game = menuBar()->addMenu(tr("&Game"));
    m_openGameAction = game->addAction(tr("&Open"), this, &MainWindow::openGame);
    m_openGameAction->setStatusTip(tr("Open the selected game: its files, its state on the headset, and Install"));
    m_view->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_view->addAction(m_openGameAction);
    // Double-click or Enter.
    connect(m_view, &QListView::activated, this, &MainWindow::openGameAt);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::updateGameActions);
    connect(m_sorted, &QAbstractItemModel::modelReset, this, &MainWindow::updateGameActions);
    updateGameActions();

    createToolBar();
    statusBar()->addWidget(m_status, 1);

    // The headset connection, at the right of the status bar. A click opens the Headset dialog.
    m_headsetStatus->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_headsetStatus->setAutoRaise(true);
    m_headsetStatus->setIconSize(QSize(12, 12));
    connect(m_headsetStatus, &QToolButton::clicked, this, &MainWindow::showHeadsetDialog);
    statusBar()->addPermanentWidget(m_headsetStatus);

    connect(m_headset, &HeadsetConnection::statusChanged, this, &MainWindow::updateHeadsetStatus);
    updateHeadsetStatus();
    m_headset->start();
}

void MainWindow::createToolBar()
{
    QToolBar* toolBar = addToolBar(tr("Games"));
    toolBar->setObjectName(QStringLiteral("gamesToolBar"));
    toolBar->setMovable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    m_headsetAction = toolBar->addAction(tr("&Headset"), this, &MainWindow::showHeadsetDialog);
    m_headsetAction->setToolTip(tr("Connect a headset"));
    toolBar->addSeparator();

    auto* label = new QLabel(tr("&Order:"), toolBar);
    label->setBuddy(m_order);
    label->setContentsMargins(4, 0, 4, 0);
    toolBar->addWidget(label);

    for (questports::GameOrder order : questports::allGameOrders())
        m_order->addItem(orderLabel(order), questports::gameOrderKey(order));
    m_order->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    toolBar->addWidget(m_order);

    const QString saved = QSettings().value(kOrderSetting).toString();
    m_order->setCurrentIndex(qMax(0, m_order->findData(questports::gameOrderKey(questports::gameOrderFromKey(saved)))));
    m_sorted->setOrder(questports::gameOrderFromKey(m_order->currentData().toString()));
    connect(m_order, &QComboBox::currentIndexChanged, this, &MainWindow::orderChanged);
}

void MainWindow::orderChanged()
{
    const QString key = m_order->currentData().toString();
    m_sorted->setOrder(questports::gameOrderFromKey(key));
    QSettings().setValue(kOrderSetting, key);
    m_view->scrollToTop();
}

void MainWindow::showHeadsetDialog()
{
    if (!m_headsetDialog)
        m_headsetDialog = new HeadsetDialog(m_headset, this);
    m_headsetDialog->show();
    m_headsetDialog->raise();
    m_headsetDialog->activateWindow();
}

void MainWindow::updateHeadsetStatus()
{
    const QIcon icon = HeadsetConnection::statusIcon(m_headset->isReady());
    const QString text = m_headset->statusText();
    const QString advice = m_headset->statusAdvice();
    const QString toolTip = advice.isEmpty() ? text : QStringLiteral("%1\n%2").arg(text, advice);

    m_headsetStatus->setIcon(icon);
    m_headsetStatus->setText(text);
    m_headsetStatus->setToolTip(toolTip);
    m_headsetAction->setIcon(icon);
    m_headsetAction->setToolTip(toolTip);
}

bool MainWindow::loadCatalog(const QString& path)
{
    m_catalogPath = path;
    QString error;
    const auto catalog = questports::loadCatalog(path, &error);
    if (!catalog) {
        m_model->setGames({});
        m_status->setText(error);
        return false;
    }

    m_model->setGames(catalog->games);
    QSettings().setValue(QStringLiteral("catalogPath"), QFileInfo(path).absoluteFilePath());
    const qsizetype count = catalog->games.size();
    const auto recipes = std::count_if(catalog->games.cbegin(), catalog->games.cend(),
                                       [](const questports::Game& game) { return game.hasRecipe(); });
    const QString games = count == 1 ? tr("1 game") : tr("%1 games").arg(count);
    m_status->setText(tr("%1 (%2 with a recipe) from %3, built %4")
                          .arg(games, QString::number(recipes),
                               QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()),
                               QLocale().toString(catalog->generatedAt.toLocalTime(), QLocale::ShortFormat)));
    return true;
}

void MainWindow::openCatalog()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Catalog"), QFileInfo(m_catalogPath).path(),
                                                      tr("qgp catalog (*.json);;All files (*)"));
    if (path.isEmpty())
        return;
    if (!loadCatalog(path))
        QMessageBox::warning(this, tr("Open Catalog"), m_status->text());
}

void MainWindow::reloadCatalog()
{
    if (!m_catalogPath.isEmpty())
        loadCatalog(m_catalogPath);
}

const questports::Game* MainWindow::selectedGame() const
{
    const QModelIndex index = m_view->selectionModel()->currentIndex();
    if (!index.isValid() || !m_view->selectionModel()->isSelected(index))
        return nullptr;
    return &m_model->game(m_sorted->mapToSource(index).row());
}

void MainWindow::updateGameActions()
{
    const questports::Game* game = selectedGame();
    m_openGameAction->setEnabled(game != nullptr);
}

void MainWindow::chooseFilesFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Files Folder"), GameDialog::filesRoot());
    if (folder.isEmpty())
        return;
    GameDialog::setFilesRoot(folder);
    m_status->setText(tr("qgp looks for your game files in %1").arg(QDir::toNativeSeparators(folder)));
}

void MainWindow::openGame()
{
    openGameAt(m_view->selectionModel()->currentIndex());
}

void MainWindow::openGameAt(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    const questports::Game& game = m_model->game(m_sorted->mapToSource(index).row());
    QPointer<GameDialog>& dialog = m_gameDialogs[game.slug];
    if (!dialog) {
        dialog = new GameDialog(game, m_headset, m_covers, this);
        dialog->show();
    }
    dialog->raise();
    dialog->activateWindow();
}
