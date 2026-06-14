#pragma once

#include "model/AppState.h"
#include "protocol/CliSession.h"
#include "protocol/MspCodec.h"
#include "transport/SerialTransport.h"
#include "transport/TcpTransport.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshSerialPorts();
    void connectSerial();
    void connectTcp();
    void disconnectTransport();
    void enterCli();
    void sendCliCommand();
    void requestStatus();
    void handleMspMessage(const MspMessage &message);
    void appendLog(const QString &message);
    void setConnectedUi(bool connected);

private:
    QWidget *buildConnectionBar();
    QWidget *buildDashboardTab();
    QWidget *buildCliTab();
    QWidget *buildPlaceholderTab(const QString &title, const QStringList &items);
    void setTransport(ITransport *transport);
    void sendMsp(quint16 command, const QByteArray &payload = {});

    AppState m_state;
    SerialTransport m_serial;
    TcpTransport m_tcp;
    ITransport *m_transport = nullptr;
    MspCodec m_msp;
    CliSession m_cli;

    QComboBox *m_serialPorts = nullptr;
    QLineEdit *m_tcpHost = nullptr;
    QLineEdit *m_tcpPort = nullptr;
    QPushButton *m_serialConnect = nullptr;
    QPushButton *m_tcpConnect = nullptr;
    QPushButton *m_disconnect = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_firmwareLabel = nullptr;
    QLabel *m_sensorLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_armingLabel = nullptr;
    QPlainTextEdit *m_cliOutput = nullptr;
    QLineEdit *m_cliInput = nullptr;
};
