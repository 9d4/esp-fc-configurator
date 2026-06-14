#include "protocol/MspClient.h"

MspClient::MspClient(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, &MspClient::handleTimeout);
    connect(&m_codec, &MspCodec::messageReceived, this, &MspClient::handleMessage);
    connect(&m_codec, &MspCodec::parseError, this, &MspClient::parseError);
}

void MspClient::setTransport(ITransport *transport)
{
    if (m_transport && m_bytesConnection) {
        disconnect(m_bytesConnection);
    }
    m_transport = transport;
    clearQueue();
    if (m_transport) {
        m_bytesConnection = connect(m_transport, &ITransport::bytesReceived, &m_codec, &MspCodec::consume);
    } else {
        m_bytesConnection = {};
    }
}

void MspClient::request(quint16 command, const QByteArray &payload, int timeoutMs)
{
    m_queue.enqueue(Request{command, payload, timeoutMs});
    sendNext();
}

void MspClient::clearQueue()
{
    m_queue.clear();
    m_busy = false;
    m_current = Request{};
    m_timeout.stop();
}

void MspClient::sendNext()
{
    if (m_busy || m_queue.isEmpty()) {
        return;
    }
    if (!m_transport || !m_transport->isOpen()) {
        const Request request = m_queue.dequeue();
        emit requestFailed(request.command, QStringLiteral("No active transport"));
        QMetaObject::invokeMethod(this, &MspClient::sendNext, Qt::QueuedConnection);
        return;
    }

    m_current = m_queue.dequeue();
    m_busy = true;
    m_transport->writeBytes(MspCodec::encodeV1(m_current.command, m_current.payload));
    m_timeout.start(m_current.timeoutMs);
}

void MspClient::handleMessage(const MspMessage &message)
{
    if (!m_busy || message.command != m_current.command) {
        emit messageReceived(message);
        return;
    }

    m_timeout.stop();
    m_busy = false;
    emit messageReceived(message);
    sendNext();
}

void MspClient::handleTimeout()
{
    const quint16 command = m_current.command;
    m_busy = false;
    emit requestFailed(command, QStringLiteral("MSP request timed out"));
    sendNext();
}
