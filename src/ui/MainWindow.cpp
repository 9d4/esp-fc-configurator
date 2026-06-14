#include "ui/MainWindow.h"

#include <QApplication>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("ESP-FC Configurator"));

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->addWidget(buildConnectionBar());

    auto *tabs = new QTabWidget(central);
    tabs->addTab(buildDashboardTab(), QStringLiteral("Dashboard"));
    tabs->addTab(buildPlaceholderTab(QStringLiteral("Setup Wizard"), {
        QStringLiteral("ESP32-S3 + BMI160 I2C preset"),
        QStringLiteral("SBUS/CRSF receiver preset"),
        QStringLiteral("PWM motor preset on GPIO39-42"),
        QStringLiteral("AUX1 arm switch preset"),
    }), QStringLiteral("Setup"));
    tabs->addTab(buildPlaceholderTab(QStringLiteral("Receiver"), {
        QStringLiteral("Read/write serial RX provider with MSP_RX_CONFIG / MSP_SET_RX_CONFIG"),
        QStringLiteral("Warn when more than one UART has RX_SERIAL"),
        QStringLiteral("Show channel activity and failsafe state"),
    }), QStringLiteral("Receiver"));
    tabs->addTab(buildPlaceholderTab(QStringLiteral("Motors"), {
        QStringLiteral("PWM/DSHOT protocol, rate, min/max throttle"),
        QStringLiteral("QUADX motor order diagram: RR, FR, RL, FL"),
        QStringLiteral("Safety-gated motor tests with props-off confirmation"),
    }), QStringLiteral("Motors"));
    tabs->addTab(buildPlaceholderTab(QStringLiteral("PID"), {
        QStringLiteral("Roll/pitch/yaw PID and feed-forward values"),
        QStringLiteral("Level and altitude-hold groups"),
        QStringLiteral("Import/export tuning profiles"),
    }), QStringLiteral("PID"));
    tabs->addTab(buildPlaceholderTab(QStringLiteral("Filters"), {
        QStringLiteral("Gyro LPF, LPF2, LPF3, static notches"),
        QStringLiteral("D-term filters and dynamic filter ranges"),
        QStringLiteral("Beginner/moderate/strong filter presets"),
    }), QStringLiteral("Filters"));
    tabs->addTab(buildPlaceholderTab(QStringLiteral("Rates"), {
        QStringLiteral("Roll/pitch/yaw rate, super-rate, expo, limits"),
        QStringLiteral("Throttle smoothing guidance through radio curves and mixer limit"),
        QStringLiteral("Beginner preset for reduced sensitivity"),
    }), QStringLiteral("Rates"));
    tabs->addTab(buildPlaceholderTab(QStringLiteral("Sensors / 3D"), {
        QStringLiteral("MSP_ATTITUDE 3D model"),
        QStringLiteral("MSP_RAW_IMU gravity vector"),
        QStringLiteral("gyro_align assistant and calibration commands"),
    }), QStringLiteral("Sensors"));
    tabs->addTab(buildCliTab(), QStringLiteral("CLI"));

    layout->addWidget(tabs, 1);
    setCentralWidget(central);

    m_cli.setTransport(nullptr);
    connect(&m_cli, &CliSession::textReceived, this, [this](const QString &text) {
        m_cliOutput->moveCursor(QTextCursor::End);
        m_cliOutput->insertPlainText(text);
        m_cliOutput->moveCursor(QTextCursor::End);
    });
    connect(&m_msp, &MspCodec::messageReceived, this, &MainWindow::handleMspMessage);
    connect(&m_msp, &MspCodec::parseError, this, &MainWindow::appendLog);

    refreshSerialPorts();
    setConnectedUi(false);
}

