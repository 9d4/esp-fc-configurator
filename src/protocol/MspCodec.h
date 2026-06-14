#pragma once

#include "model/FcStatus.h"

#include <QByteArray>
#include <QObject>
#include <QQueue>

struct MspMessage
{
    quint8 version = 1;
    quint16 command = 0;
    QByteArray payload;
    bool error = false;
};

class MspCodec final : public QObject
{
    Q_OBJECT

public:
    enum Command : quint16 {
        ApiVersion = 1,
        FcVariant = 2,
        FcVersion = 3,
        BoardInfo = 4,
        BuildInfo = 5,
        RawImu = 102,
        Attitude = 108,
        Status = 101,
        StatusEx = 150,
        RxConfig = 44,
        SetRxConfig = 45,
        EepromWrite = 250,
        Reboot = 68,
    };

    explicit MspCodec(QObject *parent = nullptr);

    static QByteArray encodeV1(quint16 command, const QByteArray &payload = {});
    static QString commandName(quint16 command);
    static FcStatus parseStatus(const MspMessage &message);

    void consume(const QByteArray &bytes);

signals:
    void messageReceived(const MspMessage &message);
    void parseError(const QString &message);

private:
    enum class State {
        Idle,
        HeaderStart,
        HeaderM,
        Direction,
        Size,
        Command,
        Payload,
        Checksum,
    };

    void reset();
    void consumeByte(quint8 byte);

    State m_state = State::Idle;
    quint8 m_direction = 0;
    quint8 m_payloadSize = 0;
    quint8 m_checksum = 0;
    MspMessage m_message;
};
