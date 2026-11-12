#pragma once

#include "InputResolver.h"
#include "StoreAvailability.h"

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
class Runner;
} // namespace installer

// One game: its cover and recipe, what is on the current headset, the files the recipe needs and
// where they were found, and the Install button with the install's progress and log. Every action
// on a game starts here.
class GameDialog : public QDialog
{
    Q_OBJECT

public:
    GameDialog(const questports::Game& game, HeadsetConnection* headset, CoverCache* covers,
               QWidget* parent = nullptr);

    QString slug() const { return m_game.slug; }

    // The user's files folder: the setting, or <Documents>/qgp/files.
    static QString filesRoot();
    static void setFilesRoot(const QString& folder);

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
    void updateButtons();

    void install();
    void fetchApk(int index);
    void runSteps();
    void recordInstall();
    void cancel();
    void finishInstall(bool success, const QString& message);
    void log(const QString& text);
    // Asks whether to stop a running install. True when nothing runs any more.
    bool confirmStop();

    questports::Game m_game;
    HeadsetConnection* m_headset;
    CoverCache* m_covers;
    std::optional<installer::Recipe> m_recipe;
    QString m_recipeError;
    QList<ResolvedInput> m_inputs;

    QLabel* m_cover;
    QLabel* m_headsetState;
    QGroupBox* m_filesBox;
    QGridLayout* m_filesGrid;
    QPushButton* m_installButton;
    QPushButton* m_cancelButton;
    QLabel* m_installHint;
    QWidget* m_progressArea;
    QLabel* m_step;
    QProgressBar* m_progress;
    QWidget* m_tellArea;
    QLabel* m_tellText;
    QPlainTextEdit* m_log;

    bool m_running = false;
    QString m_serial; // The headset the install runs on.
    QStringList m_apkFiles;
    QString m_releaseTag;
    QString m_asset;
    QPointer<installer::ApkFetcher> m_fetcher;
    QPointer<installer::Runner> m_runner;
    QNetworkAccessManager* m_network = nullptr;
};
