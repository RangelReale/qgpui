#pragma once

#include <qadb/qadb.h>

#include <QMainWindow>
#include <QPointer>

#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTreeWidget;

// A debug window for the qadb library: every button runs one library request and logs the result.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QWidget* buildServerBox();
    QWidget* buildNetworkBox();
    QWidget* buildDeviceBox();
    QWidget* buildFilesBox();
    QWidget* buildPackagesBox();
    QWidget* buildShellBox();

    // The selected device's serial, or empty (with a log line) when none is selected.
    QString selectedSerial();
    void updateDeviceList();

    void log(const QString& text);
    // Logs the request, follows its progress, and logs its result. `onSuccess` adds detail.
    void watch(qadb::Reply* reply, const QString& what,
               std::function<void(qadb::Reply*)> onSuccess = {});
    void runShell(const QString& command);

    void startServer();
    void installApk(const QString& path);
    void pushFile(const QString& path);
    QString remoteDir() const;

    qadb::Client* m_client = nullptr;
    qadb::DeviceTracker* m_tracker = nullptr;

    QLabel* m_serverStatus = nullptr;
    QLineEdit* m_adbPath = nullptr;
    QTreeWidget* m_devices = nullptr;
    QLineEdit* m_host = nullptr;
    QSpinBox* m_port = nullptr;
    QLineEdit* m_pairAddress = nullptr;
    QLineEdit* m_pairCode = nullptr;
    QLineEdit* m_remotePath = nullptr;
    QLineEdit* m_package = nullptr;
    QCheckBox* m_grant = nullptr;
    QCheckBox* m_downgrade = nullptr;
    QLineEdit* m_shellCommand = nullptr;
    QPlainTextEdit* m_log = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_abort = nullptr;
    QPointer<qadb::Reply> m_transfer;
};
