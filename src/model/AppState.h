#pragma once

#include "model/FcStatus.h"

#include <QObject>

class AppState final : public QObject
{
    Q_OBJECT

public:
    explicit AppState(QObject *parent = nullptr) : QObject(parent) {}

    const FcStatus &status() const { return m_status; }

    void setStatus(const FcStatus &status)
    {
        m_status = status;
        emit statusChanged(m_status);
    }

signals:
    void statusChanged(const FcStatus &status);

private:
    FcStatus m_status;
};
