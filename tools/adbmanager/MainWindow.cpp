#include "MainWindow.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QLocale>
#include <QMetaEnum>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace qadb;

namespace {

QPushButton* button(const QString& text, QWidget* parent, std::function<void()> onClick)
{
    auto* b = new QPushButton(text, parent);
    QObject::connect(b, &QPushButton::clicked, parent, std::move(onClick));
    return b;
}

QString errorName(Reply::Error error)
{
    return QString::fromLatin1(QMetaEnum::fromType<Reply::Error>().valueToKey(int(error)));
}

QString humanSize(quint64 bytes)
{
    return QLocale().formattedDataSize(qint64(bytes));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_client(new Client(this))
    , m_tracker(new DeviceTracker(this))
{
    setWindowTitle(QStringLiteral("adbmanager (qadb debug tool)"));
    setAcceptDrops(true);

    auto* controls = new QWidget;
    auto* controlsLayout = new QVBoxLayout(controls);
    controlsLayout->addWidget(buildServerBox());
    controlsLayout->addWidget(buildNetworkBox());
    controlsLayout->addWidget(buildDeviceBox());
    controlsLayout->addWidget(buildFilesBox());
    controlsLayout->addWidget(buildPackagesBox());
    controlsLayout->addStretch();
    auto* scroll = new QScrollArea;
    scroll->setWidget(controls);
    scroll->setWidgetResizable(true);
    scroll->setMinimumWidth(430);

    m_devices = new QTreeWidget;
    m_devices->setHeaderLabels({tr("Serial"), tr("State"), tr("Model"), tr("Product"),
                                tr("Transport"), tr("USB")});
    m_devices->setRootIsDecorated(false);
    m_devices->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_log->setMaximumBlockCount(20000);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 1000);
    m_progress->setValue(0);
    m_abort = button(tr("Abort"), this, [this] {
        if (m_transfer)
            m_transfer->abort();
    });
    m_abort->setEnabled(false);
    auto* progressRow = new QHBoxLayout;
    progressRow->addWidget(m_progress);
    progressRow->addWidget(m_abort);
    progressRow->addWidget(button(tr("Clear log"), this, [this] { m_log->clear(); }));

    auto* right = new QWidget;
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    auto* rightSplit = new QSplitter(Qt::Vertical);
    rightSplit->addWidget(m_devices);
    auto* logPane = new QWidget;
    auto* logLayout = new QVBoxLayout(logPane);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->addWidget(buildShellBox());
    logLayout->addWidget(m_log);
    logLayout->addLayout(progressRow);
    rightSplit->addWidget(logPane);
    rightSplit->setStretchFactor(1, 3);
    rightLayout->addWidget(rightSplit);

    auto* split = new QSplitter;
    split->addWidget(scroll);
    split->addWidget(right);
    split->setStretchFactor(1, 1);
    setCentralWidget(split);

    connect(m_tracker, &DeviceTracker::connectedChanged, this, [this](bool connected) {
        m_serverStatus->setText(connected ? tr("Connected to %1:%2")
                                                .arg(m_tracker->server().host)
                                                .arg(m_tracker->server().port)
                                          : tr("Not connected (retrying)"));
        log(connected ? tr("Tracker: connected to the adb server")
                      : tr("Tracker: lost the adb server"));
    });
    connect(m_tracker, &DeviceTracker::errorOccurred, this,
            [this](const QString& message) { log(tr("Tracker error: %1").arg(message)); });
    connect(m_tracker, &DeviceTracker::deviceAdded, this, [this](const DeviceInfo& d) {
        log(tr("Device added: %1 (%2)").arg(d.serial, d.stateText));
    });
    connect(m_tracker, &DeviceTracker::deviceRemoved, this, [this](const DeviceInfo& d) {
        m_client->clearFeatureCache(d.serial);
        log(tr("Device removed: %1").arg(d.serial));
    });
    connect(m_tracker, &DeviceTracker::deviceChanged, this, [this](const DeviceInfo& d) {
        log(tr("Device changed: %1 is now %2").arg(d.serial, d.stateText));
    });
    connect(m_tracker, &DeviceTracker::devicesChanged, this, &MainWindow::updateDeviceList);
    m_tracker->start();

    log(tr("Drop an .apk to install it, or any other file to push it to the remote folder."));
}

QWidget* MainWindow::buildServerBox()
{
    auto* box = new QGroupBox(tr("adb server"));
    auto* layout = new QGridLayout(box);
    m_serverStatus = new QLabel(tr("Connecting..."));
    m_adbPath = new QLineEdit(AdbServer::findAdb());
    m_adbPath->setPlaceholderText(tr("Path to adb (not found)"));
    layout->addWidget(new QLabel(tr("Status:")), 0, 0);
    layout->addWidget(m_serverStatus, 0, 1, 1, 3);
    layout->addWidget(new QLabel(tr("adb:")), 1, 0);
    layout->addWidget(m_adbPath, 1, 1, 1, 3);
    layout->addWidget(button(tr("Start server"), box, [this] { startServer(); }), 2, 0);
    layout->addWidget(button(tr("Version"), box,
                             [this] {
                                 watch(m_client->serverVersion(), tr("Server version"),
                                       [this](Reply* r) { log(tr("  version %1").arg(r->outputText())); });
                             }),
                      2, 1);
    layout->addWidget(button(tr("List devices"), box,
                             [this] {
                                 auto* reply = m_client->devices();
                                 watch(reply, tr("List devices"), [this, reply](Reply*) {
                                     for (const DeviceInfo& d : reply->devices())
                                         log(QStringLiteral("  %1  %2  %3").arg(d.serial, d.stateText, d.model));
                                     if (reply->devices().isEmpty())
                                         log(tr("  (none)"));
                                 });
                             }),
                      2, 2);
    layout->addWidget(button(tr("Kill server"), box,
                             [this] { watch(m_client->killServer(), tr("Kill server")); }),
                      2, 3);
    return box;
}

QWidget* MainWindow::buildNetworkBox()
{
    auto* box = new QGroupBox(tr("Wi-Fi"));
    auto* layout = new QGridLayout(box);
    m_host = new QLineEdit;
    m_host->setPlaceholderText(tr("Headset IP address"));
    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(5555);
    layout->addWidget(m_host, 0, 0, 1, 2);
    layout->addWidget(m_port, 0, 2);

    layout->addWidget(button(tr("Get IP (USB)"), box,
                             [this] {
                                 const QString serial = selectedSerial();
                                 if (serial.isEmpty())
                                     return;
                                 auto* reply = m_client->shell(serial, QStringLiteral("ip -f inet addr show wlan0"));
                                 watch(reply, tr("Get Wi-Fi address"), [this, reply](Reply*) {
                                     static const QRegularExpression inet(QStringLiteral("inet (\\d+\\.\\d+\\.\\d+\\.\\d+)"));
                                     const auto match = inet.match(reply->outputText());
                                     if (match.hasMatch()) {
                                         m_host->setText(match.captured(1));
                                         log(tr("  wlan0: %1").arg(match.captured(1)));
                                     } else {
                                         log(tr("  no IPv4 address on wlan0"));
                                     }
                                 });
                             }),
                      1, 0);
    layout->addWidget(button(tr("Enable TCP/IP"), box,
                             [this] {
                                 const QString serial = selectedSerial();
                                 if (serial.isEmpty())
                                     return;
                                 watch(m_client->enableTcpip(serial, quint16(m_port->value())),
                                       tr("Enable TCP/IP on %1").arg(m_port->value()),
                                       [this](Reply* r) { log(QStringLiteral("  ") + r->outputText().trimmed()); });
                             }),
                      1, 1);
    layout->addWidget(button(tr("Connect"), box,
                             [this] {
                                 watch(m_client->connectDevice(m_host->text().trimmed(), quint16(m_port->value())),
                                       tr("Connect %1:%2").arg(m_host->text().trimmed()).arg(m_port->value()),
                                       [this](Reply* r) { log(QStringLiteral("  ") + r->outputText().trimmed()); });
                             }),
                      1, 2);
    layout->addWidget(button(tr("Disconnect"), box,
                             [this] {
                                 const QString address = m_host->text().trimmed().isEmpty()
                                                             ? QString()
                                                             : QStringLiteral("%1:%2").arg(m_host->text().trimmed()).arg(m_port->value());
                                 watch(m_client->disconnectDevice(address), tr("Disconnect %1").arg(address),
                                       [this](Reply* r) { log(QStringLiteral("  ") + r->outputText().trimmed()); });
                             }),
                      2, 0);
    layout->addWidget(button(tr("mDNS services"), box,
                             [this] {
                                 auto* reply = m_client->mdnsServices();
                                 watch(reply, tr("mDNS services"), [this, reply](Reply*) {
                                     for (const MdnsService& s : reply->services()) {
                                         log(QStringLiteral("  %1  %2  %3").arg(s.name, s.type, s.address));
                                         // A phone showing "Pair device with pairing code"
                                         // advertises this service: offer it for pairing.
                                         if (s.type.startsWith(QLatin1String("_adb-tls-pairing")))
                                             m_pairAddress->setText(s.address);
                                     }
                                     if (reply->services().isEmpty())
                                         log(tr("  (none)"));
                                 });
                             }),
                      2, 1);

    // Wireless debugging pairing (Android 11 and later).
    m_pairAddress = new QLineEdit;
    m_pairAddress->setPlaceholderText(tr("Pairing address (IP:port)"));
    m_pairCode = new QLineEdit;
    m_pairCode->setPlaceholderText(tr("Code"));
    m_pairCode->setMaxLength(10);
    layout->addWidget(new QLabel(tr("Pair:")), 3, 0, 1, 3);
    layout->addWidget(m_pairAddress, 4, 0, 1, 2);
    layout->addWidget(m_pairCode, 4, 2);
    layout->addWidget(button(tr("Pair"), box,
                             [this] {
                                 const QString address = m_pairAddress->text().trimmed();
                                 // The code is used once; log the address only.
                                 watch(m_client->pair(address, m_pairCode->text()), tr("Pair with %1").arg(address),
                                       [this](Reply* r) { log(QStringLiteral("  ") + r->outputText().trimmed()); });
                                 m_pairCode->clear();
                             }),
                      5, 0);
    return box;
}

QWidget* MainWindow::buildDeviceBox()
{
    auto* box = new QGroupBox(tr("Selected device"));
    auto* layout = new QGridLayout(box);
    const auto shellButton = [this, box](const QString& text, const QString& command) {
        return button(text, box, [this, command] { runShell(command); });
    };

    layout->addWidget(button(tr("Features"), box,
                             [this] {
                                 const QString serial = selectedSerial();
                                 if (serial.isEmpty())
                                     return;
                                 auto* reply = m_client->features(serial);
                                 watch(reply, tr("Features of %1").arg(serial), [this, reply](Reply*) {
                                     log(QStringLiteral("  ") + reply->features().join(QStringLiteral(", ")));
                                 });
                             }),
                      0, 0);
    layout->addWidget(shellButton(tr("Model / Android"),
                                  QStringLiteral("getprop ro.product.model; getprop ro.build.version.release; getprop ro.build.version.sdk")),
                      0, 1);
    layout->addWidget(shellButton(tr("Battery"), QStringLiteral("dumpsys battery | grep -E 'level|status|powered'")), 0, 2);
    layout->addWidget(shellButton(tr("Stay awake on"), QStringLiteral("svc power stayon true")), 1, 0);
    layout->addWidget(shellButton(tr("Stay awake off"), QStringLiteral("svc power stayon false")), 1, 1);
    layout->addWidget(shellButton(tr("Screen on (wake)"), QStringLiteral("input keyevent KEYCODE_WAKEUP")), 1, 2);
    layout->addWidget(shellButton(tr("Quest: proximity off"),
                                  QStringLiteral("am broadcast -a com.oculus.vrpowermanager.prox_close")),
                      2, 0);
    layout->addWidget(shellButton(tr("Quest: proximity on"),
                                  QStringLiteral("am broadcast -a com.oculus.vrpowermanager.automation_disable")),
                      2, 1);
    layout->addWidget(button(tr("Reboot"), box,
                             [this] {
                                 const QString serial = selectedSerial();
                                 if (!serial.isEmpty())
                                     watch(m_client->reboot(serial), tr("Reboot %1").arg(serial));
                             }),
                      2, 2);
    return box;
}

QWidget* MainWindow::buildFilesBox()
{
    auto* box = new QGroupBox(tr("Files"));
    auto* layout = new QGridLayout(box);
    m_remotePath = new QLineEdit(QStringLiteral("/sdcard/Download/"));
    layout->addWidget(new QLabel(tr("Remote:")), 0, 0);
    layout->addWidget(m_remotePath, 0, 1, 1, 3);

    layout->addWidget(button(tr("List"), box,
                             [this] {
                                 const QString serial = selectedSerial();
                                 if (serial.isEmpty())
                                     return;
                                 auto* reply = m_client->list(serial, m_remotePath->text());
                                 watch(reply, tr("List %1").arg(m_remotePath->text()), [this, reply](Reply*) {
                                     for (const RemoteFileInfo& e : reply->entries()) {
                                         log(QStringLiteral("  %1 %2 %3  %4")
                                                 .arg(e.isDir() ? QStringLiteral("d") : QStringLiteral("-"))
                                                 .arg(humanSize(e.size), 10)
                                                 .arg(e.modified.toString(Qt::ISODate), e.name));
                                     }
                                     log(tr("  %n entries", nullptr, int(reply->entries().size())));
                                 });
                             }),
                      1, 0);
    layout->addWidget(button(tr("Stat"), box,
                             [this] {
                                 const QString serial = selectedSerial();
                                 if (serial.isEmpty())
                                     return;
                                 auto* reply = m_client->stat(serial, m_remotePath->text());
                                 watch(reply, tr("Stat %1").arg(m_remotePath->text()), [this, reply](Reply*) {
                                     const RemoteFileInfo info = reply->info();
                                     if (!info.exists())
                                         log(tr("  does not exist"));
                                     else
                                         log(tr("  mode %1, %2, modified %3")
                                                 .arg(info.mode, 0, 8)
                                                 .arg(humanSize(info.size), info.modified.toString(Qt::ISODate)));
                                 });
                             }),
                      1, 1);
    layout->addWidget(button(tr("Push file..."), box,
                             [this] {
                                 const QString path = QFileDialog::getOpenFileName(this, tr("Push file"));
                                 if (!path.isEmpty())
                                     pushFile(path);
                             }),
                      1, 2);
    layout->addWidget(button(tr("Pull file..."), box,
                             [this] {
                                 const QString serial = selectedSerial();
                                 if (serial.isEmpty())
                                     return;
                                 const QString remote = m_remotePath->text();
                                 const QString local = QFileDialog::getSaveFileName(
                                     this, tr("Pull file"), QFileInfo(remote).fileName());
                                 if (local.isEmpty())
                                     return;
                                 watch(m_client->pull(serial, remote, local), tr("Pull %1 to %2").arg(remote, local));
                             }),
                      1, 3);
    return box;
}

QWidget* MainWindow::buildPackagesBox()
{
    auto* box = new QGroupBox(tr("Packages"));
    auto* layout = new QGridLayout(box);
    m_grant = new QCheckBox(tr("Grant permissions (-g)"));
    m_downgrade = new QCheckBox(tr("Allow downgrade (-d)"));
    layout->addWidget(m_grant, 0, 0, 1, 2);
    layout->addWidget(m_downgrade, 0, 2, 1, 2);
    layout->addWidget(button(tr("Install APK..."), box,
                             [this] {
                                 const QString path = QFileDialog::getOpenFileName(
                                     this, tr("Install APK"), {}, tr("Android packages (*.apk)"));
                                 if (!path.isEmpty())
                                     installApk(path);
                             }),
                      1, 0, 1, 2);
    layout->addWidget(button(tr("List 3rd-party"), box,
                             [this] { runShell(QStringLiteral("pm list packages -3")); }),
                      1, 2, 1, 2);

    m_package = new QLineEdit;
    m_package->setPlaceholderText(tr("com.example.game"));
    layout->addWidget(m_package, 2, 0, 1, 2);
    layout->addWidget(button(tr("Uninstall"), box,
                             [this] {
                                 const QString serial = selectedSerial();
                                 if (serial.isEmpty() || m_package->text().trimmed().isEmpty())
                                     return;
                                 watch(m_client->uninstall(serial, m_package->text().trimmed()),
                                       tr("Uninstall %1").arg(m_package->text().trimmed()),
                                       [this](Reply* r) { log(QStringLiteral("  ") + r->outputText().trimmed()); });
                             }),
                      2, 2);
    layout->addWidget(button(tr("Launch"), box,
                             [this] {
                                 const QString package = m_package->text().trimmed();
                                 if (!package.isEmpty())
                                     runShell(protocol::shellCommand({QStringLiteral("monkey"), QStringLiteral("-p"), package,
                                                                      QStringLiteral("-c"), QStringLiteral("android.intent.category.LAUNCHER"),
                                                                      QStringLiteral("1")}));
                             }),
                      2, 3);
    return box;
}

QWidget* MainWindow::buildShellBox()
{
    auto* box = new QWidget;
    auto* layout = new QHBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    m_shellCommand = new QLineEdit;
    m_shellCommand->setPlaceholderText(tr("Shell command for the selected device, for example: ls /sdcard"));
    connect(m_shellCommand, &QLineEdit::returnPressed, this, [this] { runShell(m_shellCommand->text()); });
    layout->addWidget(new QLabel(tr("shell$")));
    layout->addWidget(m_shellCommand);
    layout->addWidget(button(tr("Run"), box, [this] { runShell(m_shellCommand->text()); }));
    return box;
}

QString MainWindow::selectedSerial()
{
    const QTreeWidgetItem* item = m_devices->currentItem();
    if (item)
        return item->text(0);
    // With exactly one device, use it.
    if (m_devices->topLevelItemCount() == 1)
        return m_devices->topLevelItem(0)->text(0);
    log(tr("Select a device first."));
    return {};
}

void MainWindow::updateDeviceList()
{
    const QString selected = m_devices->currentItem() ? m_devices->currentItem()->text(0) : QString();
    m_devices->clear();
    for (const DeviceInfo& d : m_tracker->devices()) {
        auto* item = new QTreeWidgetItem(m_devices,
                                         {d.serial, d.stateText, d.model, d.product,
                                          d.transportId >= 0 ? QString::number(d.transportId) : QString(),
                                          d.usb});
        if (d.serial == selected)
            m_devices->setCurrentItem(item);
    }
}

void MainWindow::log(const QString& text)
{
    m_log->appendPlainText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz ")) + text);
}

void MainWindow::watch(Reply* reply, const QString& what, std::function<void(Reply*)> onSuccess)
{
    log(what + QStringLiteral("..."));
    QElapsedTimer* timer = new QElapsedTimer;
    timer->start();

    connect(reply, &Reply::progress, this, [this, reply](qint64 done, qint64 total) {
        if (m_transfer != reply) {
            m_transfer = reply;
            m_abort->setEnabled(true);
        }
        m_progress->setValue(total > 0 ? int(done * 1000 / total) : 0);
        m_progress->setFormat(total > 0 ? QStringLiteral("%1 / %2").arg(humanSize(quint64(done)), humanSize(quint64(total)))
                                        : humanSize(quint64(done)));
    });
    connect(reply, &Reply::finished, this, [this, reply, what, onSuccess, timer] {
        const qint64 ms = timer->elapsed();
        delete timer;
        if (reply->isSuccess()) {
            log(tr("%1: OK (%2 ms)").arg(what).arg(ms));
            if (onSuccess)
                onSuccess(reply);
        } else {
            log(tr("%1: %2: %3 (%4 ms)").arg(what, errorName(reply->error()), reply->errorString()).arg(ms));
            if (!reply->output().isEmpty())
                log(QStringLiteral("  ") + reply->outputText().trimmed());
        }
        if (m_transfer == reply) {
            m_abort->setEnabled(false);
            if (reply->isSuccess())
                m_progress->setValue(1000);
        }
        reply->deleteLater();
    });
}

void MainWindow::runShell(const QString& command)
{
    const QString serial = selectedSerial();
    if (serial.isEmpty() || command.trimmed().isEmpty())
        return;
    auto* reply = m_client->shell(serial, command);
    watch(reply, QStringLiteral("shell %1: %2").arg(serial, command), [this, reply](Reply*) {
        for (const QString& line : reply->outputText().split(QLatin1Char('\n'))) {
            if (!line.trimmed().isEmpty())
                log(QStringLiteral("  ") + line.trimmed());
        }
        for (const QString& line : QString::fromUtf8(reply->standardError()).split(QLatin1Char('\n'))) {
            if (!line.trimmed().isEmpty())
                log(QStringLiteral("  stderr: ") + line.trimmed());
        }
        log(reply->hasExitCode() ? tr("  exit code %1").arg(reply->exitCode()) : tr("  exit code unknown"));
    });
}

void MainWindow::startServer()
{
    watch(AdbServer::start(m_adbPath->text(), m_tracker->server(), this), tr("Start adb server"),
          [this](Reply* r) {
              if (!r->output().isEmpty())
                  log(QStringLiteral("  ") + r->outputText().trimmed());
          });
}

QString MainWindow::remoteDir() const
{
    QString dir = m_remotePath->text().trimmed();
    if (!dir.endsWith(QLatin1Char('/')))
        dir = dir.left(dir.lastIndexOf(QLatin1Char('/')) + 1);
    return dir.isEmpty() ? QStringLiteral("/sdcard/Download/") : dir;
}

void MainWindow::installApk(const QString& path)
{
    const QString serial = selectedSerial();
    if (serial.isEmpty())
        return;
    InstallOptions options;
    options.grantPermissions = m_grant->isChecked();
    options.allowDowngrade = m_downgrade->isChecked();
    watch(m_client->install(serial, path, options), tr("Install %1").arg(QFileInfo(path).fileName()),
          [this](Reply* r) { log(QStringLiteral("  ") + r->outputText().trimmed()); });
}

void MainWindow::pushFile(const QString& path)
{
    const QString serial = selectedSerial();
    if (serial.isEmpty())
        return;
    const QString remote = remoteDir() + QFileInfo(path).fileName();
    watch(m_client->push(serial, path, remote), tr("Push %1 to %2").arg(QFileInfo(path).fileName(), remote));
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    for (const QUrl& url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (path.isEmpty() || !QFileInfo(path).isFile())
            continue;
        if (path.endsWith(QLatin1String(".apk"), Qt::CaseInsensitive))
            installApk(path);
        else
            pushFile(path);
    }
}