QWidget *MainWindow::buildConnectionBar()
{
    auto *box = new QGroupBox(QStringLiteral("Connection"), this);
    auto *layout = new QHBoxLayout(box);

    m_serialPorts = new QComboBox(box);
    auto *refresh = new QPushButton(QStringLiteral("Refresh"), box);
    m_serialConnect = new QPushButton(QStringLiteral("Connect Serial"), box);
    m_tcpHost = new QLineEdit(QStringLiteral("192.168.4.1"), box);
    m_tcpPort = new QLineEdit(QStringLiteral("1111"), box);
    m_tcpConnect = new QPushButton(QStringLiteral("Connect TCP"), box);
    m_disconnect = new QPushButton(QStringLiteral("Disconnect"), box);
    m_connectionLabel = new QLabel(QStringLiteral("Disconnected"), box);

    layout->addWidget(m_serialPorts, 2);
    layout->addWidget(refresh);
    layout->addWidget(m_serialConnect);
    layout->addSpacing(16);
    layout->addWidget(new QLabel(QStringLiteral("Host:"), box));
    layout->addWidget(m_tcpHost);
    layout->addWidget(new QLabel(QStringLiteral("Port:"), box));
    layout->addWidget(m_tcpPort);
    layout->addWidget(m_tcpConnect);
    layout->addWidget(m_disconnect);
    layout->addWidget(m_connectionLabel, 1);

    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
    connect(m_serialConnect, &QPushButton::clicked, this, &MainWindow::connectSerial);
    connect(m_tcpConnect, &QPushButton::clicked, this, &MainWindow::connectTcp);
    connect(m_disconnect, &QPushButton::clicked, this, &MainWindow::disconnectTransport);

    return box;
}

QWidget *MainWindow::buildDashboardTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *actions = new QHBoxLayout();
    auto *statusButton = new QPushButton(QStringLiteral("Request MSP Status"), page);
    auto *cliButton = new QPushButton(QStringLiteral("Enter CLI"), page);
    actions->addWidget(statusButton);
    actions->addWidget(cliButton);
    actions->addStretch();
    layout->addLayout(actions);

    auto *form = new QFormLayout();
    m_firmwareLabel = new QLabel(QStringLiteral("Unknown"), page);
    m_sensorLabel = new QLabel(QStringLiteral("Unknown"), page);
    m_statusLabel = new QLabel(QStringLiteral("No status yet"), page);
    m_armingLabel = new QLabel(QStringLiteral("Unknown"), page);
    form->addRow(QStringLiteral("Firmware"), m_firmwareLabel);
    form->addRow(QStringLiteral("Sensors"), m_sensorLabel);
    form->addRow(QStringLiteral("Loop / CPU"), m_statusLabel);
    form->addRow(QStringLiteral("Arming flags"), m_armingLabel);
    layout->addLayout(form);
    layout->addStretch();

    connect(statusButton, &QPushButton::clicked, this, &MainWindow::requestStatus);
    connect(cliButton, &QPushButton::clicked, this, &MainWindow::enterCli);
    return page;
}

QWidget *MainWindow::buildCliTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    m_cliOutput = new QPlainTextEdit(page);
    m_cliOutput->setReadOnly(true);
    m_cliInput = new QLineEdit(page);
    auto *buttons = new QHBoxLayout();
    auto *enter = new QPushButton(QStringLiteral("Enter CLI (#)"), page);
    auto *send = new QPushButton(QStringLiteral("Send"), page);
    auto *dump = new QPushButton(QStringLiteral("dump"), page);
    auto *status = new QPushButton(QStringLiteral("status"), page);
    auto *stats = new QPushButton(QStringLiteral("stats"), page);
    buttons->addWidget(enter);
    buttons->addWidget(dump);
    buttons->addWidget(status);
    buttons->addWidget(stats);
    buttons->addStretch();
    buttons->addWidget(send);
    layout->addWidget(m_cliOutput, 1);
    layout->addWidget(m_cliInput);
    layout->addLayout(buttons);

    connect(enter, &QPushButton::clicked, this, &MainWindow::enterCli);
    connect(send, &QPushButton::clicked, this, &MainWindow::sendCliCommand);
    connect(m_cliInput, &QLineEdit::returnPressed, this, &MainWindow::sendCliCommand);
    connect(dump, &QPushButton::clicked, this, [this]() { m_cli.sendCommand(QStringLiteral("dump")); });
    connect(status, &QPushButton::clicked, this, [this]() { m_cli.sendCommand(QStringLiteral("status")); });
    connect(stats, &QPushButton::clicked, this, [this]() { m_cli.sendCommand(QStringLiteral("stats")); });

    return page;
}

QWidget *MainWindow::buildPlaceholderTab(const QString &title, const QStringList &items)
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    auto *heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(title), page);
    layout->addWidget(heading);
    for (const QString &item : items) {
        layout->addWidget(new QLabel(QStringLiteral("- %1").arg(item), page));
    }
    layout->addStretch();
    return page;
}

