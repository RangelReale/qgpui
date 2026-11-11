#include "qadb/Protocol.h"

#include <QRegularExpression>
#include <QtEndian>

namespace qadb::protocol {

QByteArray encodeRequest(const QByteArray& payload)
{
    return QByteArray::number(payload.size(), 16).rightJustified(4, '0') + payload;
}

int parseHexLength(const QByteArray& text)
{
    if (text.size() != 4)
        return -1;
    bool ok = false;
    const int value = text.toInt(&ok, 16);
    return ok && value >= 0 ? value : -1;
}

namespace {

// The key:value fields that follow the state in the long device list.
bool isDeviceField(const QString& token)
{
    static const char* const keys[] = {"usb:", "product:", "model:", "device:", "transport_id:"};
    for (const char* key : keys) {
        if (token.startsWith(QLatin1String(key)))
            return true;
    }
    return false;
}

} // namespace

QList<DeviceInfo> parseDeviceList(const QByteArray& payload)
{
    QList<DeviceInfo> devices;
    const QString text = QString::fromUtf8(payload);
    for (QString line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;

        DeviceInfo info;
        const qsizetype tab = line.indexOf(QLatin1Char('\t'));
        if (tab >= 0) {
            // Short form: "<serial>\t<state>".
            info.serial = line.left(tab);
            info.stateText = line.mid(tab + 1).trimmed();
        } else {
            // Long form: "<serial>   <state> usb:1-1 product:x model:y device:z transport_id:3".
            // The state can contain spaces ("no permissions (...)").
            const QStringList tokens = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            info.serial = tokens.value(0);
            qsizetype i = 1;
            QStringList stateParts;
            while (i < tokens.size() && !isDeviceField(tokens.at(i)))
                stateParts << tokens.at(i++);
            info.stateText = stateParts.join(QLatin1Char(' '));
            for (; i < tokens.size(); ++i) {
                const QString& token = tokens.at(i);
                const qsizetype colon = token.indexOf(QLatin1Char(':'));
                const QString key = token.left(colon);
                const QString value = token.mid(colon + 1);
                if (key == QLatin1String("usb"))
                    info.usb = value;
                else if (key == QLatin1String("product"))
                    info.product = value;
                else if (key == QLatin1String("model"))
                    info.model = value;
                else if (key == QLatin1String("device"))
                    info.device = value;
                else if (key == QLatin1String("transport_id"))
                    info.transportId = value.toLongLong();
            }
        }
        info.state = deviceStateFromString(info.stateText);
        devices << info;
    }
    return devices;
}

QList<MdnsService> parseMdnsServices(const QByteArray& payload)
{
    QList<MdnsService> services;
    const QString text = QString::fromUtf8(payload);
    for (const QString& rawLine : text.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        const QStringList fields = line.split(QLatin1Char('\t'));
        if (fields.size() < 3)
            continue;
        services << MdnsService{fields.at(0), fields.at(1), fields.at(2)};
    }
    return services;
}

QStringList parseFeatures(const QByteArray& payload)
{
    QStringList features;
    for (const QByteArray& part : payload.split(',')) {
        const QByteArray name = part.trimmed();
        if (!name.isEmpty())
            features << QString::fromUtf8(name);
    }
    return features;
}

QString shellQuote(const QString& argument)
{
    static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9_@%+=:,./-]+$"));
    if (plain.match(argument).hasMatch())
        return argument;
    QString quoted = argument;
    quoted.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

QString shellCommand(const QStringList& arguments)
{
    QStringList quoted;
    quoted.reserve(arguments.size());
    for (const QString& argument : arguments)
        quoted << shellQuote(argument);
    return quoted.join(QLatin1Char(' '));
}

QByteArray syncHeader(const char id[4], quint32 length)
{
    QByteArray header(id, 4);
    char bytes[4];
    qToLittleEndian(length, bytes);
    header.append(bytes, 4);
    return header;
}

QByteArray syncPacket(const char id[4], const QByteArray& data)
{
    return syncHeader(id, static_cast<quint32>(data.size())) + data;
}

quint32 readLittleEndian32(const char* bytes)
{
    return qFromLittleEndian<quint32>(bytes);
}

QByteArray shellPacket(ShellPacketId id, const QByteArray& data)
{
    QByteArray packet(1, static_cast<char>(id));
    char bytes[4];
    qToLittleEndian(static_cast<quint32>(data.size()), bytes);
    packet.append(bytes, 4);
    packet.append(data);
    return packet;
}

} // namespace qadb::protocol
