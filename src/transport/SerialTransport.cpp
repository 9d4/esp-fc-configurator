#include "transport/SerialTransport.h"

namespace {
constexpr quint16 EspressifVid = 0x303A;
constexpr quint16 LolinS3MiniPid = 0x8167;
}

SerialTransport::SerialTransport(QObject *parent) : ITransport(parent)
{
    connect(&m_port, &QSerialPort::readyRead, this, &SerialTransport::readAvailable);
    connect(&m_port, &QSerialPort::errorOccurred, this, &SerialTransport::handleError);
}

QList<SerialPortEntry> SerialTransport::availablePorts()
{
    QList<SerialPortEntry> result;
    const auto ports = QSerialPortInfo::availablePorts();
    result.reserve(ports.size());

    for (const QSerialPortInfo &info : ports) {
        SerialPortEntry entry;
        entry.portName = info.portName();
        entry.systemLocation = info.systemLocation();
        entry.description = info.description();
        entry.manufacturer = info.manufacturer();
        entry.hasVendorId = info.hasVendorIdentifier();
        entry.hasProductId = info.hasProductIdentifier();
        entry.vendorId = entry.hasVendorId ? info.vendorIdentifier() : 0;
        entry.productId = entry.hasProductId ? info.productIdentifier() : 0;
        entry.isEsp32S3NativeUsb = entry.hasVendorId && entry.hasProductId &&
            entry.vendorId == EspressifVid && entry.productId == LolinS3MiniPid;
        result.push_back(entry);
    }

    return result;
}

QString SerialTransport::name() const
{
    return m_port.portName();
}

bool SerialTransport::isOpen() const
{
    return m_port.isOpen();
}

bool SerialTransport::open(const QString &portName, qint32 baudRate)
{
    if (m_port.isOpen()) {
        m_port.close();
    }

    m_port.setPortName(portName);
    m_port.setBaudRate(baudRate);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        emit errorOccurred(m_port.errorString());
        return false;
    }

    emit opened();
    return true;
}

void SerialTransport::close()
{
    if (!m_port.isOpen()) {
        return;
    }
    m_port.close();
    emit closed();
}

void SerialTransport::writeBytes(const QByteArray &bytes)
{
    if (!m_port.isOpen()) {
        emit errorOccurred(QStringLiteral("Serial port is not open"));
        return;
    }
    m_port.write(bytes);
}

void SerialTransport::readAvailable()
{
    const QByteArray bytes = m_port.readAll();
    if (!bytes.isEmpty()) {
        emit bytesReceived(bytes);
    }
}

void SerialTransport::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    emit errorOccurred(m_port.errorString());
}
