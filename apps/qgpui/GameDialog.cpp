#include "GameDialog.h"

#include "CoverCache.h"
#include "GameFlags.h"
#include "GuideView.h"
#include "HeadsetConnection.h"
#include "SettingsDialog.h"

#include "gamestores/FilesFolder.h"
#include "gamestores/Finder.h"
#include "installer/ApkFetcher.h"
#include "installer/HeadsetRecords.h"
#include "installer/InstallPlan.h"
#include "installer/KeepAwake.h"
#include "installer/ReleaseLookup.h"
#include "installer/Runner.h"
#include "installer/Updates.h"
#include "qadb/Client.h"
#include "qadb/Reply.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QResizeEvent>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTime>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

namespace {

// A word-wrapped label that asks its layout for the height its text takes at its actual width.
// Layouts give a wrapped rich-text label too little height when its width changes after the
// first layout, and the last line is cut off (seen on macOS, whose fonts are larger).
class WrappedLabel : public QLabel
{
public:
    using QLabel::QLabel;

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QLabel::resizeEvent(event);
        const int needed = heightForWidth(width());
        if (needed > 0 && needed != minimumHeight())
            setMinimumHeight(needed);
    }
};

const QString kFilesFolderSetting = QStringLiteral("filesFolder");

QString supportText(installer::Support support)
{
    switch (support) {
    case installer::Support::Automatic:
        return GameDialog::tr("QGPUI installs it");
    case installer::Support::Guided:
        return GameDialog::tr("QGPUI installs it with your help");
    case installer::Support::Manual:
        return GameDialog::tr("QGPUI installs part of it; the rest is up to you");
    }
    return {};
}

// The site's names for its port types, for one game.
QString categoryText(const QString& category)
{
    if (category == QLatin1StringView("source_port"))
        return GameDialog::tr("Source port");
    if (category == QLatin1StringView("decompilation"))
        return GameDialog::tr("Decomp");
    if (category == QLatin1StringView("engine_recreation"))
        return GameDialog::tr("Engine recreation");
    if (category == QLatin1StringView("emulator"))
        return GameDialog::tr("Emulator");
    if (category == QLatin1StringView("wrapper"))
        return GameDialog::tr("Wrapper");
    if (category == QLatin1StringView("vr_injection"))
        return GameDialog::tr("VR mod");
    if (category == QLatin1StringView("game_mod"))
        return GameDialog::tr("Game mod");
    QString text = category;
    return text.replace(QLatin1Char('_'), QLatin1Char(' '));
}

// Badge colours as "r, g, b". Translucent tints read on both light and dark palettes, as the
// result banner's do.
const QString kGreen = QStringLiteral("46, 160, 67");
const QString kBlue = QStringLiteral("56, 132, 255");
const QString kPurple = QStringLiteral("137, 87, 229");
const QString kOrange = QStringLiteral("210, 153, 34");
const QString kRed = QStringLiteral("218, 54, 51");
const QString kGrey = QStringLiteral("128, 128, 128");

// The port's status, and the colour of its badge.
std::pair<QString, QString> portStatusBadge(const QString& status)
{
    if (status == QLatin1StringView("released"))
        return {GameDialog::tr("Released"), kGreen};
    if (status == QLatin1StringView("playable_beta"))
        return {GameDialog::tr("Playable beta"), kOrange};
    if (status == QLatin1StringView("in_development"))
        return {GameDialog::tr("In development"), kRed};
    QString text = status;
    return {text.replace(QLatin1Char('_'), QLatin1Char(' ')), kGrey};
}

// Sets a badge's text, tooltip and colour.
void setBadge(QLabel* badge, const QString& text, const QString& toolTip, const QString& colour)
{
    badge->setText(text);
    badge->setToolTip(toolTip);
    badge->setStyleSheet(QStringLiteral("QLabel { background: rgba(%1, 0.18); border: 1px solid rgba(%1, 0.7);"
                                        " border-radius: 9px; padding: 1px 8px; }")
                             .arg(colour));
}

