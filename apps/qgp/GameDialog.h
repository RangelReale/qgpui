#pragma once

#include "InputResolver.h"
#include "GameFlags.h"

#include "installer/Recipe.h"
#include "questports/Catalog.h"

#include <QDialog>
#include <QPointer>
#include <QStringList>

#include <optional>

class CoverCache;
class HeadsetConnection;
class QGridLayout;
class QGroupBox;
class QLabel;
class QNetworkAccessManager;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QWidget;

namespace installer {
class ApkFetcher;
struct InstallRecord;
class KeepAwake;
class ReleaseLookup;
class Runner;
} // namespace installer
namespace qadb {
struct PackageInfo;
}

// One game: its cover and recipe, what is on the current headset, the files the recipe needs and
// where they were found, and the Install button with the install's progress and log. Every action
// on a game starts here.
class GameDialog : public QDialog
{
    Q_OBJECT

public:
    // releases finds the newest release of the game's APK; the main window shares it. sourceLabel
    // is the catalog's name for the game's origin platform, such as "PC".
    GameDialog(const questports::Game& game, HeadsetConnection* headset, CoverCache* covers,
               installer::ReleaseLookup* releases, const QString& sourceLabel, QWidget* parent = nullptr);

    QString slug() const { return m_game.slug; }

    // The user's files folder: the setting, or <Documents>/qgp/files.
    static QString filesRoot();
    static void setFilesRoot(const QString& folder);

signals:
    // An install ended, whether or not it succeeded: what is on the headset may have changed.
    void installFinished();

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    void findInputs();
    void showInputs();
    void chooseInput(int index);
    void openFilesFolder();
    void openStorePage(const StorePage& page);
    void showHeadsetState();
    // Shows the installed badge, or hides it when the state is not known (no headset, no package).
    enum class Installed { Unknown, NotInstalled, ByQgp, OutsideQgp };
    void showInstalledBadge(Installed installed);
    // Adds to state whether the newest release is newer than the installed package, and turns
    // Install into Update when it is.
    void showUpdate(const QString& state, const qadb::PackageInfo& installed,
                    const std::optional<installer::InstallRecord>& record);
    void updateButtons();

    void install();
    // Shows the commands the install will run, in a dialog of their own.
    void showInstallDetails();
    void fetchApk(int index);
    void runSteps();
    void recordInstall();
    // Starts or stops m_keepAwake: on while an install runs, if the setting is on and the toolbar's
    // keep-awake button is off.
    void updateKeepAwake();
    void cancel();
    // How an install ended. Warning: the game is installed, but something after it failed.
    enum class Outcome { Success, Warning, Failure };
    void finishInstall(Outcome outcome, const QString& message);
    void showResult(Outcome outcome, const QString& text);
    void log(const QString& text);
    // Asks whether to stop a running install. True when nothing runs any more.
    bool confirmStop();

    questports::Game m_game;
    HeadsetConnection* m_headset;
    CoverCache* m_covers;
    installer::ReleaseLookup* m_releases;
    std::optional<installer::Recipe> m_recipe;
    QString m_recipeError;
    QList<ResolvedInput> m_inputs;

    QLabel* m_cover;
    QLabel* m_headsetState;
    QLabel* m_installedBadge;
    QLabel* m_updateBadge;
    QGroupBox* m_filesBox;
    QGridLayout* m_filesGrid;
    QPushButton* m_installButton;
    QPushButton* m_cancelButton;
    QPushButton* m_detailsButton;
    QLabel* m_installHint;
    QWidget* m_progressArea;
    QLabel* m_step;
    QProgressBar* m_progress;
    QWidget* m_resultArea;
    QLabel* m_resultIcon;
    QLabel* m_resultText;
    QWidget* m_tellArea;
    QLabel* m_tellText;
    QPlainTextEdit* m_log;

    bool m_running = false;
    bool m_updateAvailable = false;
    QString m_serial; // The headset the install runs on.
    QStringList m_apkFiles;
    QString m_releaseTag;
    QString m_asset;
    QPointer<installer::ApkFetcher> m_fetcher;
    QPointer<installer::Runner> m_runner;
    QPointer<installer::KeepAwake> m_keepAwake; // While an install runs, if the setting is on.
    QNetworkAccessManager* m_network = nullptr;
};
