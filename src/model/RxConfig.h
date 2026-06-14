#pragma once

#include <QByteArray>
#include <QString>

struct RxConfig
{
    enum Provider : quint8 {
        Spektrum1024 = 0,
        Spektrum2048 = 1,
        Sbus = 2,
        Ibus = 7,
        Crsf = 9,
    };

    quint8 serialRxProvider = Sbus;
    quint16 maxCheck = 1900;
    quint16 midRc = 1500;
    quint16 minCheck = 1050;
    quint16 minRc = 885;
    quint16 maxRc = 2115;
    QByteArray rawPayload;

    static QString providerName(quint8 provider)
    {
        switch (provider) {
        case Sbus: return QStringLiteral("SBUS");
        case Ibus: return QStringLiteral("IBUS");
        case Crsf: return QStringLiteral("CRSF");
        case Spektrum1024: return QStringLiteral("Spektrum 1024");
        case Spektrum2048: return QStringLiteral("Spektrum 2048");
        default: return QStringLiteral("Provider %1").arg(provider);
        }
    }
};
