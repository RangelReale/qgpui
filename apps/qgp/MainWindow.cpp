#include "MainWindow.h"

#include "CoverCache.h"
#include "GameDialog.h"
#include "GameListModel.h"
#include "GameSortModel.h"
#include "HeadsetConnection.h"
#include "HeadsetDialog.h"
#include "SettingsDialog.h"
#include "GameFlags.h"

#include "questports/Catalog.h"
#include "questports/GameOrder.h"
#include "gamestores/Finder.h"
#include "installer/Recipe.h"
#include "installer/ReleaseLookup.h"
#include "installer/Updates.h"
#include "qadb/Client.h"
#include "qadb/Reply.h"


#include <QApplication>
#include <QCheckBox>
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
#include <QMap>
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
const QString kSourceSetting = QStringLiteral("filter/source");
const QString kStoreSetting = QStringLiteral("filter/store");
const QString kHeadsetSetting = QStringLiteral("filter/headset");
const QString kSelfContainedSetting = QStringLiteral("filter/selfContained");
const QString kRunsOnSetting = QStringLiteral("filter/runsOn");
const QString kVrStyleSetting = QStringLiteral("filter/vrStyle");
const QString kNoPcNeededSetting = QStringLiteral("filter/noPcNeeded");
const QString kCategorySetting = QStringLiteral("filter/category");
const QString kDeveloperSetting = QStringLiteral("filter/developer");
const QString kStatusSetting = QStringLiteral("filter/status");
const QString kMoreFiltersSetting = QStringLiteral("filter/more");

// The site's names for its port types, in the order of its Type box.
const QList<std::pair<QString, const char*>> kCategories = {
    {QStringLiteral("source_port"), QT_TRANSLATE_NOOP("MainWindow", "Source ports")},
    {QStringLiteral("decompilation"), QT_TRANSLATE_NOOP("MainWindow", "Decomps")},
    {QStringLiteral("engine_recreation"), QT_TRANSLATE_NOOP("MainWindow", "Engine recreations")},
    {QStringLiteral("emulator"), QT_TRANSLATE_NOOP("MainWindow", "Emulators")},
    {QStringLiteral("wrapper"), QT_TRANSLATE_NOOP("MainWindow", "Wrappers")},
    {QStringLiteral("vr_injection"), QT_TRANSLATE_NOOP("MainWindow", "VR mods")},
    {QStringLiteral("game_mod"), QT_TRANSLATE_NOOP("MainWindow", "Game mods")},
};

const QList<std::pair<QString, const char*>> kStatuses = {
    {QStringLiteral("released"), QT_TRANSLATE_NOOP("MainWindow", "Released")},
    {QStringLiteral("playable_beta"), QT_TRANSLATE_NOOP("MainWindow", "Playable beta")},
    {QStringLiteral("in_development"), QT_TRANSLATE_NOOP("MainWindow", "In development")},
};

// A box's choice: the value it filters by, and its text.
using Choice = std::pair<QString, QString>;

// Fills a box with "any" and choices. Keeps the choice made before, or the saved one when the box
// is still empty.
void fillChoiceBox(QComboBox* box, const QString& anyText, const QList<Choice>& choices, const QString& setting)
{
    const QString wanted = box->count() > 1 ? box->currentData().toString() : QSettings().value(setting).toString();
    const QSignalBlocker blocker(box);
    box->clear();
    box->addItem(anyText, QString());
    for (const auto& [id, text] : choices)
        box->addItem(text, id);
    box->setCurrentIndex(qMax(0, box->findData(wanted)));
}

// Saves a box's choice, unless the box is still empty (no catalog yet).
void saveChoice(QSettings& settings, const QComboBox* box, const QString& setting)
{
    if (box->count() > 1)
        settings.setValue(setting, box->currentData().toString());
}

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

