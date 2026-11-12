#pragma once

#include <QLockFile>
#include <QObject>
#include <QString>

class QLocalServer;

// Keeps qgp to one running instance per user (decision 0012). The first instance holds a lock file
// and listens on a local socket. A later instance finds the lock taken, sends its catalog argument
// over the socket and quits; the first instance then raises its window and opens that catalog.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject* parent = nullptr);
    ~SingleInstance() override;

    // Takes the lock and starts listening. Returns false when another instance holds the lock;
    // the message (a catalog path, or empty) has then been passed on to it.
    bool claim(const QString& message);

signals:
    // Another instance was started. message is its catalog argument, or empty.
    void activated(const QString& message);

private:
    void readConnection();

    QLockFile m_lock;
    QLocalServer* m_server = nullptr;
};
