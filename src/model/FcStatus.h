#pragma once

#include <QString>
#include <QStringList>

struct FcStatus
{
    QString apiVersion;
    QString fcVariant;
    QString fcVersion;
    QString boardInfo;
    QString buildInfo;
    QString devices;
    QStringList armingFlags;
    int loopTimeUs = 0;
    int i2cErrors = 0;
    int cpuLoad = 0;
    quint32 modeMask = 0;
    quint32 armingDisableFlags = 0;
    bool gyroPresent = false;
    bool accelPresent = false;
    bool baroPresent = false;
    bool magPresent = false;
    bool gpsPresent = false;
};