// Adds a label and its box to a toolbar. Returns the toolbar's actions for both.
QList<QAction*> addBox(QToolBar* toolBar, const QString& text, QComboBox* box)
{
    auto* label = new QLabel(text, toolBar);
    label->setBuddy(box);
    label->setContentsMargins(12, 0, 4, 0);
    box->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    return {toolBar->addWidget(label), toolBar->addWidget(box)};
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
    , m_source(new QComboBox(this))
    , m_store(new QComboBox(this))
    , m_onHeadset(new QComboBox(this))
    , m_selfContained(new QCheckBox(tr("Self-&contained only"), this))
    , m_runsOn(new QComboBox(this))
    , m_vrStyle(new QComboBox(this))
    , m_noPcNeeded(new QCheckBox(tr("&No PC needed"), this))
    , m_category(new QComboBox(this))
    , m_developer(new QComboBox(this))
    , m_portStatus(new QComboBox(this))
    , m_view(new GameListView(this))
    , m_status(new QLabel(this))
    , m_headset(new HeadsetConnection(this))
    , m_releases(new installer::ReleaseLookup(this))
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
    QAction* settings = file->addAction(tr("&Settings..."), QKeySequence::Preferences, this, [this] {
        SettingsDialog dialog(this);
        dialog.exec();
    });
    settings->setMenuRole(QAction::PreferencesRole);
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
    connect(m_headset, &HeadsetConnection::statusChanged, this, [this] {
        // Only a change of headset or readiness needs a new look at its packages.
        const QString serial = m_headset->isReady() ? m_headset->currentSerial() : QString();
        if (serial != m_headsetQuerySerial)
            findHeadsetGames();
    });
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

    m_keepAwakeAction = toolBar->addAction(HeadsetConnection::keepAwakeIcon(), tr("Keep Awake"));
    m_keepAwakeAction->setCheckable(true);
    m_keepAwakeAction->setToolTip(tr("Keep the headset's screen on, even when it is taken off, until you turn this off"));
    if (auto* button = qobject_cast<QToolButton*>(toolBar->widgetForAction(m_keepAwakeAction)))
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connect(m_keepAwakeAction, &QAction::toggled, m_headset, &HeadsetConnection::setKeepAwake);
    connect(m_headset, &HeadsetConnection::keepAwakeChanged, m_keepAwakeAction, &QAction::setChecked);
    connect(m_headset, &HeadsetConnection::keepAwakeFailed, this, [this](const QString& errorMessage) {
        statusBar()->showMessage(tr("Could not keep the headset's screen on: %1").arg(errorMessage), 10000);
    });
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

    toolBar->addSeparator();

    // Source: filled when a catalog loads, from its platforms.
    addBox(toolBar, tr("&Source:"), m_source);
    m_source->addItem(tr("All sources"), QString());

    addBox(toolBar, tr("S&tore:"), m_store);
    m_store->addItem(tr("Any"), 0);
    m_store->addItem(tr("Sold on Steam"), int(GameFlag::SoldOnSteam));
    m_store->addItem(tr("Sold on GOG"), int(GameFlag::SoldOnGog));
    m_store->addItem(tr("In my Steam library"), int(GameFlag::InSteamLibrary));
    m_store->addItem(tr("In my GOG library"), int(GameFlag::InGogLibrary));
    m_store->setItemData(1, tr("Games that need a PC game sold on Steam"), Qt::ToolTipRole);
    m_store->setItemData(2, tr("Games that need a PC game sold on GOG"), Qt::ToolTipRole);
    m_store->setItemData(3, tr("Games whose PC game is installed through Steam on this PC"), Qt::ToolTipRole);
    m_store->setItemData(4, tr("Games whose PC game is installed through GOG, or owned in GOG Galaxy"), Qt::ToolTipRole);

    addBox(toolBar, tr("&Headset:"), m_onHeadset);
    m_onHeadset->addItem(tr("Any"), int(GameFilter::Headset::Any));
    m_onHeadset->addItem(tr("On my headset"), int(GameFilter::Headset::OnHeadset));
    m_onHeadset->addItem(tr("Installed by qgp"), int(GameFilter::Headset::InstalledByQgp));
    m_onHeadset->addItem(tr("Installed outside qgp"), int(GameFilter::Headset::InstalledOutsideQgp));
    m_onHeadset->addItem(tr("Not on my headset"), int(GameFilter::Headset::NotOnHeadset));
    m_onHeadset->addItem(tr("Update available"), int(GameFilter::Headset::UpdateAvailable));
    m_onHeadset->setItemData(5, tr("Games on your headset with a newer release than the one installed"), Qt::ToolTipRole);

    m_selfContained->setToolTip(tr("Games qgp installs on its own: no files from you, or only files anyone can get for free"));
    toolBar->addWidget(m_selfContained);

    QSettings settings;
    m_store->setCurrentIndex(qMax(0, m_store->findData(settings.value(kStoreSetting, 0).toInt())));
    m_onHeadset->setCurrentIndex(qMax(0, m_onHeadset->findData(settings.value(kHeadsetSetting, 0).toInt())));
    m_selfContained->setChecked(settings.value(kSelfContainedSetting, false).toBool());

    createFilterBar();
    filterChanged();
    for (QComboBox* box : {m_source, m_store, m_onHeadset, m_runsOn, m_vrStyle, m_category, m_developer, m_portStatus})
        connect(box, &QComboBox::currentIndexChanged, this, &MainWindow::filterChanged);
    for (QCheckBox* box : {m_selfContained, m_noPcNeeded})
        connect(box, &QCheckBox::toggled, this, &MainWindow::filterChanged);
}

