#include "MainWindow.h"

#include "CoverCache.h"
#include "GameListModel.h"

#include "questports/Catalog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>

namespace {

// Covers are mostly Steam header images (460x215) or 16:9; this fits both without much border.
constexpr QSize kCoverSize(300, 140);
constexpr int kCellPadding = 16;
constexpr int kTitleLines = 2;

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_covers(new CoverCache(kCoverSize, this))
    , m_model(new GameListModel(m_covers, this))
    , m_view(new QListView(this))
    , m_status(new QLabel(this))
{
    setWindowTitle(QStringLiteral("qgp"));

    m_view->setModel(m_model);
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

    statusBar()->addWidget(m_status, 1);
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