void MainWindow::refreshSerialPorts()
{
    m_serialPorts->clear();
    for (const SerialPortEntry &entry : SerialTransport::availablePorts()) {
        QString label = entry.systemLocation;
        if (!entry.description.isEmpty()) {
            label += QStringLiteral(" - %1").arg(entry.description);
        }
        if (entry.hasVendorId && entry.hasProductId) {
            label += QStringLiteral(" [%1:%2]")
                .arg(entry.vendorId, 4, 16, QLatin1Char('0'))
                .arg(entry.productId, 4, 16, QLatin1Char('0'));
        }
        if (entry.isEsp32S3NativeUsb) {
            label += QStringLiteral(" ESP32-S3 native USB");
        }
        m_serialPorts->addItem(label, entry.systemLocation);
    }
}

void MainWindow::connectSerial()
{
    const QString port = m_serialPorts->currentData().toString();
    if (port.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("No port"), QStringLiteral("Select a serial port first."));
        return;
    }
    if (m_serial.open(port)) {
        setTransport(&m_serial);
        setConnectedUi(true);
    }
}

void MainWindow::connectTcp()
{
    bool ok = false;
    const quint16 port = m_tcpPort->text().toUShort(&ok);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Invalid port"), QStringLiteral("Enter a valid TCP port."));
        return;
    }
    setTransport(&m_tcp);
    m_tcp.connectToHost(m_tcpHost->text(), port);
}

void MainWindow::disconnectTransport()
{
    if (m_transport) {
        m_transport->close();
    }
    setConnectedUi(false);
}

void MainWindow::enterCli()
{
    m_cli.enter();
}

void MainWindow::sendCliCommand()
{
    const QString command = m_cliInput->text().trimmed();
    if (command.isEmpty()) {
        return;
    }
    m_cliOutput->appendPlainText(QStringLiteral("> %1").arg(command));
    m_cli.sendCommand(command);
    m_cliInput->clear();
}

void MainWindow::requestStatus()
{
    sendMsp(MspCodec::Status);
}

void MainWindow::handleMspMessage(const MspMessage &message)
{
    appendLog(QStringLiteral("Received %1 (%2 bytes)").arg(MspCodec::commandName(message.command)).arg(message.payload.size()));
    if (message.command == MspCodec::Status || message.command == MspCodec::StatusEx) {
        FcStatus status = MspCodec::parseStatus(message);
        m_state.setStatus(status);
        m_sensorLabel->setText(QStringLiteral("gyro=%1 accel=%2 baro=%3 mag=%4 gps=%5")
            .arg(status.gyroPresent).arg(status.accelPresent).arg(status.baroPresent).arg(status.magPresent).arg(status.gpsPresent));
        m_statusLabel->setText(QStringLiteral("loop=%1 us, cpu=%2, i2c errors=%3")
            .arg(status.loopTimeUs).arg(status.cpuLoad).arg(status.i2cErrors));
        m_armingLabel->setText(QStringLiteral("0x%1").arg(status.armingDisableFlags, 8, 16, QLatin1Char('0')));
    }
}

void MainWindow::appendLog(const QString &message)
{
    statusBar()->showMessage(message, 6000);
    if (m_cliOutput) {
        m_cliOutput->appendPlainText(QStringLiteral("[log] %1").arg(message));
    }
}

void MainWindow::setConnectedUi(bool connected)
{
    m_serialConnect->setEnabled(!connected);
    m_tcpConnect->setEnabled(!connected);
    m_disconnect->setEnabled(connected);
    m_connectionLabel->setText(connected && m_transport ? QStringLiteral("Connected: %1").arg(m_transport->name()) : QStringLiteral("Disconnected"));
}

void MainWindow::setTransport(ITransport *transport)
{
    if (m_transport) {
        disconnect(m_transport, nullptr, this, nullptr);
    }
    m_transport = transport;
    m_cli.setTransport(transport);

    if (!m_transport) {
        return;
    }
    connect(m_transport, &ITransport::bytesReceived, &m_msp, &MspCodec::consume);
    connect(m_transport, &ITransport::opened, this, [this]() { setConnectedUi(true); });
    connect(m_transport, &ITransport::closed, this, [this]() { setConnectedUi(false); });
    connect(m_transport, &ITransport::errorOccurred, this, &MainWindow::appendLog);
}

void MainWindow::sendMsp(quint16 command, const QByteArray &payload)
{
    if (!m_transport || !m_transport->isOpen()) {
        appendLog(QStringLiteral("No active transport"));
        return;
    }
    m_transport->writeBytes(MspCodec::encodeV1(command, payload));
}
