#pragma once

#include "qadb/Reply.h"
#include "qadb/Types.h"

#include <QDeadlineTimer>
#include <QIcon>
#include <QHash>
#include <QList>
#include <QSet>
#include <QObject>
#include <QPointer>
#include <QString>

#include <optional>

namespace installer {
class KeepAwake;
}
namespace qadb {
class Client;
class DeviceTracker;
} // namespace qadb

// The app's link to the headsets: starts the adb server when it is not running, keeps the list of
// connected headsets, and remembers the one headset the user chose (the current headset).
//
// The current headset is remembered by its adb serial and by its hardware serial (ro.serialno,
// decision 0004). When the remembered adb serial is not online, for example because wireless
// debugging moved to another port, the online device with the same hardware serial becomes current.
class HeadsetConnection : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        StartingAdb,     // Waiting for the adb server.
        AdbMissing,      // The adb program was not found.
        AdbFailed,       // adb did not start.
        NoHeadset,       // No headset chosen yet.
        NotConnected,    // The chosen headset is not connected, or is offline.
        NeedsPermission, // The headset waits for the user to allow USB debugging.
        Connecting,
        Ready,
    };
    Q_ENUM(Status)

    explicit HeadsetConnection(QObject* parent = nullptr);
    ~HeadsetConnection() override;

    // Connects to the adb server, and starts it if needed.
    void start();
    // Tries again to start the adb server after AdbMissing or AdbFailed.
    void retryAdb();

    qadb::Client* client() const { return m_client; }
    bool isAdbRunning() const;
    QList<qadb::DeviceInfo> devices() const;

    QString currentSerial() const { return m_serial; }
    std::optional<qadb::DeviceInfo> currentDevice() const;
    void setCurrentSerial(const QString& serial);

    Status status() const;
    bool isReady() const { return status() == Status::Ready; }
    // A short status, such as "Connected to Quest 3".
    QString statusText() const;
    // What the user can do about the status, or an empty string when there is nothing to do.
    QString statusAdvice() const;

    // Keeps the current headset's screen on (installer::KeepAwake) until turned off or QGPUI exits.
    // It follows the current headset: a headset that becomes current is kept awake, and one that
    // stops being current is released. Off when QGPUI starts.
    bool keepsAwake() const { return m_keepAwakeWanted; }
    void setKeepAwake(bool on);
    // True while the button is on but another app on the headset already keeps it awake, so
    // QGPUI leaves that app's override alone.
    bool isKeptAwakeElsewhere() const;
    // A small open eye, for the keep-awake button.
    static QIcon keepAwakeIcon();

    // Looks for wireless headsets on the local network and connects to the ones it finds.
    // Emits scanFinished.
    void scan();
    bool isScanning() const { return m_scanReply; }
    // Pairs with a headset's wireless debugging, or, when code is empty, connects to address
    // (host or host:port). A headset that connects within a short time becomes the current one.
    // The reply belongs to the caller.
    qadb::Reply* addWirelessHeadset(const QString& address, const QString& code);
    // Moves a headset on a USB cable to Wi-Fi (adb tcpip and connect). On success the Wi-Fi
    // connection becomes the current headset, and the cable can be unplugged. The reply belongs
    // to the caller.
    qadb::Reply* switchToWifi(const QString& serial);

    // "Quest 3", from the device's model.
    static QString displayName(const qadb::DeviceInfo& device);
    // "USB" or "Wi-Fi".
    static QString linkName(const qadb::DeviceInfo& device);
    // The device's state in plain words, such as "Ready".
    static QString stateDescription(const qadb::DeviceInfo& device);
    // A filled circle: green when ok, red otherwise.
    static QIcon statusIcon(bool ok);

signals:
    void devicesChanged();
    void statusChanged();
    // message describes the result for the user. pairingAddress is the address of a headset
    // that offers pairing, or empty.
    void scanFinished(const QString& message, const QString& pairingAddress);
    void keepAwakeChanged(bool on);
    // isKeptAwakeElsewhere() may have changed.
    void keepAwakeStateChanged();
    // A keep-awake broadcast failed on the current headset.
    void keepAwakeFailed(const QString& errorMessage);

private:
    void startServer();
    void trackerDevicesChanged();
    void adoptNewHeadset();
    void reconnectRemembered();
    // Reads ro.serialno of online devices not read yet, then selectByHardwareSerial().
    void lookUpHardwareSerials();
    void selectByHardwareSerial();
    // Starts or stops m_keepAwake to match m_keepAwakeWanted and the current headset.
    void updateKeepAwake();

    qadb::Client* m_client;
    qadb::DeviceTracker* m_tracker;
    QPointer<qadb::Reply> m_startReply;
    QPointer<qadb::Reply> m_scanReply;
    bool m_startTried = false;
    bool m_adbMissing = false;
    bool m_reconnectPending = false;
    QString m_adbError;
    QString m_serial;
    QString m_name;
    QString m_hardwareSerial; // Of the current headset; empty until read.
    QHash<QString, QString> m_hardwareBySerial; // adb serial -> ro.serialno, for online devices.
    QSet<QString> m_hardwarePending;
    // Until then, the next headset that comes online becomes the current one.
    QDeadlineTimer m_adoptUntil;
    QList<QString> m_onlineBefore;
    bool m_keepAwakeWanted = false;
    QPointer<installer::KeepAwake> m_keepAwake; // On the current headset, while it is ready.
};