void MainWindow::createFilterBar()
{
    addToolBarBreak();
    QToolBar* toolBar = addToolBar(tr("Filters"));
    toolBar->setObjectName(QStringLiteral("filtersToolBar"));
    toolBar->setMovable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    // The boxes are filled when a catalog loads.
    addBox(toolBar, tr("&Runs on:"), m_runsOn);
    m_runsOn->setToolTip(tr("Games that run on this headset model"));
    m_runsOn->addItem(tr("All headsets"), QString());
    addBox(toolBar, tr("&VR style:"), m_vrStyle);
    m_vrStyle->addItem(tr("Any"), QString());
    m_noPcNeeded->setToolTip(tr("Games the headset alone can install, without a PC, as QuestPorts judges it"));
    auto* spacer = new QWidget(toolBar);
    spacer->setFixedWidth(12);
    toolBar->addWidget(spacer);
    toolBar->addWidget(m_noPcNeeded);
    toolBar->addSeparator();

    // Rarely used, so hidden until asked for, as on the site.
    m_moreFiltersAction = toolBar->addAction(tr("&More filters"));
    m_moreFiltersAction->setCheckable(true);
    m_moreFiltersAction->setToolTip(tr("Filter by type, developer and status"));
    m_moreFilters << addBox(toolBar, tr("T&ype:"), m_category);
    m_category->addItem(tr("All types"), QString());
    m_moreFilters << addBox(toolBar, tr("&Developer:"), m_developer);
    m_developer->addItem(tr("All developers"), QString());
    // Some names are long; the list still shows them whole.
    m_developer->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_developer->setMinimumContentsLength(20);
    m_moreFilters << addBox(toolBar, tr("St&atus:"), m_portStatus);
    m_portStatus->addItem(tr("All statuses"), QString());
    for (const auto& [id, text] : kStatuses)
        m_portStatus->addItem(tr(text), id);
    toolBar->addSeparator();
    m_resetFiltersAction = toolBar->addAction(tr("R&eset"), this, &MainWindow::resetFilters);
    m_resetFiltersAction->setToolTip(tr("Show every game again"));

    QSettings settings;
    m_noPcNeeded->setChecked(settings.value(kNoPcNeededSetting, false).toBool());
    m_portStatus->setCurrentIndex(qMax(0, m_portStatus->findData(settings.value(kStatusSetting).toString())));
    m_moreFiltersAction->setChecked(settings.value(kMoreFiltersSetting, false).toBool());
    connect(m_moreFiltersAction, &QAction::toggled, this, [this](bool open) {
        QSettings().setValue(kMoreFiltersSetting, open);
        updateMoreFilters();
    });
}

void MainWindow::fillCatalogBoxes(const questports::Catalog& catalog)
{
    QHash<QString, int> platforms;
    QHash<QString, int> styles;
    QHash<QString, int> categories;
    QMap<QString, QString> developers; // Lower case -> the first spelling, sorted without case.
    int noPcNeeded = 0;
    for (const questports::Game& game : catalog.games) {
        ++platforms[game.originPlatform];
        ++styles[game.vrStyle];
        ++categories[game.category];
        if (!game.developer.isEmpty() && !developers.contains(game.developer.toLower()))
            developers.insert(game.developer.toLower(), game.developer);
        noPcNeeded += game.noPcNeeded ? 1 : 0;
    }
    auto counted = [](const QString& text, int count) { return tr("%1 (%2)").arg(text).arg(count); };

    QList<Choice> choices;
    for (const questports::OriginPlatform& platform : catalog.originPlatforms) {
        if (const int count = platforms.value(platform.id))
            choices.append({platform.id, counted(platform.label, count)});
    }
    fillChoiceBox(m_source, tr("All sources"), choices, kSourceSetting);

    choices.clear();
    for (const QString& headset : catalog.headsets) {
        const bool listed = std::any_of(catalog.games.cbegin(), catalog.games.cend(),
                                        [&](const questports::Game& game) { return game.headsets.contains(headset); });
        if (listed)
            choices.append({headset, headset});
    }
    fillChoiceBox(m_runsOn, tr("All headsets"), choices, kRunsOnSetting);

    choices.clear();
    for (const questports::VrStyle& style : catalog.vrStyles) {
        if (const int count = styles.value(style.id))
            choices.append({style.id, counted(style.label, count)});
    }
    fillChoiceBox(m_vrStyle, tr("Any"), choices, kVrStyleSetting);

    choices.clear();
    for (const auto& [id, text] : kCategories) {
        if (const int count = categories.value(id))
            choices.append({id, counted(tr(text), count)});
    }
    fillChoiceBox(m_category, tr("All types"), choices, kCategorySetting);

    choices.clear();
    for (const QString& developer : std::as_const(developers))
        choices.append({developer, developer});
    fillChoiceBox(m_developer, tr("All developers"), choices, kDeveloperSetting);

    m_noPcNeeded->setText(counted(tr("&No PC needed"), noPcNeeded));
}

