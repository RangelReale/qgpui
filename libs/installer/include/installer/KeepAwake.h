#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace qadb {
class Client;
class Reply;
} // namespace qadb

namespace installer {

// Broadcasts that override a Meta Quest's proximity sensor. They come from Meta's testing tools.
// Tested on a Quest 3 (see "Keeping the screen on" in docs/research/quest-adb-behaviour.md).
inline constexpr char kKeepAwakeAction[] = "com.oculus.vrpowermanager.prox_close";
inline constexpr char kReleaseAwakeAction[] = "com.oculus.vrpowermanager.automation_disable";

// Keeps a headset's screen on while it is held: the headset acts as if it were worn, so it does not
// sleep about 20 seconds after it is taken off. start() sends kKeepAwakeAction and sends it again
// every renewInterval(), because the override lapses some minutes after its last renewal. stop(),
// or the destructor, sends kReleaseAwakeAction, which hands control back to the sensor. If the
// release never reaches the headset, the override lapses by itself.
class KeepAwake : public QObject
{
    Q_OBJECT

public:
    KeepAwake(qadb::Client* client, const QString& serial, QObject* parent = nullptr);
    ~KeepAwake() override;

    void start();
    // Returns the release's reply, which belongs to the client, or nullptr when not active.
    qadb::Reply* stop();
    // Stops renewing without the release, for when another KeepAwake takes over the headset.
    void abandon();
    bool isActive() const { return m_active; }
    QString serial() const { return m_serial; }

    int renewInterval() const { return m_timer.interval(); }
    void setRenewInterval(int msec) { m_timer.setInterval(msec); }

signals:
    // A broadcast failed. Sent once per start(); the override is not critical to an install.
    void failed(const QString& errorMessage);

private:
    qadb::Reply* broadcast(const char* action);

    qadb::Client* m_client;
    QString m_serial;
    QTimer m_timer;
    bool m_active = false;
    bool m_reportedFailure = false;
};

} // namespace installer
