#include "GameDialog.h"

#include "CoverCache.h"
#include "GameFlags.h"
#include "HeadsetConnection.h"

#include "gamestores/FilesFolder.h"
#include "gamestores/Finder.h"
#include "installer/ApkFetcher.h"
#include "installer/HeadsetRecords.h"
#include "installer/Runner.h"
#include "installer/Updates.h"
#include "qadb/Client.h"
#include "qadb/Reply.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTime>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

namespace {

const QString kFilesFolderSetting = QStringLiteral("filesFolder");

QString supportText(installer::Support support)
{
    switch (support) {
    case installer::Support::Automatic:
        return GameDialog::tr("qgp installs it");
    case installer::Support::Guided:
        return GameDialog::tr("qgp installs it with your help");
    case installer::Support::Manual:
        return GameDialog::tr("qgp installs part of it; the rest is up to you");
    }
    return {};
}

QString statusText(const QString& status)
{
    if (status == QLatin1StringView("tested"))
        return GameDialog::tr("tested on a headset");
    if (status == QLatin1StringView("reviewed"))
        return GameDialog::tr("reviewed, not yet tested");
    return GameDialog::tr("not yet tested");
}

QString native(const QString& path)
{
    return QDir::toNativeSeparators(path);
}

// Where the PC keeps its cache of the headsets' install records.
QString recordsCachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/install-records.json");
}

QString apkCacheFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/apks");
}

} // namespace

QString GameDialog::filesRoot()
{
    const QString saved = QSettings().value(kFilesFolderSetting).toString();
    return saved.isEmpty() ? gamestores::FilesFolder::defaultRoot() : saved;
}

void GameDialog::setFilesRoot(const QString& folder)
{
    QSettings().setValue(kFilesFolderSetting, QDir::fromNativeSeparators(folder));
}

