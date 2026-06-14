#include "protocol/CliSession.h"

#include "transport/ITransport.h"

CliSession::CliSession(QObject *parent) : QObject(parent) {}

void CliSession::setTransport(ITransport *transport)
{
    if (m_transport) {
        disconnect(m_transport, nullptr, this, nullptr);
    }
    m_transport = transport;
    if (m_transport) {
        connect(m_transport, &ITransport::bytesReceived, this, &CliSession::onBytesReceived);
    }
}

void CliSession::enter()
{
    if (!m_transport) {
        return;
    }
    m_transport->writeBytes(QByteArrayLiteral("#"));
}

void CliSession::exit()
{
    if (!m_transport) {
        return;
    }
    m_transport->writeBytes(QByteArray(1, char(4)));
    m_active = false;
    emit activeChanged(m_active);
}

void CliSession::sendCommand(const QString &command)
{
    if (!m_transport) {
        return;
    }
    QByteArray bytes = command.toUtf8();
    bytes.append('\n');
    m_transport->writeBytes(bytes);
}

void CliSession::onBytesReceived(const QByteArray &bytes)
{
    const QString text = QString::fromUtf8(bytes);
    if (!m_active && text.contains(QStringLiteral("Entering CLI Mode"))) {
        m_active = true;
        emit activeChanged(m_active);
    }
    emit textReceived(text);
}
