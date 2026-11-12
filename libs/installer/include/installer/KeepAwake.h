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

// True when `dumpsys vrpowermanager` output says the sensor is overridden ("Virtual proximity
// state: CLOSE"). Without an override it says DISABLED.
bool isProximityOverridden(const QByteArray& dumpsysOutput);

// Keeps a headset's screen on while it is held: the headset acts as if it were worn, so it does not
// sleep about 20 seconds after it is taken off.
//
// start() first reads the headset's state. If another app (such as one on the headset) already
// overrides the sensor, it leaves that override alone (Deferring) and looks again every
// renewInterval(), taking over if the other app lets go. Otherwise it sends kKeepAwakeAction
// (Holding) and sends it again every renewInterval(), because the override lapses some minutes
// after its last renewal. stop(), or the destructor, sends kReleaseAwakeAction only when this
// object holds the override. If the release never reaches the headset, the override lapses.
class KeepAwake : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Off,       // Not started.
        Checking,  // Started; reading the headset's state.
        Deferring, // Another app keeps the screen on.
        Holding,   // This object keeps the screen on.
    };
    Q_ENUM(State)

    KeepAwake(qadb::Client* client, const QString& serial, QObject* parent = nullptr);
    ~KeepAwake() override;

    void start();
    // Returns the release's reply, which belongs to the client, or nullptr when nothing was
    // released (not holding).
    qadb::Reply* stop();
    // Stops without the release, for when another KeepAwake takes over the headset.
    void abandon();
    bool isActive() const { return m_state != State::Off; }
    State state() const { return m_state; }
    QString serial() const { return m_serial; }

    // Stops every active KeepAwake and waits up to waitMsecs for the headsets to answer the
    // releases. qgp calls it before it exits; works also after the application's event loop ended.
    static void releaseAll(int waitMsecs);

    int renewInterval() const { return m_timer.interval(); }
    void setRenewInterval(int msec) { m_timer.setInterval(msec); }

signals:
    void stateChanged(installer::KeepAwake::State state);
    // A broadcast failed. Sent once per start(); the override is not critical to an install.
    void failed(const QString& errorMessage);

private:
    void check();
    void setState(State state);
    qadb::Reply* broadcast(const char* action);

    qadb::Client* m_client;
    QString m_serial;
    QTimer m_timer;
    State m_state = State::Off;
    bool m_reportedFailure = false;
    int m_generation = 0; // Ignores a check that a stop() overtook.
};

} // namespace installer
