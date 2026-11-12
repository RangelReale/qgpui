#pragma once

#include "qadb/Types.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

// Encoders and parsers for the adb wire format. Client and DeviceTracker use them; they are public
// so that tests and tools can use them too.
namespace qadb::protocol {

// A host request: the payload's length as 4 lowercase hex digits, then the payload.
QByteArray encodeRequest(const QByteArray& payload);

// Parses 4 hex digits. Returns -1 when the text is not exactly 4 hex digits.
int parseHexLength(const QByteArray& text);

// Parses the payload of host:devices, host:devices-l and host:track-devices[-l]. Both the short
// (tab-separated) and long (key:value) forms are accepted.
QList<DeviceInfo> parseDeviceList(const QByteArray& payload);

// Parses the payload of host:mdns:services.
QList<MdnsService> parseMdnsServices(const QByteArray& payload);

// Parses the comma-separated payload of <host-prefix>:features.
QStringList parseFeatures(const QByteArray& payload);

// Parses `pm list packages --show-versioncode`: lines such as
// "package:com.example.game versionCode:101". Lines without "package:" are skipped.
QList<PackageInfo> parsePackageList(const QByteArray& output);

// Parses `dumpsys package <name>` for that package: versionCode, versionName, firstInstallTime
// and lastUpdateTime of its first "Package [<name>]" section. Returns nothing when the output has
// no such section, which is what dumpsys prints for a package that is not installed.
std::optional<PackageInfo> parsePackageDump(const QString& packageName, const QByteArray& output);

// Parses `stat -f -c '%S %b %a'`: block size, total blocks, blocks available to apps.
std::optional<StorageInfo> parseStatFs(const QByteArray& output);

// Quotes one argument for the device's shell (mksh). Plain words are returned as they are.
QString shellQuote(const QString& argument);

// Joins arguments into one shell command line, quoting each of them.
QString shellCommand(const QStringList& arguments);

// The sync protocol: 4-byte id and 4-byte little-endian length, followed by `data`.
QByteArray syncPacket(const char id[4], const QByteArray& data);
QByteArray syncHeader(const char id[4], quint32 length);
quint32 readLittleEndian32(const char* bytes);

// Shell protocol v2 packet ids.
enum ShellPacketId : quint8 {
    ShellStdin = 0,
    ShellStdout = 1,
    ShellStderr = 2,
    ShellExit = 3,
    ShellCloseStdin = 4,
    ShellWindowSizeChange = 5,
};
QByteArray shellPacket(ShellPacketId id, const QByteArray& data = {});

// The longest remote path the sync protocol accepts.
constexpr int kSyncMaxPathLength = 1024;
// The largest DATA chunk in the sync protocol.
constexpr int kSyncMaxChunk = 64 * 1024;

} // namespace qadb::protocol
