#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class ITransport : public QObject
{
    Q_OBJECT

public:
    explicit ITransport(QObject *parent = nullptr) : QObject(parent) {}
    ~ITransport() override = default;

    virtual QString name() const = 0;
    virtual bool isOpen() const = 0;

public slots:
    virtual void close() = 0;
    virtual void writeBytes(const QByteArray &bytes) = 0;

signals:
    void opened();
    void closed();
    void bytesReceived(const QByteArray &bytes);
    void errorOccurred(const QString &message);
};
