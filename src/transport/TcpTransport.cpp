#include "transport/TcpTransport.h"

TcpTransport::TcpTransport(QObject *parent) : ITransport(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &TcpTransport::opened);
    connect(&m_socket, &QTcpSocket::disconnected, this, &TcpTransport::closed);
    connect(&m_socket, &QTcpSocket::readyRead, this, &TcpTransport::readAvailable);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &TcpTransport::socketError);
}

QString TcpTransport::name() const
{
    return QStringLiteral("tcp://%1:%2").arg(m_host).arg(m_port);
}

bool TcpTransport::isOpen() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void TcpTransport::connectToHost(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.abort();
    }
    m_socket.connectToHost(host, port);
}

void TcpTransport::close()
{
    m_socket.disconnectFromHost();
}

void TcpTransport::writeBytes(const QByteArray &bytes)
{
    if (!isOpen()) {
        emit errorOccurred(QStringLiteral("TCP connection is not open"));
        return;
    }
    m_socket.write(bytes);
}

void TcpTransport::readAvailable()
{
    const QByteArray bytes = m_socket.readAll();
    if (!bytes.isEmpty()) {
        emit bytesReceived(bytes);
    }
}

void TcpTransport::socketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    emit errorOccurred(m_socket.errorString());
}
