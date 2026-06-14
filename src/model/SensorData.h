#pragma once

#include <QtGlobal>

struct AttitudeState
{
    float rollDeg = 0.0f;
    float pitchDeg = 0.0f;
    float yawDeg = 0.0f;
    bool valid = false;
};

struct RawImuState
{
    qint16 acc[3] = {};
    qint16 gyro[3] = {};
    qint16 mag[3] = {};
    bool valid = false;
};
