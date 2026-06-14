#pragma once

#include <QString>
#include <QStringList>

struct FirmwareInfo
{
    quint8 mspProtocol = 0;
    quint8 apiMajor = 0;
    quint8 apiMinor = 0;
    QString variant;
    QString version;
    QString board;
    QString target;
    QString buildDate;
    QString buildTime;
    QString gitRevision;

    QString summary() const
    {
        QStringList parts;
        if (!variant.isEmpty()) {
            parts << variant;
        }
        if (!version.isEmpty()) {
            parts << version;
        }
        if (apiMajor || apiMinor) {
            parts << QStringLiteral("api=%1.%2").arg(apiMajor).arg(apiMinor);
        }
        if (!target.isEmpty()) {
            parts << target;
        } else if (!board.isEmpty()) {
            parts << board;
        }
        if (!gitRevision.isEmpty()) {
            parts << gitRevision;
        }
        return parts.isEmpty() ? QStringLiteral("Unknown") : parts.join(QStringLiteral(" "));
    }
};
