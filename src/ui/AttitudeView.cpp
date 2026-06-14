#include "ui/AttitudeView.h"

#include <QPainter>
#include <QtMath>

#include <algorithm>

AttitudeView::AttitudeView(QWidget *parent) : QOpenGLWidget(parent)
{
    setMinimumSize(360, 260);
}

void AttitudeView::setAttitude(const AttitudeState &attitude)
{
    m_attitude = attitude;
    update();
}

void AttitudeView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(18, 22, 28));

    const QPointF center(width() * 0.5, height() * 0.5);
    const float roll = m_attitude.valid ? m_attitude.rollDeg : 0.0f;
    const float pitch = m_attitude.valid ? m_attitude.pitchDeg : 0.0f;
    const float pitchOffset = std::clamp(pitch * 3.0f, -height() * 0.35f, height() * 0.35f);

    painter.save();
    painter.translate(center);
    painter.rotate(-roll);
    painter.translate(0.0, pitchOffset);

    QRectF sky(-width(), -height() * 2.0, width() * 2.0, height() * 2.0);
    QRectF ground(-width(), 0.0, width() * 2.0, height() * 2.0);
    painter.fillRect(sky, QColor(52, 111, 170));
    painter.fillRect(ground, QColor(112, 76, 40));
    painter.setPen(QPen(QColor(240, 240, 240), 3));
    painter.drawLine(QPointF(-width(), 0.0), QPointF(width(), 0.0));

    painter.setPen(QPen(QColor(255, 255, 255, 160), 1));
    for (int step = -60; step <= 60; step += 15) {
        if (step == 0) {
            continue;
        }
        const qreal y = -step * 3.0;
        painter.drawLine(QPointF(-36, y), QPointF(36, y));
    }
    painter.restore();

    painter.save();
    painter.translate(center);
    painter.setPen(QPen(QColor(255, 220, 80), 4));
    painter.drawLine(QPointF(-72, 0), QPointF(-16, 0));
    painter.drawLine(QPointF(16, 0), QPointF(72, 0));
    painter.drawLine(QPointF(0, -12), QPointF(0, 18));

    painter.setPen(QPen(QColor(235, 235, 235), 2));
    painter.setBrush(QColor(35, 38, 45, 210));
    painter.drawRoundedRect(QRectF(-42, -26, 84, 52), 10, 10);
    painter.setBrush(QColor(245, 92, 72));
    QPolygonF nose;
    nose << QPointF(0, -58) << QPointF(-14, -24) << QPointF(14, -24);
    painter.drawPolygon(nose);
    painter.restore();

    painter.setPen(QColor(235, 235, 235));
    painter.drawText(14, 24, m_attitude.valid
        ? QStringLiteral("Roll %1°  Pitch %2°  Yaw %3°")
            .arg(m_attitude.rollDeg, 0, 'f', 1)
            .arg(m_attitude.pitchDeg, 0, 'f', 1)
            .arg(m_attitude.yawDeg, 0, 'f', 0)
        : QStringLiteral("Waiting for MSP_ATTITUDE"));

    painter.setPen(QColor(180, 190, 205));
    painter.drawText(14, height() - 16, QStringLiteral("Blue/brown horizon shows pitch and roll. Red arrow marks board nose."));
}
