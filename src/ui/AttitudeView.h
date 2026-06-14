#pragma once

#include "model/SensorData.h"

#include <QOpenGLWidget>

class AttitudeView final : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit AttitudeView(QWidget *parent = nullptr);

    void setAttitude(const AttitudeState &attitude);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    AttitudeState m_attitude;
};