// Plain text as HTML, with its web addresses as links. An address ends before closing punctuation,
// so that "(https://x.test)." links only the address.
QString linkified(const QString& text)
{
    static const QRegularExpression url(QStringLiteral(R"(https?:\/\/[^\s<>()]*[^\s<>().,;:!?'"])"));
    QString html;
    qsizetype last = 0;
    for (auto it = url.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        html += text.mid(last, match.capturedStart() - last).toHtmlEscaped();
        const QString address = match.captured().toHtmlEscaped();
        html += QStringLiteral("<a href=\"%1\">%1</a>").arg(address);
        last = match.capturedEnd();
    }
    html += text.mid(last).toHtmlEscaped();
    return html.replace(u'\n', QStringLiteral("<br>"));
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

GameDialog::GameDialog(const questports::Game& game, HeadsetConnection* headset, CoverCache* covers,
                       installer::ReleaseLookup* releases, const QString& sourceLabel, QWidget* parent)
    : QDialog(parent)
    , m_game(game)
    , m_headset(headset)
    , m_covers(covers)
    , m_releases(releases)
    , m_cover(new QLabel(this))
    , m_headsetState(new QLabel(this))
    , m_installedBadge(new QLabel(this))
    , m_updateBadge(new QLabel(this))
    , m_filesBox(new QGroupBox(tr("Files"), this))
    , m_filesGrid(new QGridLayout)
    , m_checkFilesButton(new QPushButton(tr("Check Files &Again"), m_filesBox))
    , m_openFolderButton(new QPushButton(tr("Custom &Files Folder..."), m_filesBox))
    , m_installButton(new QPushButton(tr("&Install"), this))
    , m_cancelButton(new QPushButton(tr("Stop"), this))
    , m_detailsButton(new QPushButton(tr("&Details..."), this))
    , m_installHint(new QLabel(this))
    , m_progressArea(new QWidget(this))
    , m_step(new QLabel(this))
    , m_progress(new QProgressBar(this))
    , m_resultArea(new QWidget(this))
    , m_resultIcon(new QLabel(this))
    , m_resultText(new QLabel(this))
    , m_resultNext(new QLabel(this))
    , m_tellArea(new QWidget(this))
    , m_tellText(new QLabel(this))
    , m_continueButton(new QPushButton(tr("&Done, Continue"), this))
    , m_log(new QPlainTextEdit(this))
{
    setWindowTitle(m_game.title);
    setAttribute(Qt::WA_DeleteOnClose);

    if (m_game.hasRecipe())
        m_recipe = installer::parseRecipe(m_game.recipe, gamePackageName(m_game), &m_recipeError);

    // Header: the cover, then the title and what QGPUI knows about the game.
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

    // Badges: the game on the headset, then the catalog's source, type and status.
    auto* badges = new QHBoxLayout;
    badges->setSpacing(6);
    badges->addWidget(m_installedBadge);
    setBadge(m_updateBadge, tr("Update available"), tr("A newer release of the game's APK is out"), kOrange);
    badges->addWidget(m_updateBadge);
    m_installedBadge->hide();
    m_updateBadge->hide();
    const auto addBadge = [this, badges](const QString& text, const QString& toolTip, const QString& colour) {
        auto* badge = new QLabel(this);
        setBadge(badge, text, toolTip, colour);
        badges->addWidget(badge);
    };
    if (!m_game.originPlatform.isEmpty()) {
        const QString source = sourceLabel.isEmpty() ? m_game.originPlatform : sourceLabel;
        addBadge(source, tr("Source: the platform the game was first made for"), kBlue);
    }
    if (!m_game.category.isEmpty())
        addBadge(categoryText(m_game.category), tr("Type of port"), kPurple);
    if (!m_game.status.isEmpty()) {
        const auto [text, colour] = portStatusBadge(m_game.status);
        addBadge(text, tr("Status of the port"), colour);
    }
    badges->addStretch();

    // Where to get the original game (the port row's base_game_store), highlighted: most games
    // need the user's own copy. It sits just above the Install button, where the user decides.
    // The port's base_game_url, when it is a web address, follows as a link to the store page.
    auto* baseGame = new QLabel(this);
    const QString baseGameStore = m_game.port.value(QLatin1StringView("base_game_store")).toString().trimmed();
    const QUrl baseGameUrl(m_game.port.value(QLatin1StringView("base_game_url")).toString().trimmed());
    const bool hasBaseGameUrl = baseGameUrl.isValid() && !baseGameUrl.host().isEmpty()
        && (baseGameUrl.scheme() == QLatin1StringView("https") || baseGameUrl.scheme() == QLatin1StringView("http"));
    QStringList baseGameParts;
    if (!baseGameStore.isEmpty())
        baseGameParts.append(baseGameStore.toHtmlEscaped());
    if (hasBaseGameUrl)
        baseGameParts.append(QStringLiteral("<a href=\"%1\">%2</a>")
                                 .arg(QString::fromUtf8(baseGameUrl.toEncoded()).toHtmlEscaped(), tr("Store page")));
    baseGame->setText(tr("<b>Original game:</b> %1").arg(baseGameParts.join(QStringLiteral(" &middot; "))));
    baseGame->setTextFormat(Qt::RichText);
    baseGame->setWordWrap(true);
    baseGame->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    baseGame->setOpenExternalLinks(true);
    baseGame->setStyleSheet(QStringLiteral("QLabel { background: rgba(%1, 0.15); border-left: 3px solid rgb(%1);"
                                           " padding: 4px 8px; }")
                                .arg(kOrange));
    baseGame->setVisible(!baseGameParts.isEmpty());

    auto* about = new WrappedLabel(this);
    about->setWordWrap(true);
    QStringList lines;
    if (!m_game.developer.isEmpty())
        lines.append(tr("By %1").arg(m_game.developer.toHtmlEscaped()));
    if (m_recipe) {
        lines.append(tr("%1.").arg(supportText(m_recipe->support)));
        lines.append(tr("Package: %1").arg(m_recipe->packageName.toHtmlEscaped()));
    } else if (m_game.hasRecipe()) {
        lines.append(tr("The recipe cannot be read: %1").arg(m_recipeError.toHtmlEscaped()));
    } else {
        lines.append(tr("QGPUI has no install recipe for this game yet."));
    }
    // Every external link on one line: QuestPorts, then the Steam, GOG and Epic pages of each PC game the
    // recipe needs. Store links are "store:<index>", because a GOG page needs a lookup first.
    QStringList links{QStringLiteral("<a href=\"https://questports.com/ports/%1\">%2</a>")
                          .arg(QString::fromUtf8(QUrl::toPercentEncoding(m_game.slug)), tr("QuestPorts page"))};
    QList<StorePage> pages;
    if (m_recipe) {
        int pcInputs = 0;
        for (const installer::Input& input : m_recipe->inputs)
            pcInputs += storePages(input.json).isEmpty() ? 0 : 1;
        for (const installer::Input& input : m_recipe->inputs) {
            for (const StorePage& page : storePages(input.json)) {
                const QString store = page.store == QLatin1StringView("steam") ? tr("Steam")
                                      : page.store == QLatin1StringView("gog") ? tr("GOG")
                                                                               : tr("Epic");
                // Name the PC game when the recipe needs more than one.
                const QString text = pcInputs > 1 ? tr("%1: %2").arg(store, input.label) : tr("%1 page").arg(store);
                links.append(QStringLiteral("<a href=\"store:%1\">%2</a>").arg(pages.size()).arg(text.toHtmlEscaped()));
                pages.append(page);
            }
        }
    }
    lines.append(links.join(QStringLiteral(" &middot; ")));
    about->setText(lines.join(QStringLiteral("<br>")));
    about->setTextFormat(Qt::RichText);
    connect(about, &QLabel::linkActivated, this, [this, pages](const QString& link) {
        if (link.startsWith(QLatin1StringView("store:"))) {
            const int index = link.mid(6).toInt();
            if (index >= 0 && index < pages.size())
                openStorePage(pages[index]);
            return;
        }
        QDesktopServices::openUrl(QUrl(link));
    });
    m_headsetState->setWordWrap(true);

    // Ways to install the game without QGPUI, such as the port's own installer, highlighted: a user
    // who reads the port's guide expects them, and some need fewer steps than QGPUI's way.
    auto* otherWays = new QLabel(this);
    if (m_recipe && !m_recipe->otherWays.isEmpty()) {
        QStringList ways;
        for (const installer::OtherWay& way : m_recipe->otherWays) {
            ways.append(QStringLiteral("<a href=\"%1\">%2</a>: %3")
                            .arg(way.url.toHtmlEscaped(), way.label.toHtmlEscaped(), way.help.toHtmlEscaped()));
        }
        const QString heading = ways.size() == 1 ? tr("Another way to install:") : tr("Other ways to install:");
        otherWays->setText(QStringLiteral("<b>%1</b> %2")
                               .arg(heading.toHtmlEscaped(),
                                    ways.size() == 1 ? ways.first()
                                                     : QStringLiteral("<br>") + ways.join(QStringLiteral("<br>"))));
    }
    otherWays->setTextFormat(Qt::RichText);
    otherWays->setWordWrap(true);
    otherWays->setOpenExternalLinks(true);
    otherWays->setTextInteractionFlags(Qt::TextBrowserInteraction);
    otherWays->setStyleSheet(QStringLiteral("QLabel { background: rgba(%1, 0.15); border-left: 3px solid rgb(%1);"
                                            " padding: 4px 8px; }")
                                 .arg(kBlue));
    otherWays->setVisible(m_recipe && !m_recipe->otherWays.isEmpty());

    auto* headerText = new QVBoxLayout;
    headerText->addWidget(title);
    headerText->addLayout(badges);
    headerText->addWidget(otherWays);
    headerText->addWidget(about);
    headerText->addWidget(m_headsetState);
    headerText->addStretch();
    auto* header = new QHBoxLayout;
    header->addWidget(m_cover, 0, Qt::AlignTop);
    header->addLayout(headerText, 1);

    // Files: one row per input, with a Choose button each, and below the last one Check Files Again
    // and Custom Files Folder. The buttons fill their column, so they all have the same width.
    auto* filesLayout = new QVBoxLayout(m_filesBox);
    m_filesGrid->setColumnStretch(0, 2);
    m_filesGrid->setColumnStretch(1, 3);
    filesLayout->addLayout(m_filesGrid);
    m_checkFilesButton->setToolTip(tr("Look for the files again in Steam, GOG and your files folder"));
    connect(m_checkFilesButton, &QPushButton::clicked, this, &GameDialog::findInputs);
    m_openFolderButton->setToolTip(tr("Create this game's folder in your files folder, with a guide to what goes there, and open it"));
    connect(m_openFolderButton, &QPushButton::clicked, this, &GameDialog::openFilesFolder);
    m_filesBox->setVisible(m_recipe && !m_recipe->inputs.isEmpty());

    // Install: the button, then the progress, the user's turn and the log.
    m_installHint->setWordWrap(true);
    connect(m_installButton, &QPushButton::clicked, this, &GameDialog::install);
    connect(m_cancelButton, &QPushButton::clicked, this, &GameDialog::cancel);
    m_cancelButton->hide();
    m_detailsButton->setToolTip(tr("Show the port's installation guide and troubleshooting notes, and every command the install runs"));
    connect(m_detailsButton, &QPushButton::clicked, this, &GameDialog::showInstallDetails);
    auto* installRow = new QHBoxLayout;
    installRow->addWidget(m_installButton);
    installRow->addWidget(m_cancelButton);
    installRow->addWidget(m_detailsButton);
    installRow->addWidget(m_installHint, 1);

    auto* progressLayout = new QVBoxLayout(m_progressArea);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    m_step->setWordWrap(true);
    progressLayout->addWidget(m_step);
    progressLayout->addWidget(m_progress);
    m_progressArea->hide();

    // The result of the last install, in place of the progress once it ends.
    m_resultArea->setObjectName(QStringLiteral("installResult"));
    m_resultArea->setAttribute(Qt::WA_StyledBackground);
    auto* resultLayout = new QHBoxLayout(m_resultArea);
    resultLayout->setContentsMargins(10, 8, 10, 8);
    QFont iconFont = m_resultIcon->font();
    iconFont.setPointSizeF(iconFont.pointSizeF() * 1.8);
    iconFont.setBold(true);
    m_resultIcon->setFont(iconFont);
    QFont resultFont = m_resultText->font();
    resultFont.setPointSizeF(resultFont.pointSizeF() * 1.25);
    resultFont.setBold(true);
    m_resultText->setFont(resultFont);
    m_resultText->setWordWrap(true);
    m_resultText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // What the recipe's last tell steps ask the user to do in the headset after the install.
    m_resultNext->setWordWrap(true);
    m_resultNext->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_resultNext->hide();
    auto* resultTexts = new QVBoxLayout;
    resultTexts->addWidget(m_resultText);
    resultTexts->addWidget(m_resultNext);
    resultLayout->addWidget(m_resultIcon, 0, Qt::AlignTop);
    resultLayout->addLayout(resultTexts, 1);
    m_resultArea->hide();

    // The user's turn in the middle of an install: an amber banner, like the result's, that the
    // install waits on.
    m_tellArea->setObjectName(QStringLiteral("tellArea"));
    m_tellArea->setAttribute(Qt::WA_StyledBackground);
    m_tellArea->setStyleSheet(QStringLiteral("#tellArea { background: rgba(210, 153, 34, 0.15);"
                                             " border: 1px solid rgb(210, 153, 34); border-radius: 4px; }"));
    auto* tellLayout = new QHBoxLayout(m_tellArea);
    tellLayout->setContentsMargins(10, 8, 10, 8);
    auto* tellHeading = new QLabel(tr("The install is waiting for you"), this);
    tellHeading->setFont(resultFont);
    m_tellText->setWordWrap(true);
    m_tellText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* tellTexts = new QVBoxLayout;
    tellTexts->addWidget(tellHeading);
    tellTexts->addWidget(m_tellText);
    connect(m_continueButton, &QPushButton::clicked, this, [this] {
        m_tellArea->hide();
        m_progress->setRange(0, 0);
        log(tr("Continued."));
        if (m_runner)
            m_runner->continueAfterTell();
    });
    tellLayout->addLayout(tellTexts, 1);
    tellLayout->addWidget(m_continueButton, 0, Qt::AlignVCenter);
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
    layout->addWidget(baseGame);
    layout->addLayout(installRow);
    layout->addWidget(m_progressArea);
    layout->addWidget(m_resultArea);
    layout->addWidget(m_tellArea);
    layout->addWidget(m_log, 1);
    layout->addWidget(buttons);

    connect(m_headset, &HeadsetConnection::keepAwakeChanged, this, &GameDialog::updateKeepAwake);
    connect(m_headset, &HeadsetConnection::statusChanged, this, [this] {
        showHeadsetState();
        updateButtons();
    });
    findInputs();
    showHeadsetState();

    // Size the dialog for its wrapped labels at this width, after the inputs fill the Files box.
    // Sized before that, the dialog grows with the box to a height that ignores wrapped labels, and
    // the header's lines overlap. sizeHint() wraps at a narrower width and is too tall on Windows,
    // and too short with macOS's larger fonts.
    constexpr int kWidth = 720;
    resize(kWidth, heightForWidth(kWidth));
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
        QWidget* widget = item->widget();
        if (widget != m_checkFilesButton && widget != m_openFolderButton)
            delete widget;
        delete item;
    }

    // What each input needs, in the words of the files folder's guide.
    const gamestores::FilesFolder files(filesRoot());
    QJsonArray inputsJson;
    for (const ResolvedInput& resolved : std::as_const(m_inputs))
        inputsJson.append(resolved.input.json);
    // The guide's web addresses, such as a tool's download page, are links.
    const auto guideLabel = [this](const QString& text) {
        auto* label = new QLabel(linkified(text), m_filesBox);
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);
        label->setOpenExternalLinks(true);
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        label->setForegroundRole(QPalette::PlaceholderText);
        label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        return label;
    };
    const QString shared = files.sharedFilesNote(inputsJson);

    for (int index = 0; index < m_inputs.size(); ++index) {
        const int row = index * 2;
        const ResolvedInput& resolved = m_inputs.at(index);
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
        auto* status = new QLabel(state, m_filesBox);
        status->setTextFormat(Qt::PlainText);
        status->setWordWrap(true);
        status->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (resolved.state != ResolvedInput::State::Found && input.required)
            status->setStyleSheet(QStringLiteral("color: palette(link)"));
        if (!resolved.candidates.isEmpty())
            status->setToolTip(resolved.candidates.join(u'\n'));

        auto* choose = new QPushButton(tr("Choose..."), m_filesBox);
        connect(choose, &QPushButton::clicked, this, [this, index] { chooseInput(index); });
        choose->setEnabled(!m_running);

        m_filesGrid->addWidget(label, row, 0);
        m_filesGrid->addWidget(status, row, 1);
        m_filesGrid->addWidget(choose, row, 2, Qt::AlignTop); // Vertical only: it fills the column.
        const QStringList guide = files.inputGuide(inputsJson, index, m_game.slug);
        // The last guide also takes the row of Open Files Folder when no note needs it, so that a
        // long guide is not cut off at the height of Check Files Again's row.
        const int rows = index == m_inputs.size() - 1 && shared.isEmpty() ? 2 : 1;
        if (!guide.isEmpty())
            m_filesGrid->addWidget(guideLabel(guide.join(u'\n')), row + 1, 0, rows, 2);
    }

    // The last input's guide row has the third column free: the buttons start there.
    const int end = qMax(1, int(m_inputs.size()) * 2);
    m_filesGrid->addWidget(m_checkFilesButton, end - 1, 2, Qt::AlignTop);
    m_filesGrid->addWidget(m_openFolderButton, end, 2, Qt::AlignTop);
    if (!shared.isEmpty())
        m_filesGrid->addWidget(guideLabel(shared), end, 0, 1, 2);
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
    m_updateAvailable = false;
    m_updateBadge->hide();
    showInstalledBadge(Installed::Unknown);
    updateButtons();
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
            showInstalledBadge(Installed::NotInstalled);
            return;
        }
        const qadb::PackageInfo installed = reply->info();
        const QString version = installed.versionName.isEmpty() ? QString::number(installed.versionCode)
                                                                : installed.versionName;
        // Whether QGPUI installed it: the headset's install records say.
        auto* records = client->shell(serial, QStringList{QStringLiteral("cat"), QLatin1StringView(installer::kHeadsetRecordsPath)});
        connect(records, &qadb::Reply::finished, this, [this, records, name, package, installed, version] {
            records->deleteLater();
            std::optional<installer::InstallRecord> record;
            if (records->isSuccess()) {
                if (const auto list = installer::parseHeadsetRecords(records->output())) {
                    const auto it = std::find_if(list->cbegin(), list->cend(), [&](const installer::InstallRecord& r) {
                        return r.packageName == package;
                    });
                    if (it != list->cend())
                        record = *it;
                }
            }
            const QString state = record ? tr("Installed on %1 by qgpui: version %2.").arg(name, version)
                                         : tr("Installed on %1 outside qgpui: version %2.").arg(name, version);
            m_headsetState->setText(state);
            showInstalledBadge(record ? Installed::ByQgpui : Installed::OutsideQgpui);
            showUpdate(state, installed, record);
        });
    });
}

