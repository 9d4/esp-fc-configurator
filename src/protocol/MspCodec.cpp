#include "protocol/MspCodec.h"

#include <QStringList>

#include <algorithm>

namespace {
quint16 readU16Le(const QByteArray &payload, int offset)
{
    if (offset + 2 > payload.size()) {
        return 0;
    }
    return static_cast<quint8>(payload[offset]) |
        (static_cast<quint16>(static_cast<quint8>(payload[offset + 1])) << 8);
}

qint16 readI16Le(const QByteArray &payload, int offset)
{
    return static_cast<qint16>(readU16Le(payload, offset));
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

QString asciiString(const QByteArray &payload, int offset, int length)
{
    if (offset < 0 || length <= 0 || offset >= payload.size()) {
        return {};
    }
    const qsizetype count = std::min<qsizetype>(length, payload.size() - offset);
    return QString::fromLatin1(payload.mid(offset, count)).trimmed();
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

QByteArray MspCodec::encodeSetRxProvider(const RxConfig &config, quint8 provider)
{
    QByteArray payload = config.rawPayload;
    if (payload.isEmpty()) {
        payload.resize(12);
        payload[1] = static_cast<char>(config.maxCheck & 0xff);
        payload[2] = static_cast<char>((config.maxCheck >> 8) & 0xff);
        payload[3] = static_cast<char>(config.midRc & 0xff);
        payload[4] = static_cast<char>((config.midRc >> 8) & 0xff);
        payload[5] = static_cast<char>(config.minCheck & 0xff);
        payload[6] = static_cast<char>((config.minCheck >> 8) & 0xff);
        payload[8] = static_cast<char>(config.minRc & 0xff);
        payload[9] = static_cast<char>((config.minRc >> 8) & 0xff);
        payload[10] = static_cast<char>(config.maxRc & 0xff);
        payload[11] = static_cast<char>((config.maxRc >> 8) & 0xff);
    }
    payload[0] = static_cast<char>(provider);
    return payload;
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
    case RxConfigCommand: return QStringLiteral("MSP_RX_CONFIG");
    case SetRxConfigCommand: return QStringLiteral("MSP_SET_RX_CONFIG");
    case EepromWrite: return QStringLiteral("MSP_EEPROM_WRITE");
    case Reboot: return QStringLiteral("MSP_REBOOT");
    default: return QStringLiteral("MSP_%1").arg(command);
    }
}

FirmwareInfo MspCodec::parseApiVersion(const MspMessage &message, FirmwareInfo current)
{
    const QByteArray &p = message.payload;
    if (p.size() >= 3) {
        current.mspProtocol = static_cast<quint8>(p[0]);
        current.apiMajor = static_cast<quint8>(p[1]);
        current.apiMinor = static_cast<quint8>(p[2]);
    }
    return current;
}

FirmwareInfo MspCodec::parseFcVariant(const MspMessage &message, FirmwareInfo current)
{
    current.variant = QString::fromLatin1(message.payload).trimmed();
    return current;
}

FirmwareInfo MspCodec::parseFcVersion(const MspMessage &message, FirmwareInfo current)
{
    const QByteArray &p = message.payload;
    if (p.size() >= 3) {
        current.version = QStringLiteral("%1.%2.%3")
            .arg(static_cast<quint8>(p[0]))
            .arg(static_cast<quint8>(p[1]))
            .arg(static_cast<quint8>(p[2]));
    }
    return current;
}

FirmwareInfo MspCodec::parseBoardInfo(const MspMessage &message, FirmwareInfo current)
{
    const QByteArray &p = message.payload;
    if (p.size() >= 4) {
        current.board = QString::fromLatin1(p.left(4)).trimmed();
    }
    if (p.size() >= 9) {
        const int targetLength = static_cast<quint8>(p[8]);
        current.target = asciiString(p, 9, targetLength);
    }
    return current;
}

FirmwareInfo MspCodec::parseBuildInfo(const MspMessage &message, FirmwareInfo current)
{
    const QByteArray &p = message.payload;
    current.buildDate = asciiString(p, 0, 11);
    current.buildTime = asciiString(p, 11, 8);
    current.gitRevision = asciiString(p, 19, 7);
    return current;
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

RxConfig MspCodec::parseRxConfig(const MspMessage &message)
{
    RxConfig config;
    const QByteArray &p = message.payload;
    config.rawPayload = p;
    if (p.size() >= 1) {
        config.serialRxProvider = static_cast<quint8>(p[0]);
    }
    if (p.size() >= 12) {
        config.maxCheck = readU16Le(p, 1);
        config.midRc = readU16Le(p, 3);
        config.minCheck = readU16Le(p, 5);
        config.minRc = readU16Le(p, 8);
        config.maxRc = readU16Le(p, 10);
    }
    return config;
}

AttitudeState MspCodec::parseAttitude(const MspMessage &message)
{
    AttitudeState attitude;
    const QByteArray &p = message.payload;
    if (p.size() < 6) {
        return attitude;
    }
    attitude.rollDeg = static_cast<float>(readI16Le(p, 0)) / 10.0f;
    attitude.pitchDeg = static_cast<float>(readI16Le(p, 2)) / 10.0f;
    attitude.yawDeg = static_cast<float>(readI16Le(p, 4));
    attitude.valid = true;
    return attitude;
}

RawImuState MspCodec::parseRawImu(const MspMessage &message)
{
    RawImuState imu;
    const QByteArray &p = message.payload;
    if (p.size() < 18) {
        return imu;
    }
    for (int i = 0; i < 3; ++i) {
        imu.acc[i] = readI16Le(p, i * 2);
        imu.gyro[i] = readI16Le(p, 6 + i * 2);
        imu.mag[i] = readI16Le(p, 12 + i * 2);
    }
    imu.valid = true;
    return imu;
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
