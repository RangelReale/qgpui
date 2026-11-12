#include "installer/Runner.h"

#include <qadb/Client.h>
#include <qadb/Protocol.h>
#include <qadb/Reply.h>

#include <QFileInfo>
#include <QTimer>

#include <memory>

namespace installer {

namespace {

// adb's host request length is four hex digits; stay well below it.
constexpr qsizetype kMaxShellCommand = 8000;

} // namespace

Runner::Runner(qadb::Client* client, const QString& serial, const Recipe& recipe, const QStringList& apkFiles,
               const QHash<QString, InputSource>& inputs, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_serial(serial)
    , m_recipe(recipe)
    , m_apkFiles(apkFiles)
    , m_inputs(inputs)
{}

void Runner::start()
{
    if (m_running)
        return;
    m_running = true;
    m_step = -1;
    if (m_recipe.freeSpaceMB <= 0) {
        nextStep();
        return;
    }
    // Check the headset's free space before changing anything.
    auto* reply = m_client->storage(m_serial);
    watch(reply, [this, reply] {
        const qint64 freeMB = reply->info().freeBytes / (1024 * 1024);
        if (freeMB < m_recipe.freeSpaceMB) {
            finish(false, QStringLiteral("The headset has %1 MB free; this game needs about %2 MB.")
                              .arg(freeMB).arg(m_recipe.freeSpaceMB));
            return;
        }
        nextStep();
    });
}

void Runner::continueAfterTell()
{
    if (!m_running || !m_waitingForUser)
        return;
    m_waitingForUser = false;
    nextStep();
}

void Runner::cancel()
{
    if (!m_running)
        return;
    if (m_current)
        m_current->abort();
    finish(false, QStringLiteral("Cancelled."));
}

void Runner::finish(bool success, const QString& errorMessage)
{
    if (!m_running)
        return;
    m_running = false;
    m_waitingForUser = false;
    emit finished(success, errorMessage);
}

void Runner::watch(qadb::Reply* reply, std::function<void()> onSuccess)
{
    m_current = reply;
    connect(reply, &qadb::Reply::finished, this, [this, reply, onSuccess = std::move(onSuccess)] {
        reply->deleteLater();
        if (!m_running)
            return;
        if (!reply->isSuccess()) {
            finish(false, reply->errorString());
            return;
        }
        onSuccess();
    });
}

void Runner::runShell(const QStringList& arguments, std::function<void(int, const QString&)> done)
{
    auto* reply = m_client->shell(m_serial, arguments);
    watch(reply, [reply, done = std::move(done)] {
        QString output = reply->outputText().trimmed();
        const QString err = QString::fromUtf8(reply->standardError()).trimmed();
        if (!err.isEmpty())
            output += (output.isEmpty() ? QString() : QStringLiteral("\n")) + err;
        done(reply->hasExitCode() ? reply->exitCode() : 0, output);
    });
}

void Runner::nextStep()
{
    if (!m_running)
        return;
    ++m_step;
    if (m_step >= m_recipe.steps.size()) {
        finish(true);
        return;
    }
    const Step& step = m_recipe.steps.at(m_step);
    emit stepStarted(m_step, int(m_recipe.steps.size()), describeStep(step));
    switch (step.kind) {
    case Step::Kind::InstallApk:
        if (m_apkFiles.isEmpty()) {
            finish(false, QStringLiteral("No APK to install."));
            return;
        }
        runInstall(step, 0);
        return;
    case Step::Kind::Grant:
        runGrant(step);
        return;
    case Step::Kind::Launch:
        runLaunch(step);
        return;
    case Step::Kind::Push:
        runPush(step);
        return;
    case Step::Kind::Verify:
        runVerify(step, 0);
        return;
    case Step::Kind::Shell:
        runShellStep(step);
        return;
    case Step::Kind::Tell:
        m_waitingForUser = true;
        emit waitingForUser(step.text);
        return;
    }
}

void Runner::runInstall(const Step& step, int apkIndex)
{
    if (apkIndex >= m_apkFiles.size()) {
        nextStep();
        return;
    }
    qadb::InstallOptions options;
    options.replace = true;
    options.grantPermissions = step.grantAll;
    const QString apk = m_apkFiles.at(apkIndex);
    emit message(QStringLiteral("Installing %1").arg(QFileInfo(apk).fileName()));
    auto* reply = m_client->install(m_serial, apk, options);
    m_current = reply;
    connect(reply, &qadb::Reply::progress, this, &Runner::progress);
    connect(reply, &qadb::Reply::finished, this, [this, reply, step, apkIndex] {
        reply->deleteLater();
        if (!m_running)
            return;
        if (reply->isSuccess()) {
            runInstall(step, apkIndex + 1);
            return;
        }
        const QString error = reply->errorString() + u' ' + reply->outputText();
        if (error.contains(QLatin1StringView("INSTALL_FAILED_VERSION_DOWNGRADE"))) {
            // Ports that update themselves in the headset can be newer than the release.
            emit message(QStringLiteral("The headset already has a newer version of this app; it was kept."));
            runInstall(step, apkIndex + 1);
            return;
        }
        if (error.contains(QLatin1StringView("INSTALL_FAILED_UPDATE_INCOMPATIBLE"))) {
            finish(false, QStringLiteral("The installed app was signed with a different key, so it cannot be updated "
                                         "in place. Back up its saves, uninstall it (this deletes its data on the "
                                         "headset), then install again and restore the saves."));
            return;
        }
        finish(false, reply->errorString());
    });
}

void Runner::runGrant(const Step& step)
{
    const QStringList command = step.appop.isEmpty()
        ? QStringList{QStringLiteral("pm"), QStringLiteral("grant"), m_recipe.packageName, step.permission}
        : QStringList{QStringLiteral("appops"), QStringLiteral("set"), m_recipe.packageName, step.appop,
                      QStringLiteral("allow")};
    runShell(command, [this, command](int exitCode, const QString& output) {
        if (exitCode != 0)
            emit message(QStringLiteral("Could not run \"%1\": %2. The app may ask in the headset instead.")
                             .arg(command.join(u' '), output));
        nextStep();
    });
}

void Runner::runLaunch(const Step& step)
{
    const QStringList command{QStringLiteral("monkey"), QStringLiteral("-p"), m_recipe.packageName,
                              QStringLiteral("-c"), QStringLiteral("android.intent.category.LAUNCHER"),
                              QStringLiteral("1")};
    runShell(command, [this, step](int exitCode, const QString& output) {
        if (exitCode != 0) {
            finish(false, QStringLiteral("Could not start %1: %2").arg(m_recipe.packageName, output));
            return;
        }
        const auto wait = m_launchWaitOverride.value_or(std::chrono::seconds(step.waitSeconds));
        QTimer::singleShot(wait, this, [this, step] {
            if (!m_running)
                return;
            if (!step.stop) {
                nextStep();
                return;
            }
            runShell({QStringLiteral("am"), QStringLiteral("force-stop"), m_recipe.packageName},
                     [this](int, const QString&) { nextStep(); });
        });
    });
}

void Runner::runPush(const Step& step)
{
    const Input* input = m_recipe.input(step.input);
    const auto source = m_inputs.constFind(step.input);
    if (source == m_inputs.cend()) {
        if (input && !input->required) {
            emit message(QStringLiteral("Skipped: %1 was not given.").arg(input->label));
            nextStep();
        } else {
            finish(false, QStringLiteral("No files were given for \"%1\".").arg(input ? input->label : step.input));
        }
        return;
    }

    PushPlan plan;
    QString error;
    if (!planPush(step, *input, *source, plan, &error)) {
        finish(false, error);
        return;
    }
    for (const QString& unmatched : std::as_const(plan.unmatched))
        emit message(QStringLiteral("No file matched %1.").arg(unmatched));
    if (plan.files.isEmpty()) {
        finish(false, QStringLiteral("Nothing to copy for \"%1\".").arg(input->label));
        return;
    }

    m_pushQueue = plan.files;
    m_pushTotal = 0;
    m_pushDone = 0;
    for (const PushFile& file : std::as_const(m_pushQueue))
        m_pushTotal += QFileInfo(file.localPath).size();

    // Create the missing app-private folders and open them to the app, in batches that fit one
    // shell request. Folders are in parent-first order.
    QStringList batches;
    QString batch;
    for (const QString& folder : appPrivateFolders(plan.files)) {
        const QString quoted = qadb::protocol::shellQuote(folder);
        if (!batch.isEmpty() && batch.size() + quoted.size() > kMaxShellCommand) {
            batches.append(batch);
            batch.clear();
        }
        batch += u' ' + quoted;
    }
    if (!batch.isEmpty())
        batches.append(batch);

    auto runBatch = std::make_shared<std::function<void(int)>>();
    *runBatch = [this, batches, runBatch](int index) {
        if (index >= batches.size()) {
            pushNext();
            return;
        }
        const QString script = QStringLiteral("for d in%1; do [ -d \"$d\" ] || { mkdir \"$d\" && chmod 2777 \"$d\"; } "
                                              "|| exit 1; done")
                                   .arg(batches.at(index));
        auto* reply = m_client->shell(m_serial, script);
        watch(reply, [this, reply, runBatch, index] {
            if (reply->hasExitCode() && reply->exitCode() != 0) {
                finish(false, QStringLiteral("Could not create the app's folders: %1")
                                  .arg(QString::fromUtf8(reply->output() + reply->standardError()).trimmed()));
                return;
            }
            (*runBatch)(index + 1);
        });
    };
    (*runBatch)(0);
}

void Runner::pushNext()
{
    if (!m_running)
        return;
    if (m_pushQueue.isEmpty()) {
        nextStep();
        return;
    }
    const PushFile file = m_pushQueue.takeFirst();
    const qint64 size = QFileInfo(file.localPath).size();
    // In app-private folders the files belong to the shell user, so the app needs them open.
    const quint32 mode = isAppPrivatePath(file.remotePath) ? 0666 : 0644;
    auto* reply = m_client->push(m_serial, file.localPath, file.remotePath, mode);
    const qint64 before = m_pushDone;
    connect(reply, &qadb::Reply::progress, this, [this, before](qint64 done, qint64) {
        emit progress(before + done, m_pushTotal);
    });
    watch(reply, [this, size] {
        m_pushDone += size;
        emit progress(m_pushDone, m_pushTotal);
        pushNext();
    });
}

void Runner::runVerify(const Step& step, int fileIndex)
{
    if (fileIndex >= step.files.size()) {
        nextStep();
        return;
    }
    const QString path = step.files.at(fileIndex);
    auto* reply = m_client->stat(m_serial, path);
    watch(reply, [this, reply, step, fileIndex, path] {
        if (!reply->info().exists()) {
            finish(false, QStringLiteral("%1 is missing on the headset.").arg(path));
            return;
        }
        runVerify(step, fileIndex + 1);
    });
}

void Runner::runShellStep(const Step& step)
{
    runShell(step.args, [this, step](int exitCode, const QString& output) {
        if (exitCode != 0) {
            finish(false, QStringLiteral("\"%1\" failed (exit code %2): %3").arg(step.args.join(u' ')).arg(exitCode).arg(output));
            return;
        }
        nextStep();
    });
}

} // namespace installer