void MainWindow::resetFilters()
{
    for (QComboBox* box : {m_source, m_store, m_onHeadset, m_runsOn, m_vrStyle, m_category, m_developer, m_portStatus}) {
        const QSignalBlocker blocker(box);
        box->setCurrentIndex(0);
    }
    for (QCheckBox* box : {m_selfContained, m_noPcNeeded}) {
        const QSignalBlocker blocker(box);
        box->setChecked(false);
    }
    filterChanged();
}

void MainWindow::updateMoreFilters()
{
    const GameFilter& filter = m_sorted->filter();
    const int used = int(!filter.category.isEmpty()) + int(!filter.developer.isEmpty()) + int(!filter.status.isEmpty());
    // A filter in use stays in sight, so that a hidden one cannot empty the list.
    if (used > 0 && !m_moreFiltersAction->isChecked()) {
        m_moreFiltersAction->setChecked(true); // Calls this again.
        return;
    }
    m_moreFiltersAction->setText(used > 0 ? tr("&More filters (%1)").arg(used) : tr("&More filters"));
    for (QAction* action : std::as_const(m_moreFilters))
        action->setVisible(m_moreFiltersAction->isChecked());
}

void MainWindow::filterChanged()
{
    GameFilter filter;
    filter.source = m_source->currentData().toString();
    filter.store = GameFlags::fromInt(m_store->currentData().toInt());
    filter.headset = GameFilter::Headset(m_onHeadset->currentData().toInt());
    filter.selfContainedOnly = m_selfContained->isChecked();
    filter.runsOn = m_runsOn->currentData().toString();
    filter.vrStyle = m_vrStyle->currentData().toString();
    filter.noPcNeededOnly = m_noPcNeeded->isChecked();
    filter.category = m_category->currentData().toString();
    filter.developer = m_developer->currentData().toString();
    filter.status = m_portStatus->currentData().toString();
    m_sorted->setFilter(filter);
    m_resetFiltersAction->setEnabled(!(filter == GameFilter{}));
    updateMoreFilters();

    QSettings settings;
    // Boxes the catalog fills must not overwrite the saved choice while they are empty.
    saveChoice(settings, m_source, kSourceSetting);
    saveChoice(settings, m_runsOn, kRunsOnSetting);
    saveChoice(settings, m_vrStyle, kVrStyleSetting);
    saveChoice(settings, m_category, kCategorySetting);
    saveChoice(settings, m_developer, kDeveloperSetting);
    settings.setValue(kStoreSetting, filter.store.toInt());
    settings.setValue(kHeadsetSetting, int(filter.headset));
    settings.setValue(kSelfContainedSetting, filter.selfContainedOnly);
    settings.setValue(kNoPcNeededSetting, filter.noPcNeededOnly);
    settings.setValue(kStatusSetting, filter.status);
    m_view->scrollToTop();
}

// Reads this PC's Steam and GOG libraries and marks the games whose PC game they hold.
void MainWindow::findLibraries()
{
    QStringList warnings;
    const gamestores::GameFinder finder = gamestores::GameFinder::forThisPc(&warnings);
    QHash<QString, GameFlags> holding;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const questports::Game& game = m_model->game(row);
        if (const GameFlags flags = storesHolding(game, finder))
            holding.insert(game.slug, flags);
    }
    m_model->setLibraries(holding);
    m_sorted->refilter();
}

