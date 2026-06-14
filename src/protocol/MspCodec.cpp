#include "protocol/MspCodec.h"

#include <QDataStream>

namespace {
quint16 readU16Le(const QByteArray &payload, int offset)
{
    if (offset + 2 > payload.size()) {
        return 0;
    }
    return static_cast<quint8>(payload[offset]) |
        (static_cast<quint16>(static_cast<quint8>(payload[offset + 1])) << 8);
}

quint32 readU32Le(const QByteArray &payload, int offset)
{
    if (offset + 4 > payload.size()) {
        return 0;
    }
    return static_cast<quint8>(payload[offset]) |
        (static_cast<quint32>(static_cast<quint8>(payload[offset + 1])) << 8) |
        (static_cast<quint32>(static_cast<quint8>(payload[offset + 2])) << 16) |
        (static_cast<quint32>(static_cast<quint8>(payload[offset + 3])) << 24);
}
}

MspCodec::MspCodec(QObject *parent) : QObject(parent) {}

QByteArray MspCodec::encodeV1(quint16 command, const QByteArray &payload)
{
    QByteArray frame;
    frame.reserve(6 + payload.size());
    frame.append('$');
    frame.append('M');
    frame.append('<');
    frame.append(static_cast<char>(payload.size()));
    frame.append(static_cast<char>(command & 0xff));

    quint8 checksum = static_cast<quint8>(payload.size()) ^ static_cast<quint8>(command & 0xff);
    for (char byte : payload) {
        checksum ^= static_cast<quint8>(byte);
        frame.append(byte);
    }
    frame.append(static_cast<char>(checksum));
    return frame;
}

QString MspCodec::commandName(quint16 command)
{
    switch (command) {
    case ApiVersion: return QStringLiteral("MSP_API_VERSION");
    case FcVariant: return QStringLiteral("MSP_FC_VARIANT");
    case FcVersion: return QStringLiteral("MSP_FC_VERSION");
    case BoardInfo: return QStringLiteral("MSP_BOARD_INFO");
    case BuildInfo: return QStringLiteral("MSP_BUILD_INFO");
    case RawImu: return QStringLiteral("MSP_RAW_IMU");
    case Attitude: return QStringLiteral("MSP_ATTITUDE");
    case Status: return QStringLiteral("MSP_STATUS");
    case StatusEx: return QStringLiteral("MSP_STATUS_EX");
    case RxConfig: return QStringLiteral("MSP_RX_CONFIG");
    case SetRxConfig: return QStringLiteral("MSP_SET_RX_CONFIG");
    case EepromWrite: return QStringLiteral("MSP_EEPROM_WRITE");
    case Reboot: return QStringLiteral("MSP_REBOOT");
    default: return QStringLiteral("MSP_%1").arg(command);
    }
}

FcStatus MspCodec::parseStatus(const MspMessage &message)
{
    FcStatus status;
    const QByteArray &p = message.payload;
    if (p.size() < 11) {
        return status;
    }

    status.loopTimeUs = readU16Le(p, 0);
    status.i2cErrors = readU16Le(p, 2);
    const quint16 sensors = readU16Le(p, 4);
    status.accelPresent = sensors & (1 << 0);
    status.baroPresent = sensors & (1 << 1);
    status.magPresent = sensors & (1 << 2);
    status.gpsPresent = sensors & (1 << 3);
    status.gyroPresent = sensors & (1 << 5);
    status.modeMask = readU32Le(p, 6);

    if (p.size() >= 14) {
        status.cpuLoad = readU16Le(p, 11);
    }

    if (p.size() >= 21) {
        status.armingDisableFlags = readU32Le(p, p.size() - 5);
    }

    return status;
}

void MspCodec::consume(const QByteArray &bytes)
{
    for (char byte : bytes) {
        consumeByte(static_cast<quint8>(byte));
    }
}

void MspCodec::reset()
{
    m_state = State::Idle;
    m_direction = 0;
    m_payloadSize = 0;
    m_checksum = 0;
    m_message = MspMessage{};
}

void MspCodec::consumeByte(quint8 byte)
{
    switch (m_state) {
    case State::Idle:
        if (byte == '$') {
            m_state = State::HeaderStart;
        }
        break;
    case State::HeaderStart:
        m_state = byte == 'M' ? State::HeaderM : State::Idle;
        break;
    case State::HeaderM:
        if (byte == '>' || byte == '!') {
            m_direction = byte;
            m_message.error = byte == '!';
            m_state = State::Size;
        } else {
            reset();
        }
        break;
    case State::Direction:
        reset();
        break;
    case State::Size:
        m_payloadSize = byte;
        m_checksum = byte;
        m_message.payload.clear();
        m_state = State::Command;
        break;
    case State::Command:
        m_message.command = byte;
        m_checksum ^= byte;
        m_state = m_payloadSize == 0 ? State::Checksum : State::Payload;
        break;
    case State::Payload:
        m_message.payload.append(static_cast<char>(byte));
        m_checksum ^= byte;
        if (m_message.payload.size() == m_payloadSize) {
            m_state = State::Checksum;
        }
        break;
    case State::Checksum:
        if (m_checksum == byte) {
            emit messageReceived(m_message);
        } else {
            emit parseError(QStringLiteral("Invalid MSP checksum"));
        }
        reset();
        break;
    }
}
