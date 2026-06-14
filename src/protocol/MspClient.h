#pragma once

#include "protocol/MspCodec.h"
#include "transport/ITransport.h"

#include <QQueue>
#include <QTimer>

class MspClient final : public QObject
{
    Q_OBJECT

public:
    explicit MspClient(QObject *parent = nullptr);

    void setTransport(ITransport *transport);
    void request(quint16 command, const QByteArray &payload = {}, int timeoutMs = 1000);
    void clearQueue();

signals:
    void messageReceived(const MspMessage &message);
    void requestFailed(quint16 command, const QString &reason);
    void parseError(const QString &message);

private:
    struct Request {
        quint16 command = 0;
        QByteArray payload;
        int timeoutMs = 1000;
    };

    void sendNext();
    void handleMessage(const MspMessage &message);
    void handleTimeout();

    ITransport *m_transport = nullptr;
    MspCodec m_codec;
    QQueue<Request> m_queue;
    Request m_current;
    bool m_busy = false;
    QTimer m_timeout;
    QMetaObject::Connection m_bytesConnection;
};
