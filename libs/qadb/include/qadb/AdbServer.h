#pragma once

#include "qadb/Reply.h"
#include "qadb/Types.h"

#include <QString>

class QObject;

namespace qadb {

// Finds and starts the `adb` program, which runs the adb server. The library talks to the server
// over TCP and does not need the program otherwise.
class AdbServer
{
public:
    // Looks for adb in, in order: the ADB environment variable, ANDROID_HOME and ANDROID_SDK_ROOT
    // (platform-tools), PATH, and the Android SDK's default install folder. Returns an empty
    // string when it finds none.
    static QString findAdb();

    // Runs `adb start-server`, which returns once the server is listening. When `server` is not
    // the default port, ANDROID_ADB_SERVER_PORT is set for the process. The reply belongs to
    // the caller.
    static Reply* start(const QString& adbPath, const ServerAddress& server = {},
                        QObject* parent = nullptr);
};

} // namespace qadb
