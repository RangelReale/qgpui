#pragma once

#include "installer/PushPlan.h"
#include "installer/Recipe.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <chrono>
#include <functional>
#include <optional>

namespace qadb {
class Client;
class Reply;
} // namespace qadb

namespace installer {

// Runs a recipe's steps on one device, in order. The APKs must already be on the PC (see
// ApkFetcher) and the inputs resolved to files and folders; the runner only talks to the device.
//
// Before the first step, when the recipe sets freeSpaceMB, the headset's free space is checked.
//
// Steps:
// - install-apk: adb install -r (and -g) of every APK. A headset that already has a newer build
//   (INSTALL_FAILED_VERSION_DOWNGRADE) keeps it and the run goes on. An app signed with another
//   key (INSTALL_FAILED_UPDATE_INCOMPATIBLE) stops the run with a message on what to do.
// - grant: appops set <package> <op> allow, or pm grant. A failure is logged, not fatal: the
//   recipes follow it with a tell step for the user.
// - launch: starts the app's launcher activity, waits, then force-stops it unless told not to.
// - push: copies file by file. Below /sdcard/Android/data/ and /sdcard/Android/obb/ it first
//   creates the missing folders with mkdir and opens them to the app (chmod 2777), because adb
//   cannot create nested folders there and the folders it creates are closed to the app.
// - verify: checks that every listed file exists.
// - shell: runs the command; a non-zero exit code is fatal. Arguments are quoted, so there is no
//   wildcard expansion.
// - tell: emits waitingForUser() and pauses until continueAfterTell(). A tell step followed only by
//   tell steps does not pause: it emits afterInstall() and the run goes on, because nothing is left
//   that depends on the user.
class Runner : public QObject
{
    Q_OBJECT

public:
    Runner(qadb::Client* client, const QString& serial, const Recipe& recipe, const QStringList& apkFiles,
           const QHash<QString, InputSource>& inputs, QObject* parent = nullptr);

    // Replaces every launch step's wait, for tests.
    void setLaunchWaitOverride(std::chrono::milliseconds wait) { m_launchWaitOverride = wait; }

    void start();
    void continueAfterTell();
    void cancel();

    bool isRunning() const { return m_running; }

signals:
    void stepStarted(int index, int count, const QString& description);
    // Bytes of the current step's transfers.
    void progress(qint64 done, qint64 total);
    void message(const QString& text);
    void waitingForUser(const QString& text);
    // What the user does in the headset once the run ends; the run does not wait for it.
    void afterInstall(const QString& text);
    void finished(bool success, const QString& errorMessage);

private:
    void nextStep();
    void finish(bool success, const QString& errorMessage = {});
    // Runs a shell command; calls done(exitCode, output) or fails the run on a request error.
    void runShell(const QStringList& arguments, std::function<void(int, const QString&)> done);
    void watch(qadb::Reply* reply, std::function<void()> onSuccess);

    void runInstall(const Step& step, int apkIndex);
    void runGrant(const Step& step);
    void runLaunch(const Step& step);
    void runPush(const Step& step);
    void pushNext();
    void runVerify(const Step& step, int fileIndex);
    void runShellStep(const Step& step);

    qadb::Client* m_client;
    QString m_serial;
    Recipe m_recipe;
    QStringList m_apkFiles;
    QHash<QString, InputSource> m_inputs;
    std::optional<std::chrono::milliseconds> m_launchWaitOverride;

    int m_step = -1;
    bool m_running = false;
    bool m_waitingForUser = false;
    QPointer<qadb::Reply> m_current;
    QList<PushFile> m_pushQueue;
    qint64 m_pushTotal = 0;
    qint64 m_pushDone = 0;
};

} // namespace installer