void MainWindow::findHeadsetGames()
{
    const int query = ++m_headsetQuery;
    if (!m_headset->isReady()) {
        m_headsetQuerySerial.clear();
        m_model->setHeadset({});
        m_sorted->refilter();
        return;
    }
    const QString serial = m_headset->currentSerial();
    m_headsetQuerySerial = serial;
    qadb::Client* client = m_headset->client();
    auto* packages = client->packages(serial);
    connect(packages, &qadb::Reply::finished, this, [this, client, packages, serial, query] {
        packages->deleteLater();
        if (query != m_headsetQuery || !packages->isSuccess())
            return;
        QHash<QString, qadb::PackageInfo> installed;
        for (const qadb::PackageInfo& package : packages->packages())
            installed.insert(package.packageName, package);

        auto* records = client->shell(serial, QStringList{QStringLiteral("cat"), QLatin1StringView(installer::kHeadsetRecordsPath)});
        connect(records, &qadb::Reply::finished, this, [this, records, installed, serial, query] {
            records->deleteLater();
            if (query != m_headsetQuery)
                return;
            // No file, or one that cannot be read: nothing was recorded.
            QHash<QString, installer::InstallRecord> recorded;
            if (records->isSuccess()) {
                if (const auto list = installer::parseHeadsetRecords(records->output())) {
                    for (const installer::InstallRecord& record : *list)
                        recorded.insert(record.packageName, record);
                }
            }
            const QStringList installedNames = installed.keys();
            const QStringList recordedNames = recorded.keys();
            const QSet<QString> installedSet(installedNames.cbegin(), installedNames.cend());
            const QSet<QString> recordedSet(recordedNames.cbegin(), recordedNames.cend());
            QHash<QString, GameFlags> onHeadset;
            for (int row = 0; row < m_model->rowCount(); ++row) {
                const questports::Game& game = m_model->game(row);
                if (const GameFlags flags = headsetFlags(game, installedSet, recordedSet))
                    onHeadset.insert(game.slug, flags);
            }
            m_model->setHeadset(onHeadset);
            m_sorted->refilter();
            for (int row = 0; row < m_model->rowCount(); ++row) {
                const questports::Game& game = m_model->game(row);
                if (onHeadset.contains(game.slug))
                    checkForUpdate(game, serial, installed.value(gamePackageName(game)), recorded, query);
            }
        });
    });
}

void MainWindow::checkForUpdate(const questports::Game& game, const QString& serial, const qadb::PackageInfo& package,
                                const QHash<QString, installer::InstallRecord>& recorded, int query)
{
    if (!game.hasRecipe())
        return;
    const auto recipe = installer::parseRecipe(game.recipe, gamePackageName(game));
    const auto source = recipe ? updateSource(*recipe) : std::nullopt;
    if (!source)
        return;
    const std::optional<installer::InstallRecord> record =
        recorded.contains(package.packageName) ? std::optional(recorded.value(package.packageName)) : std::nullopt;

    const QString slug = game.slug;
    auto compare = [this, slug, record, query, source = *source](const qadb::PackageInfo& installed) {
        m_releases->latest(source, this, [this, slug, record, installed, query](const auto& asset, const QString&) {
            // No release found (no network, GitHub's rate limit): no mark.
            if (query != m_headsetQuery || !asset)
                return;
            if (installer::updateStatus(installed, record, asset->tag) == installer::UpdateStatus::UpdateAvailable) {
                m_model->setUpdateAvailable(slug);
                m_sorted->refilter();
            }
        });
    };
    // The package list has no versionName. A record of the same build does not need it.
    if (record && !record->releaseTag.isEmpty() && record->versionCode == package.versionCode) {
        compare(package);
        return;
    }
    auto* info = m_headset->client()->packageInfo(serial, package.packageName);
    connect(info, &qadb::Reply::finished, this, [this, info, query, compare] {
        info->deleteLater();
        if (query == m_headsetQuery && info->isSuccess() && info->isInstalled())
            compare(info->info());
    });
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
    fillCatalogBoxes(*catalog);
    filterChanged();
    findLibraries();
    findHeadsetGames();
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
    // Reloading also looks for new releases.
    m_releases->clear();
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
        dialog = new GameDialog(game, m_headset, m_covers, m_releases, this);
        connect(dialog, &GameDialog::installFinished, this, &MainWindow::findHeadsetGames);
        dialog->show();
    }
    dialog->raise();
    dialog->activateWindow();
}
