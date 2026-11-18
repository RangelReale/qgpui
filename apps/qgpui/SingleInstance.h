#pragma once

#include <QLockFile>
#include <QObject>
#include <QString>

class QLocalServer;

// Keeps QGPUI to one running instance per user (decision 0012). The first instance holds a lock file
// and listens on a local socket. A later instance finds the lock taken, connects to the socket and
// quits; the first instance then raises its window.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject* parent = nullptr);
    ~SingleInstance() override;

    // Takes the lock and starts listening. Returns false when another instance holds the lock;
    // it has then been told to come to the front.
    bool claim();

signals:
    // Another instance was started.
    void activated();

private:
    void readConnection();

    QLockFile m_lock;
    QLocalServer* m_server = nullptr;
};
