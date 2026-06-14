#pragma once

#include "transport/ITransport.h"

#include <QTcpSocket>

class TcpTransport final : public ITransport
{
    Q_OBJECT

public:
    explicit TcpTransport(QObject *parent = nullptr);

    QString name() const override;
    bool isOpen() const override;

    void connectToHost(const QString &host, quint16 port);

public slots:
    void close() override;
    void writeBytes(const QByteArray &bytes) override;

private slots:
    void readAvailable();
    void socketError(QAbstractSocket::SocketError error);

private:
    QTcpSocket m_socket;
    QString m_host;
    quint16 m_port = 0;
};
