#include "MainWindow.h"

#include "CoverCache.h"
#include "GameListModel.h"
#include "GameSortModel.h"

#include "questports/Catalog.h"
#include "questports/GameOrder.h"

#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
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
    file->addAction(tr("&Open Payload..."), QKeySequence::Open, this, &MainWindow::openPayload);
    file->addAction(tr("&Reload"), QKeySequence::Refresh, this, &MainWindow::reloadPayload);
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    createToolBar();
    statusBar()->addWidget(m_status, 1);
}

void MainWindow::createToolBar()
{
    QToolBar* toolBar = addToolBar(tr("Games"));
    toolBar->setObjectName(QStringLiteral("gamesToolBar"));
    toolBar->setMovable(false);

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

bool MainWindow::loadPayload(const QString& path)
{
    m_payloadPath = path;
    QString error;
    const auto catalog = questports::loadCatalog(path, &error);
    if (!catalog) {
        m_model->setGames({});
        m_status->setText(error);
        return false;
    }

    m_model->setGames(catalog->games);
    QSettings().setValue(QStringLiteral("payloadPath"), QFileInfo(path).absoluteFilePath());
    const qsizetype count = catalog->games.size();
    const QString games = count == 1 ? tr("1 game") : tr("%1 games").arg(count);
    m_status->setText(tr("%1 from %2, built %3")
                          .arg(games, QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()),
                               QLocale().toString(catalog->generatedAt.toLocalTime(), QLocale::ShortFormat)));
    return true;
}

void MainWindow::openPayload()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Payload"), QFileInfo(m_payloadPath).path(),
                                                      tr("QuestPorts payload (*.json);;All files (*)"));
    if (path.isEmpty())
        return;
    if (!loadPayload(path))
        QMessageBox::warning(this, tr("Open Payload"), m_status->text());
}

void MainWindow::reloadPayload()
{
    if (!m_payloadPath.isEmpty())
        loadPayload(m_payloadPath);
}
