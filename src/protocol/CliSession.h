#pragma once

#include <QObject>
#include <QStringList>

class ITransport;

class CliSession final : public QObject
{
    Q_OBJECT

public:
    explicit CliSession(QObject *parent = nullptr);

    void setTransport(ITransport *transport);
    bool isActive() const { return m_active; }

public slots:
    void enter();
    void exit();
    void sendCommand(const QString &command);

signals:
    void textReceived(const QString &text);
    void activeChanged(bool active);

private slots:
    void onBytesReceived(const QByteArray &bytes);

private:
    ITransport *m_transport = nullptr;
    bool m_active = false;
};