void GameDialog::showInstalledBadge(Installed installed)
{
    switch (installed) {
    case Installed::Unknown:
        m_installedBadge->hide();
        return;
    case Installed::NotInstalled:
        setBadge(m_installedBadge, tr("Not installed"), tr("The game is not on the current headset"), kGrey);
        break;
    case Installed::ByQgpui:
        setBadge(m_installedBadge, tr("Installed"), tr("QGPUI installed the game on the current headset"), kGreen);
        break;
    case Installed::OutsideQgpui:
        setBadge(m_installedBadge, tr("Installed outside QGPUI"),
                 tr("The game is on the current headset, but QGPUI did not install it"), kGreen);
        break;
    }
    m_installedBadge->show();
}

void GameDialog::showUpdate(const QString& state, const qadb::PackageInfo& installed,
                            const std::optional<installer::InstallRecord>& record)
{
    const auto source = m_recipe ? updateSource(*m_recipe) : std::nullopt;
    if (!source)
        return;
    m_releases->latest(*source, this, [this, state, installed, record](const auto& asset, const QString& errorMessage) {
        if (m_running)
            return;
        if (!asset) {
            m_headsetState->setText(tr("%1 The newest release is not known: %2").arg(state, errorMessage));
            return;
        }
        switch (installer::updateStatus(installed, record, asset->tag)) {
        case installer::UpdateStatus::UpdateAvailable:
            m_updateAvailable = true;
            m_updateBadge->show();
            m_headsetState->setText(tr("%1 An update is available: release %2.").arg(state, asset->tag));
            break;
        case installer::UpdateStatus::UpToDate:
            m_headsetState->setText(tr("%1 It is up to date with release %2.").arg(state, asset->tag));
            break;
        case installer::UpdateStatus::Unknown:
            m_headsetState->setText(tr("%1 The newest release is %2; QGPUI cannot tell whether it is newer.")
                                        .arg(state, asset->tag));
            break;
        case installer::UpdateStatus::NotInstalled:
            break;
        }
        updateButtons();
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
        hint = tr("Install needs a connected headset. %1")
                   .arg(m_headset->statusAdvice().isEmpty() ? m_headset->statusText() : m_headset->statusAdvice());
    else if (!InputResolver::ready(m_inputs))
        hint = tr("Some files are missing. Choose them, or put them in your files folder and look again.");

    m_installButton->setText(m_updateAvailable ? tr("&Update") : tr("&Install"));
    m_installButton->setEnabled(!m_running && hint.isEmpty());
    m_installButton->setVisible(!m_running);
    m_cancelButton->setVisible(m_running);
    m_detailsButton->setEnabled(m_recipe.has_value() || !installationGuide().isEmpty()
                                || !troubleshootingNotes().isEmpty());
    m_installHint->setText(m_running ? QString() : hint);
}

QString GameDialog::troubleshootingNotes() const
{
    return m_game.port.value(QLatin1StringView("troubleshooting_notes")).toString().trimmed();
}

QString GameDialog::installationGuide() const
{
    return m_game.port.value(QLatin1StringView("installation_guide")).toString().trimmed();
}

void GameDialog::showInstallDetails()
{
    const QString guide = installationGuide();
    const QString notes = troubleshootingNotes();
    if (!m_recipe && guide.isEmpty() && notes.isEmpty())
        return;

    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Installing %1").arg(m_game.title));
    auto* layout = new QVBoxLayout(dialog);
    auto* tabs = new QTabWidget(dialog);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    // QuestPorts' guide, then its troubleshooting notes, in the order of the port's page.
    if (!guide.isEmpty() || !notes.isEmpty()) {
        auto* page = new QWidget(tabs);
        auto* pageLayout = new QVBoxLayout(page);
        if (!guide.isEmpty())
            pageLayout->addWidget(makeGuideView(guide, page), 1);
        if (!notes.isEmpty()) {
            auto* box = new QGroupBox(tr("Troubleshooting && Tips"), page);
            auto* label = new QLabel(linkified(notes), box);
            label->setTextFormat(Qt::RichText);
            label->setWordWrap(true);
            label->setOpenExternalLinks(true);
            label->setTextInteractionFlags(Qt::TextBrowserInteraction);
            auto* boxLayout = new QVBoxLayout(box);
            boxLayout->addWidget(label);
            pageLayout->addWidget(box);
        }
        if (guide.isEmpty())
            pageLayout->addStretch();
        tabs->addTab(page, tr("Installation &Guide"));
    }

    QPushButton* copy = nullptr;
    if (m_recipe) {
        installer::InstallPlan plan = installer::planInstall(*m_recipe, {}, InputResolver::sources(m_inputs));
        plan.steps.append({tr("Record the install"),
                           {tr("Write the release and version to /sdcard/qgpui/installs.json on the headset.")}});
        const QString text = plan.toText();
        auto* page = new QWidget(tabs);
        auto* pageLayout = new QVBoxLayout(page);
        auto* intro = new QLabel(tr("The install runs these steps in order, with the files as they are found now. "
                                    "Lines that start with adb are the commands as the adb tool would take them; "
                                    "QGPUI sends them to the adb server itself."),
                                 page);
        intro->setWordWrap(true);
        auto* view = new QPlainTextEdit(text, page);
        view->setReadOnly(true);
        view->setLineWrapMode(QPlainTextEdit::NoWrap);
        view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        pageLayout->addWidget(intro);
        pageLayout->addWidget(view, 1);
        const int index = tabs->addTab(page, tr("&QGPUI's Steps"));
        copy = buttons->addButton(tr("&Copy Steps"), QDialogButtonBox::ActionRole);
        connect(copy, &QPushButton::clicked, dialog, [text] { QApplication::clipboard()->setText(text); });
        connect(tabs, &QTabWidget::currentChanged, copy, [copy, index](int current) { copy->setVisible(current == index); });
        copy->setVisible(tabs->currentIndex() == index);
    }

    layout->addWidget(tabs, 1);
    layout->addWidget(buttons);
    dialog->resize(900, 700);
    dialog->show();
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
    m_afterInstall.clear();
    m_log->clear();
    m_log->show();
    m_progressArea->show();
    m_resultArea->hide();
    m_tellArea->hide();
    showInputs(); // Disables the Choose buttons.
    updateButtons();
    log(tr("Installing %1 on %2.").arg(m_game.title, m_serial));
    if (SettingsDialog::keepAwakeWhileInstalling() && m_headset->keepsAwake())
        log(tr("The headset's screen is already kept on by the toolbar's keep-awake button."));
    updateKeepAwake();
    fetchApk(0);
}

void GameDialog::updateKeepAwake()
{
    // The toolbar's keep-awake button, when on, holds the headset on its own.
    const bool wanted = m_running && SettingsDialog::keepAwakeWhileInstalling() && !m_headset->keepsAwake();
    if (m_keepAwake && !wanted) {
        // Leave the headset awake when the button took over.
        if (m_headset->keepsAwake())
            m_keepAwake->abandon();
        delete m_keepAwake;
    }
    if (wanted && !m_keepAwake) {
        m_keepAwake = new installer::KeepAwake(m_headset->client(), m_serial, this);
        connect(m_keepAwake, &installer::KeepAwake::failed, this, [this](const QString& errorMessage) {
            log(tr("Could not keep the headset's screen on: %1").arg(errorMessage));
        });
        connect(m_keepAwake, &installer::KeepAwake::stateChanged, this, [this](installer::KeepAwake::State state) {
            if (state == installer::KeepAwake::State::Holding)
                log(tr("Keeping the headset's screen on until the install ends."));
            else if (state == installer::KeepAwake::State::Deferring)
                log(tr("Another app on the headset already keeps its screen on; QGPUI leaves it alone."));
        });
        m_keepAwake->start();
    }
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
            finishInstall(Outcome::Failure, tr("No APK was chosen."));
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
                    finishInstall(Outcome::Failure, errorMessage);
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
        finishInstall(Outcome::Failure, tr("The headset is no longer connected."));
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
        m_step->setText(m_step->text() + tr(" (waiting for you)"));
        // A still bar: nothing runs until the user continues.
        m_progress->setRange(0, 1);
        m_progress->setValue(0);
        m_tellText->setText(text);
        m_tellArea->show();
        m_continueButton->setFocus();
        log(tr("Waiting for you. In the headset: %1").arg(text));
        QApplication::alert(this);
    });
    connect(m_runner, &installer::Runner::afterInstall, this, [this](const QString& text) {
        m_afterInstall.append(text);
        log(tr("After the install, in the headset: %1").arg(text));
    });
    connect(m_runner, &installer::Runner::finished, this, [this](bool success, const QString& errorMessage) {
        m_runner->deleteLater();
        m_tellArea->hide();
        if (success)
            recordInstall();
        else
            finishInstall(Outcome::Failure, errorMessage);
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
                finishInstall(Outcome::Warning, tr("Installed, but the install could not be recorded: %1")
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
                            finishInstall(Outcome::Success, tr("Installed version %1.").arg(version));
                        else
                            finishInstall(Outcome::Warning, tr("Installed version %1, but the install could not be recorded: %2")
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
    finishInstall(Outcome::Failure, tr("Stopped."));
}

void GameDialog::finishInstall(Outcome outcome, const QString& message)
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
    updateKeepAwake(); // Hands the screen back to the proximity sensor.
    m_tellArea->hide();
    m_progressArea->hide();
    const QString text = outcome == Outcome::Failure ? tr("The install failed: %1").arg(message)
                                                     : tr("Install complete. %1").arg(message);
    showResult(outcome, text);
    log(text);
    // The game is on the headset; say what the user does there now.
    const bool installed = outcome != Outcome::Failure && !m_afterInstall.isEmpty();
    m_resultNext->setText(installed ? tr("Now, in the headset: %1").arg(m_afterInstall.join(QLatin1Char(' '))) : QString());
    m_resultNext->setVisible(installed);
    showInputs();
    updateButtons();
    showHeadsetState();
    QApplication::alert(this); // Flashes the taskbar if the user is in another window.
    emit installFinished();
}

void GameDialog::showResult(Outcome outcome, const QString& text)
{
    // Translucent tints read on both light and dark palettes.
    QString colour;
    QString icon;
    switch (outcome) {
    case Outcome::Success:
        colour = QStringLiteral("46, 160, 67");
        icon = QStringLiteral("✔");
        break;
    case Outcome::Warning:
        colour = QStringLiteral("210, 153, 34");
        icon = QStringLiteral("!");
        break;
    case Outcome::Failure:
        colour = QStringLiteral("218, 54, 51");
        icon = QStringLiteral("✖");
        break;
    }
    m_resultArea->setStyleSheet(QStringLiteral("#installResult { background: rgba(%1, 0.15); border: 1px solid rgb(%1);"
                                               " border-radius: 4px; }")
                                    .arg(colour));
    m_resultIcon->setStyleSheet(QStringLiteral("color: rgb(%1);").arg(colour));
    m_resultIcon->setText(icon);
    m_resultText->setText(text);
    m_resultArea->show();
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
