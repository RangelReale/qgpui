# qadb

A Qt 6 client for the adb server. It talks to the server over TCP (127.0.0.1:5037), the same way the
`adb` command does, so it needs no adb library. The `adb` program must be installed, because it runs
the server.

## Features

| Area | Requests |
|---|---|
| Server | version, list devices, live device tracking, kill server, start server |
| Network | pair with wireless debugging (pairing code), connect and disconnect Wi-Fi devices, enable TCP/IP on a USB device, switch a USB device to Wi-Fi in one step, mDNS services |
| Device | shell (with exit code and separate stderr), exec, reboot, features |
| Files | push (from a file or bytes), pull, stat, list, with progress |
| Packages | install APK (streamed, or copied then `pm install`), uninstall, list installed packages with versionCode, one package's version and install times |
| Device state | free space (`stat -f`), system properties such as the hardware serial `ro.serialno` |

## Use

Link the `qgp::qadb` target and include `<qadb/qadb.h>`.

```cpp
auto* client = new qadb::Client(this);

auto* reply = client->install(serial, QStringLiteral("C:/games/game.apk"));
connect(reply, &qadb::Reply::progress, this, [](qint64 done, qint64 total) { /* ... */ });
connect(reply, &qadb::Reply::finished, this, [reply] {
    if (reply->isSuccess())
        qInfo() << "installed";
    else
        qWarning() << reply->errorString();
    reply->deleteLater();
});
```

Every request returns a `qadb::Reply` (or a subclass with typed results, such as `DevicesReply`
or `ShellReply`). Requests start on the next event-loop iteration. `Reply::waitForFinished()`
blocks with a local event loop. It is meant for tools and tests.

`qadb::DeviceTracker` keeps a live device list and emits `deviceAdded`, `deviceRemoved` and
`deviceChanged`. It reconnects on its own when the adb server restarts.

`qadb::AdbServer::findAdb()` looks for the `adb` program and `AdbServer::start()` starts the server.
An application can ship adb in a `platform-tools` folder next to its executable; `findAdb()`
looks there right after the `ADB` environment variable.

## Errors

`Reply::error()` says what went wrong:

- `ConnectionFailed`: the adb server is not running or not reachable.
- `ServerError`: the server or device refused the request. `errorString()` holds its message,
  such as "device 'X' not found".
- `CommandFailed`: the request ran but failed, such as an install answered with "Failure [...]".
- `FileError`: a local file could not be read or written.
- `ProtocolError`, `Aborted`, `Timeout`.

A shell command that exits with a non-zero code is not an error. Check `ShellReply::exitCode()`.
