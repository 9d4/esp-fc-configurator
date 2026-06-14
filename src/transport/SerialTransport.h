#pragma once

#include "transport/ITransport.h"

#include <QSerialPort>
#include <QSerialPortInfo>

struct SerialPortEntry
{
    QString portName;
    QString systemLocation;
    QString description;
    QString manufacturer;
    quint16 vendorId = 0;
    quint16 productId = 0;
    bool hasVendorId = false;
    bool hasProductId = false;
    bool isEsp32S3NativeUsb = false;
};

class SerialTransport final : public ITransport
{
    Q_OBJECT

public:
    explicit SerialTransport(QObject *parent = nullptr);

    static QList<SerialPortEntry> availablePorts();

    QString name() const override;
    bool isOpen() const override;

    bool open(const QString &portName, qint32 baudRate = 115200);

public slots:
    void close() override;
    void writeBytes(const QByteArray &bytes) override;

private slots:
    void readAvailable();
    void handleError(QSerialPort::SerialPortError error);

private:
    QSerialPort m_port;
};