GameDialog::GameDialog(const questports::Game& game, HeadsetConnection* headset, CoverCache* covers, QWidget* parent)
    : QDialog(parent)
    , m_game(game)
    , m_headset(headset)
    , m_covers(covers)
    , m_cover(new QLabel(this))
    , m_headsetState(new QLabel(this))
    , m_filesBox(new QGroupBox(tr("Files"), this))
    , m_filesGrid(new QGridLayout)
    , m_installButton(new QPushButton(tr("&Install"), this))
    , m_cancelButton(new QPushButton(tr("Stop"), this))
    , m_installHint(new QLabel(this))
    , m_progressArea(new QWidget(this))
    , m_step(new QLabel(this))
    , m_progress(new QProgressBar(this))
    , m_tellArea(new QWidget(this))
    , m_tellText(new QLabel(this))
    , m_log(new QPlainTextEdit(this))
{
    setWindowTitle(m_game.title);
    setAttribute(Qt::WA_DeleteOnClose);

    if (m_game.hasRecipe())
        m_recipe = installer::parseRecipe(m_game.recipe, gamePackageName(m_game), &m_recipeError);

    // Header: the cover, then the title and what qgp knows about the game.
    m_cover->setPixmap(m_covers->cover(m_game.coverUrl));
    connect(m_covers, &CoverCache::coverReady, this, [this](const QUrl& url) {
        if (url == m_game.coverUrl)
            m_cover->setPixmap(m_covers->cover(url));
    });
    auto* title = new QLabel(m_game.title, this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setWordWrap(true);

    auto* about = new QLabel(this);
    about->setWordWrap(true);
    QStringList lines;
    if (!m_game.developer.isEmpty())
        lines.append(tr("By %1").arg(m_game.developer.toHtmlEscaped()));
    if (m_recipe) {
        lines.append(tr("%1. The recipe is %2.").arg(supportText(m_recipe->support), statusText(m_recipe->status)));
        lines.append(tr("Package: %1").arg(m_recipe->packageName.toHtmlEscaped()));
    } else if (m_game.hasRecipe()) {
        lines.append(tr("The recipe cannot be read: %1").arg(m_recipeError.toHtmlEscaped()));
    } else {
        lines.append(tr("qgp has no install recipe for this game yet."));
    }
    lines.append(QStringLiteral("<a href=\"https://questports.com/ports/%1\">%2</a>")
                     .arg(QString::fromUtf8(QUrl::toPercentEncoding(m_game.slug)), tr("QuestPorts page")));
    about->setText(lines.join(QStringLiteral("<br>")));
    about->setTextFormat(Qt::RichText);
    about->setOpenExternalLinks(true);
    m_headsetState->setWordWrap(true);

    auto* headerText = new QVBoxLayout;
    headerText->addWidget(title);
    headerText->addWidget(about);
    headerText->addWidget(m_headsetState);
    headerText->addStretch();
    auto* header = new QHBoxLayout;
    header->addWidget(m_cover, 0, Qt::AlignTop);
    header->addLayout(headerText, 1);

    // Files: one row per input, and the files folder.
    auto* filesLayout = new QVBoxLayout(m_filesBox);
    m_filesGrid->setColumnStretch(0, 2);
    m_filesGrid->setColumnStretch(1, 3);
    filesLayout->addLayout(m_filesGrid);
    auto* filesButtons = new QHBoxLayout;
    auto* openFolder = new QPushButton(tr("Open Files &Folder"), this);
    openFolder->setToolTip(tr("Create this game's folder in your files folder, with a guide to what goes there, and open it"));
    connect(openFolder, &QPushButton::clicked, this, &GameDialog::openFilesFolder);
    auto* lookAgain = new QPushButton(tr("&Look Again"), this);
    lookAgain->setToolTip(tr("Look for the files again in Steam, GOG and your files folder"));
    connect(lookAgain, &QPushButton::clicked, this, &GameDialog::findInputs);
    filesButtons->addWidget(openFolder);
    filesButtons->addWidget(lookAgain);
    filesButtons->addStretch();
    filesLayout->addLayout(filesButtons);
    m_filesBox->setVisible(m_recipe && !m_recipe->inputs.isEmpty());

    // Install: the button, then the progress, the user's turn and the log.
    m_installHint->setWordWrap(true);
    connect(m_installButton, &QPushButton::clicked, this, &GameDialog::install);
    connect(m_cancelButton, &QPushButton::clicked, this, &GameDialog::cancel);
    m_cancelButton->hide();
    auto* installRow = new QHBoxLayout;
    installRow->addWidget(m_installButton);
    installRow->addWidget(m_cancelButton);
    installRow->addWidget(m_installHint, 1);

    auto* progressLayout = new QVBoxLayout(m_progressArea);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    m_step->setWordWrap(true);
    progressLayout->addWidget(m_step);
    progressLayout->addWidget(m_progress);
    m_progressArea->hide();

    auto* tellLayout = new QHBoxLayout(m_tellArea);
    m_tellText->setWordWrap(true);
    m_tellText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* continueButton = new QPushButton(tr("&Done, Continue"), this);
    connect(continueButton, &QPushButton::clicked, this, [this] {
        m_tellArea->hide();
        log(tr("Continued."));
        if (m_runner)
            m_runner->continueAfterTell();
    });
    tellLayout->addWidget(m_tellText, 1);
    tellLayout->addWidget(continueButton, 0, Qt::AlignTop);
    m_tellArea->setAutoFillBackground(true);
    m_tellArea->setBackgroundRole(QPalette::AlternateBase);
    m_tellArea->hide();

    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    m_log->setMinimumHeight(120);
    m_log->hide();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &GameDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(header);
    layout->addWidget(m_filesBox);
    layout->addLayout(installRow);
    layout->addWidget(m_progressArea);
    layout->addWidget(m_tellArea);
    layout->addWidget(m_log, 1);
    layout->addWidget(buttons);
    resize(720, sizeHint().height());

    connect(m_headset, &HeadsetConnection::statusChanged, this, [this] {
        showHeadsetState();
        updateButtons();
    });
    findInputs();
    showHeadsetState();
}

void GameDialog::findInputs()
{
    if (!m_recipe || m_running)
        return;
    std::unique_ptr<gamestores::GameFinder> finder;
    const bool needsStores = std::any_of(m_recipe->inputs.cbegin(), m_recipe->inputs.cend(), [](const installer::Input& input) {
        return input.kind == installer::Input::Kind::PcInstall;
    });
    if (needsStores)
        finder = std::make_unique<gamestores::GameFinder>(gamestores::GameFinder::forThisPc());
    m_inputs = InputResolver(finder.get(), filesRoot()).resolve(m_game.slug, *m_recipe);
    showInputs();
    updateButtons();
}

void GameDialog::showInputs()
{
    while (QLayoutItem* item = m_filesGrid->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    for (int row = 0; row < m_inputs.size(); ++row) {
        const ResolvedInput& resolved = m_inputs.at(row);
        const installer::Input& input = resolved.input;

        QString note;
        if (!input.group.isEmpty())
            note += tr(" (at least one of these)");
        else if (!input.required)
            note += tr(" (optional)");
        if (input.json.value(QLatin1StringView("free")).toBool())
            note += tr(" (free)");
        auto* label = new QLabel(QStringLiteral("<b>%1</b>%2").arg(input.label.toHtmlEscaped(), note), m_filesBox);
        label->setWordWrap(true);
        label->setToolTip(input.help);

        QString state;
        switch (resolved.state) {
        case ResolvedInput::State::Found:
            if (input.kind == installer::Input::Kind::PcInstall)
                state = tr("Found in %1: %2").arg(resolved.origin, native(resolved.source.folder));
            else if (resolved.source.files.size() == 1)
                state = tr("Found in %1: %2").arg(resolved.origin, native(resolved.source.files.first()));
            else
                state = tr("Found in %1: %n file(s) in %2", nullptr, int(resolved.source.files.size()))
                            .arg(resolved.origin, native(QFileInfo(resolved.source.files.first()).path()));
            break;
        case ResolvedInput::State::Ambiguous:
            state = tr("%n candidate(s) fit; choose the right one.", nullptr, int(resolved.candidates.size()));
            break;
        case ResolvedInput::State::Missing:
            if (!resolved.partial.isEmpty())
                state = tr("Missing: %1").arg(resolved.missingNames.join(QStringLiteral(", ")));
            else
                state = input.required ? tr("Not found.") : tr("Not found; the install goes on without it.");
            break;
        }
        // Links to the Steam and GOG pages of the PC game, when the recipe names the stores.
        const QList<StorePage> pages = storePages(input.json);
        QStringList links;
        for (int i = 0; i < pages.size(); ++i) {
            const QString name = pages[i].store == QLatin1StringView("steam") ? tr("Steam page") : tr("GOG page");
            links.append(QStringLiteral("<a href=\"%1\">%2</a>").arg(i).arg(name));
        }
        QString html = state.toHtmlEscaped();
        if (!links.isEmpty())
            html += QStringLiteral("<br>") + links.join(QStringLiteral(" &middot; "));
        auto* status = new QLabel(html, m_filesBox);
        status->setTextFormat(Qt::RichText);
        status->setWordWrap(true);
        status->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        connect(status, &QLabel::linkActivated, this, [this, pages](const QString& link) {
            const int index = link.toInt();
            if (index >= 0 && index < pages.size())
                openStorePage(pages[index]);
        });
        if (resolved.state != ResolvedInput::State::Found && input.required)
            status->setStyleSheet(QStringLiteral("color: palette(link)"));
        QStringList details;
        if (!input.help.isEmpty())
            details.append(input.help);
        if (!resolved.candidates.isEmpty())
            details.append(resolved.candidates.join(u'\n'));
        status->setToolTip(details.join(QStringLiteral("\n\n")));

        auto* choose = new QPushButton(tr("Choose..."), m_filesBox);
        connect(choose, &QPushButton::clicked, this, [this, row] { chooseInput(row); });
        choose->setEnabled(!m_running);

        m_filesGrid->addWidget(label, row, 0);
        m_filesGrid->addWidget(status, row, 1);
        m_filesGrid->addWidget(choose, row, 2, Qt::AlignTop);
    }
}

void GameDialog::chooseInput(int index)
{
    if (index < 0 || index >= m_inputs.size())
        return;
    ResolvedInput& resolved = m_inputs[index];
    const installer::Input& input = resolved.input;

    // Start where the files are, or else in the game's files folder.
    QString start = gamestores::FilesFolder(filesRoot()).gameFolder(m_game.slug);
    if (!resolved.candidates.isEmpty())
        start = QFileInfo(resolved.candidates.first()).path();
    else if (!resolved.source.folder.isEmpty())
        start = resolved.source.folder;
    else if (!resolved.source.files.isEmpty())
        start = QFileInfo(resolved.source.files.first()).path();
    if (!QFileInfo(start).isDir())
        start = QDir::homePath();

    QString error;
    bool ok = false;
    if (input.kind == installer::Input::Kind::PcInstall) {
        const QString folder = QFileDialog::getExistingDirectory(this, input.label, start);
        if (folder.isEmpty())
            return;
        ok = InputResolver::chooseFolder(resolved, folder, &error);
    } else {
        QStringList patterns;
        for (const QString& name : input.names)
            patterns.append(name);
        for (const QString& extension : input.extensions)
            patterns.append(u'*' + extension);
        const QString filter = QStringLiteral("%1 (%2);;%3").arg(input.label, patterns.join(u' '), tr("All files (*)"));
        const bool several = input.multiple || input.names.size() > 1;
        QStringList files;
        if (several) {
            files = QFileDialog::getOpenFileNames(this, input.label, start, filter);
        } else {
            const QString file = QFileDialog::getOpenFileName(this, input.label, start, filter);
            if (!file.isEmpty())
                files.append(file);
        }
        if (files.isEmpty())
            return;
        ok = InputResolver::chooseFiles(resolved, files, &error);
    }
    if (!ok)
        QMessageBox::warning(this, input.label, error);
    showInputs();
    updateButtons();
}

void GameDialog::openFilesFolder()
{
    if (!m_recipe)
        return;
    QJsonArray inputs;
    for (const installer::Input& input : m_recipe->inputs)
        inputs.append(input.json);
    const gamestores::FilesFolder files(filesRoot());
    QString error;
    if (!files.create(m_game.slug, m_game.title, inputs, &error)) {
        QMessageBox::warning(this, tr("Files Folder"), error);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(files.gameFolder(m_game.slug)));
}

void GameDialog::openStorePage(const StorePage& page)
{
    const QUrl lookup = page.gogPageLookup();
    if (lookup.isEmpty()) {
        QDesktopServices::openUrl(page.url());
        return;
    }
    // GOG's pages are named by slug, not by product ID: ask GOG's product API for the address,
    // and search the store when it does not answer.
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    QNetworkReply* reply = m_network->get(QNetworkRequest(lookup));
    connect(reply, &QNetworkReply::finished, this, [reply, page] {
        reply->deleteLater();
        const QString card = QJsonDocument::fromJson(reply->readAll())
                                 .object()
                                 .value(QLatin1StringView("links"))
                                 .toObject()
                                 .value(QLatin1StringView("product_card"))
                                 .toString();
        const QUrl url(card);
        QDesktopServices::openUrl(reply->error() == QNetworkReply::NoError && url.isValid() && !card.isEmpty()
                                      ? url
                                      : page.url());
    });
}

void GameDialog::showHeadsetState()
{
    if (!m_headset->isReady()) {
        m_headsetState->setText(tr("Headset: %1").arg(m_headset->statusText()));
        return;
    }
    const QString name = m_headset->currentDevice() ? HeadsetConnection::displayName(*m_headset->currentDevice())
                                                    : tr("the headset");
    const QString package = m_recipe ? m_recipe->packageName : gamePackageName(m_game);
    if (package.isEmpty()) {
        m_headsetState->setText(tr("Headset: %1").arg(m_headset->statusText()));
        return;
    }
    m_headsetState->setText(tr("Checking %1...").arg(name));
    qadb::Client* client = m_headset->client();
    const QString serial = m_headset->currentSerial();
    auto* reply = client->packageInfo(serial, package);
    connect(reply, &qadb::Reply::finished, this, [this, client, serial, reply, name, package] {
        reply->deleteLater();
        if (!reply->isSuccess()) {
            m_headsetState->setText(tr("Cannot read %1: %2").arg(name, reply->errorString()));
            return;
        }
        if (!reply->isInstalled()) {
            m_headsetState->setText(tr("Not installed on %1.").arg(name));
            return;
        }
        const QString version = reply->info().versionName.isEmpty() ? QString::number(reply->info().versionCode)
                                                                    : reply->info().versionName;
        // Whether qgp installed it: the headset's install records say.
        auto* records = client->shell(serial, QStringList{QStringLiteral("cat"), QLatin1StringView(installer::kHeadsetRecordsPath)});
        connect(records, &qadb::Reply::finished, this, [this, records, name, package, version] {
            records->deleteLater();
            bool byQgp = false;
            if (records->isSuccess()) {
                if (const auto list = installer::parseHeadsetRecords(records->output())) {
                    byQgp = std::any_of(list->cbegin(), list->cend(), [&](const installer::InstallRecord& record) {
                        return record.packageName == package;
                    });
                }
            }
            m_headsetState->setText(byQgp ? tr("Installed on %1 by qgp: version %2.").arg(name, version)
                                          : tr("Installed on %1 outside qgp: version %2.").arg(name, version));
        });
    });
}

void GameDialog::updateButtons()
{
    QString hint;
    if (!m_recipe)
        hint = m_game.hasRecipe() ? tr("The recipe cannot be read.") : tr("There is no recipe to install with.");
    else if (m_recipe->apks.isEmpty())
        hint = tr("The recipe names no APK.");
    else if (!m_headset->isReady())
        hint = m_headset->statusAdvice().isEmpty() ? m_headset->statusText() : m_headset->statusAdvice();
    else if (!InputResolver::ready(m_inputs))
        hint = tr("Some files are missing. Choose them, or put them in your files folder and look again.");

    m_installButton->setEnabled(!m_running && hint.isEmpty());
    m_installButton->setVisible(!m_running);
    m_cancelButton->setVisible(m_running);
    m_installHint->setText(m_running ? QString() : hint);
}

// --- Installing --------------------------------------------------------------------------------

void GameDialog::install()
{
    if (m_running || !m_recipe || !m_headset->isReady())
        return;
    m_running = true;
    m_serial = m_headset->currentSerial();
    m_apkFiles.clear();
    m_releaseTag.clear();
    m_asset.clear();
    m_log->clear();
    m_log->show();
    m_progressArea->show();
    m_tellArea->hide();
    showInputs(); // Disables the Choose buttons.
    updateButtons();
    log(tr("Installing %1 on %2.").arg(m_game.title, m_serial));
    fetchApk(0);
}

void GameDialog::fetchApk(int index)
{
    if (!m_running)
        return;
    if (index >= m_recipe->apks.size()) {
        runSteps();
        return;
    }
    const installer::ApkSource& source = m_recipe->apks.at(index);

    if (source.kind == installer::ApkSource::Kind::Manual) {
        m_step->setText(tr("Choose the APK"));
        log(tr("This APK is a manual download from %1.").arg(source.page));
        if (!source.help.isEmpty())
            log(source.help);
        const QString apk = QFileDialog::getOpenFileName(this, tr("Choose the APK (%1)").arg(source.page), QDir::homePath(),
                                                         tr("Android apps (*.apk);;All files (*)"));
        if (apk.isEmpty()) {
            finishInstall(false, tr("No APK was chosen."));
            return;
        }
        m_apkFiles.append(apk);
        if (index == 0)
            m_asset = QFileInfo(apk).fileName();
        fetchApk(index + 1);
        return;
    }

    m_step->setText(tr("Downloading the APK"));
    m_progress->setRange(0, 0);
    m_fetcher = new installer::ApkFetcher(apkCacheFolder(), this);
    connect(m_fetcher, &installer::ApkFetcher::resolved, this, [this, index](const QString& tag, const QString& name) {
        log(tag.isEmpty() ? tr("APK: %1").arg(name) : tr("Release %1, APK %2").arg(tag, name));
        if (index == 0) {
            m_releaseTag = tag;
            m_asset = name;
        }
    });
    connect(m_fetcher, &installer::ApkFetcher::progress, this, [this](qint64 done, qint64 total) {
        if (total > 0) {
            m_progress->setRange(0, 1000);
            m_progress->setValue(int(done * 1000 / total));
        }
    });
    connect(m_fetcher, &installer::ApkFetcher::finished, this,
            [this, index](bool success, const QString& apkPath, const QString& errorMessage) {
                m_fetcher->deleteLater();
                if (!success) {
                    finishInstall(false, errorMessage);
                    return;
                }
                log(tr("Downloaded to %1").arg(native(apkPath)));
                m_apkFiles.append(apkPath);
                fetchApk(index + 1);
            });
    m_fetcher->fetch(source);
}

void GameDialog::runSteps()
{
    if (!m_running)
        return;
    if (m_headset->currentSerial() != m_serial || !m_headset->isReady()) {
        finishInstall(false, tr("The headset is no longer connected."));
        return;
    }
    m_runner = new installer::Runner(m_headset->client(), m_serial, *m_recipe, m_apkFiles,
                                     InputResolver::sources(m_inputs), this);
    connect(m_runner, &installer::Runner::stepStarted, this, [this](int index, int count, const QString& text) {
        m_step->setText(tr("Step %1 of %2: %3").arg(index + 1).arg(count).arg(text));
        m_progress->setRange(0, 0);
        log(m_step->text());
    });
    connect(m_runner, &installer::Runner::progress, this, [this](qint64 done, qint64 total) {
        if (total > 0) {
            m_progress->setRange(0, 1000);
            m_progress->setValue(int(done * 1000 / total));
        }
    });
    connect(m_runner, &installer::Runner::message, this, &GameDialog::log);
    connect(m_runner, &installer::Runner::waitingForUser, this, [this](const QString& text) {
        m_tellText->setText(text);
        m_tellArea->show();
        log(tr("In the headset: %1").arg(text));
    });
    connect(m_runner, &installer::Runner::finished, this, [this](bool success, const QString& errorMessage) {
        m_runner->deleteLater();
        m_tellArea->hide();
        if (success)
            recordInstall();
        else
            finishInstall(false, errorMessage);
    });
    m_runner->start();
}

void GameDialog::recordInstall()
{
    m_step->setText(tr("Recording the install on the headset"));
    m_progress->setRange(0, 0);
    qadb::Client* client = m_headset->client();
    auto* hardware = client->getProperty(m_serial, QStringLiteral("ro.serialno"));
    connect(hardware, &qadb::Reply::finished, this, [this, client, hardware] {
        hardware->deleteLater();
        const QString hardwareSerial = hardware->outputText().trimmed();
        auto* info = client->packageInfo(m_serial, m_recipe->packageName);
        connect(info, &qadb::Reply::finished, this, [this, client, info, hardwareSerial] {
            info->deleteLater();
            if (hardwareSerial.isEmpty() || !info->isSuccess() || !info->isInstalled()) {
                // The game is installed; only the record is missing.
                finishInstall(true, tr("Installed, but the install could not be recorded: %1")
                                        .arg(info->isSuccess() ? tr("the package was not found") : info->errorString()));
                return;
            }
            installer::InstallRecord record;
            record.packageName = m_recipe->packageName;
            record.slug = m_game.slug;
            record.releaseTag = m_releaseTag;
            record.asset = m_asset;
            record.versionCode = info->info().versionCode;
            record.versionName = info->info().versionName;
            record.installedAt = QDateTime::currentDateTimeUtc();

            auto cache = std::make_shared<installer::InstallRecords>(installer::InstallRecords::load(recordsCachePath()));
            auto* sync = new installer::HeadsetRecordSync(client, m_serial, hardwareSerial, cache.get(), this);
            connect(sync, &installer::HeadsetRecordSync::finished, this,
                    [this, sync, cache, record](bool success, const QString& errorMessage) {
                        sync->deleteLater();
                        const QString version = record.versionName.isEmpty() ? QString::number(record.versionCode)
                                                                             : record.versionName;
                        if (success)
                            finishInstall(true, tr("Installed version %1.").arg(version));
                        else
                            finishInstall(true, tr("Installed version %1, but the install could not be recorded: %2")
                                                    .arg(version, errorMessage));
                    });
            sync->run(record);
        });
    });
}

void GameDialog::cancel()
{
    if (!m_running)
        return;
    // Disconnect first, so that their finished() does not report the stop as a failure.
    if (m_fetcher) {
        m_fetcher->disconnect(this);
        m_fetcher->abort();
    }
    if (m_runner) {
        m_runner->disconnect(this);
        m_runner->cancel();
    }
    finishInstall(false, tr("Stopped."));
}

void GameDialog::finishInstall(bool success, const QString& message)
{
    if (!m_running)
        return;
    m_running = false;
    if (m_fetcher) {
        m_fetcher->disconnect(this);
        m_fetcher->deleteLater();
    }
    if (m_runner) {
        m_runner->disconnect(this);
        m_runner->deleteLater();
    }
    m_tellArea->hide();
    m_progress->setRange(0, 1);
    m_progress->setValue(success ? 1 : 0);
    m_step->setText(success ? tr("Done. %1").arg(message) : tr("The install failed: %1").arg(message));
    log(m_step->text());
    showInputs();
    updateButtons();
    showHeadsetState();
    emit installFinished();
}

void GameDialog::log(const QString& text)
{
    m_log->appendPlainText(QStringLiteral("%1  %2").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), text));
}

bool GameDialog::confirmStop()
{
    if (!m_running)
        return true;
    const auto answer = QMessageBox::question(this, m_game.title,
                                              tr("The install is still running. Stop it?"));
    if (answer != QMessageBox::Yes)
        return false;
    cancel();
    return true;
}

void GameDialog::closeEvent(QCloseEvent* event)
{
    if (confirmStop())
        event->accept();
    else
        event->ignore();
}

void GameDialog::reject()
{
    if (confirmStop())
        QDialog::reject();
}
