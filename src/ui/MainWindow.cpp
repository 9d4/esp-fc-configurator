#include "ui/MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTextEdit>
#include <QStatusBar>
#include <QStringList>
#include <QVBoxLayout>

#include <array>

namespace {
constexpr std::array<const char *, 26> ArmingFlagNames = {
    "NO_GYRO", "FAILSAFE", "RX_FAILSAFE", "BAD_RX_RECOVERY", "BOXFAILSAFE", "RUNAWAY_TAKEOFF",
    "CRASH_DETECTED", "THROTTLE", "ANGLE", "BOOT_GRACE_TIME", "NOPREARM", "LOAD",
    "CALIBRATING", "CLI", "CMS_MENU", "BST", "MSP", "PARALYZE", "GPS", "RESC",
    "RPMFILTER", "REBOOT_REQUIRED", "DSHOT_BITBANG", "ACC_CALIBRATION", "MOTOR_PROTOCOL", "ARM_SWITCH",
};
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("ESP-FC Configurator"));

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->addWidget(buildConnectionBar());

    auto *tabs = new QTabWidget(central);
    tabs->addTab(buildDashboardTab(), QStringLiteral("Dashboard"));
    tabs->addTab(buildSetupTab(), QStringLiteral("Setup"));
    tabs->addTab(buildReceiverTab(), QStringLiteral("Receiver"));
    tabs->addTab(buildMotorsTab(), QStringLiteral("Motors"));
    tabs->addTab(buildPidTab(), QStringLiteral("PID"));
    tabs->addTab(buildFiltersTab(), QStringLiteral("Filters"));
    tabs->addTab(buildRatesTab(), QStringLiteral("Rates"));
    tabs->addTab(buildBlackboxTab(), QStringLiteral("Blackbox"));
    tabs->addTab(buildSensorsTab(), QStringLiteral("Sensors"));
    tabs->addTab(buildCliTab(), QStringLiteral("CLI"));

    layout->addWidget(tabs, 1);
    setCentralWidget(central);

    m_cli.setTransport(nullptr);
    connect(&m_cli, &CliSession::textReceived, this, [this](const QString &text) {
        m_cliOutput->moveCursor(QTextCursor::End);
        m_cliOutput->insertPlainText(text);
        m_cliOutput->moveCursor(QTextCursor::End);
        captureCliText(text);
    });
    connect(&m_cli, &CliSession::activeChanged, this, [this](bool active) {
        if (active) {
            flushPendingCliCommands();
        }
    });
    connect(&m_msp, &MspClient::messageReceived, this, &MainWindow::handleMspMessage);
    connect(&m_msp, &MspClient::parseError, this, &MainWindow::appendLog);
    connect(&m_msp, &MspClient::requestFailed, this, [this](quint16 command, const QString &error) {
        appendLog(QStringLiteral("%1 failed: %2").arg(MspCodec::commandName(command), error));
    });
    m_sensorPollTimer.setInterval(100);
    connect(&m_sensorPollTimer, &QTimer::timeout, this, &MainWindow::requestSensorFrame);

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
    auto *handshakeButton = new QPushButton(QStringLiteral("Handshake"), page);
    auto *loadDump = new QPushButton(QStringLiteral("Load Config Dump"), page);
    auto *cliButton = new QPushButton(QStringLiteral("Enter CLI"), page);
    actions->addWidget(statusButton);
    actions->addWidget(handshakeButton);
    actions->addWidget(loadDump);
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
    connect(handshakeButton, &QPushButton::clicked, this, &MainWindow::requestHandshake);
    connect(loadDump, &QPushButton::clicked, this, &MainWindow::loadConfigDump);
    connect(cliButton, &QPushButton::clicked, this, &MainWindow::enterCli);
    return page;
}

QWidget *MainWindow::buildSetupTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *configBox = new QGroupBox(QStringLiteral("Configuration Store"), page);
    auto *configLayout = new QVBoxLayout(configBox);
    m_configSummary = new QLabel(QStringLiteral("No config dump loaded"), configBox);
    auto *configButtons = new QHBoxLayout();
    auto *loadDump = new QPushButton(QStringLiteral("Load Dump"), configBox);
    m_applyConfig = new QPushButton(QStringLiteral("Write to FC && Reboot"), configBox);
    configButtons->addWidget(loadDump);
    configButtons->addWidget(m_applyConfig);
    configButtons->addStretch();
    configLayout->addWidget(m_configSummary);
    configLayout->addLayout(configButtons);

    auto *presetBox = new QGroupBox(QStringLiteral("Current ESP32-S3 Build Presets"), page);
    auto *presetLayout = new QVBoxLayout(presetBox);
    auto *presetButtons = new QHBoxLayout();
    auto *boardPreset = new QPushButton(QStringLiteral("ESP32-S3 Pins"), presetBox);
    auto *imuPreset = new QPushButton(QStringLiteral("GY-BMI160 I2C"), presetBox);
    auto *rxSbusPreset = new QPushButton(QStringLiteral("ELRS SBUS Serial2"), presetBox);
    auto *rxCrsfPreset = new QPushButton(QStringLiteral("ELRS CRSF Serial2"), presetBox);
    auto *pwmPreset = new QPushButton(QStringLiteral("PWM QuadX"), presetBox);
    auto *armPreset = new QPushButton(QStringLiteral("AUX1 High Arm"), presetBox);
    presetButtons->addWidget(boardPreset);
    presetButtons->addWidget(imuPreset);
    presetButtons->addWidget(rxSbusPreset);
    presetButtons->addWidget(rxCrsfPreset);
    presetButtons->addWidget(pwmPreset);
    presetButtons->addWidget(armPreset);
    presetButtons->addStretch();
    auto *presetNote = new QLabel(QStringLiteral(
        "Presets update the local config store first. Click Write to FC && Reboot to apply, save, and restart."), presetBox);
    presetNote->setWordWrap(true);
    presetLayout->addLayout(presetButtons);
    presetLayout->addWidget(presetNote);

    auto *validationBox = new QGroupBox(QStringLiteral("Setup Validation"), page);
    auto *validationLayout = new QVBoxLayout(validationBox);
    m_setupValidation = new QLabel(QStringLiteral("Load a dump to validate setup."), validationBox);
    m_setupValidation->setWordWrap(true);
    validationLayout->addWidget(m_setupValidation);

    layout->addWidget(configBox);
    layout->addWidget(presetBox);
    layout->addWidget(validationBox);
    layout->addStretch();

    connect(loadDump, &QPushButton::clicked, this, &MainWindow::loadConfigDump);
    connect(m_applyConfig, &QPushButton::clicked, this, &MainWindow::writeSaveReboot);
    connect(boardPreset, &QPushButton::clicked, this, [this]() {
        m_config.setInt(QStringLiteral("pin_i2c_scl"), 10);
        m_config.setInt(QStringLiteral("pin_i2c_sda"), 9);
        m_config.setInt(QStringLiteral("pin_output_0"), 39);
        m_config.setInt(QStringLiteral("pin_output_1"), 40);
        m_config.setInt(QStringLiteral("pin_output_2"), 41);
        m_config.setInt(QStringLiteral("pin_output_3"), 42);
        m_config.setInt(QStringLiteral("pin_serial_2_rx"), 17);
        m_config.setInt(QStringLiteral("pin_serial_2_tx"), 18);
        updateConfigUi();
    });
    connect(imuPreset, &QPushButton::clicked, this, [this]() {
        m_config.setValue(QStringLiteral("gyro_bus"), QStringLiteral("I2C"));
        m_config.setValue(QStringLiteral("gyro_dev"), QStringLiteral("BMI160"));
        m_config.setValue(QStringLiteral("accel_bus"), QStringLiteral("I2C"));
        m_config.setValue(QStringLiteral("accel_dev"), QStringLiteral("BMI160"));
        updateConfigUi();
    });
    connect(rxSbusPreset, &QPushButton::clicked, this, [this]() {
        m_config.setBool(QStringLiteral("feature_rx_serial"), true);
        m_config.setBool(QStringLiteral("feature_rx_ppm"), false);
        m_config.setBool(QStringLiteral("feature_rx_spi"), false);
        m_config.set(QStringLiteral("serial_2"), {QStringLiteral("64"), QStringLiteral("115200"), QStringLiteral("0")});
        m_config.setInt(QStringLiteral("pin_serial_2_rx"), 17);
        m_config.setInt(QStringLiteral("pin_serial_2_tx"), 18);
        m_rxProvider->setCurrentIndex(m_rxProvider->findData(RxConfig::Sbus));
        appendLog(QStringLiteral("SBUS provider selected. Use Receiver > Apply Provider to write serialrx_provider."));
        updateConfigUi();
    });
    connect(rxCrsfPreset, &QPushButton::clicked, this, [this]() {
        m_config.setBool(QStringLiteral("feature_rx_serial"), true);
        m_config.setBool(QStringLiteral("feature_rx_ppm"), false);
        m_config.setBool(QStringLiteral("feature_rx_spi"), false);
        m_config.set(QStringLiteral("serial_2"), {QStringLiteral("64"), QStringLiteral("115200"), QStringLiteral("0")});
        m_config.setInt(QStringLiteral("pin_serial_2_rx"), 17);
        m_config.setInt(QStringLiteral("pin_serial_2_tx"), 18);
        m_rxProvider->setCurrentIndex(m_rxProvider->findData(RxConfig::Crsf));
        appendLog(QStringLiteral("CRSF provider selected. Use Receiver > Apply Provider to write serialrx_provider."));
        updateConfigUi();
    });
    connect(pwmPreset, &QPushButton::clicked, this, [this]() {
        m_config.setValue(QStringLiteral("mixer_type"), QStringLiteral("QUADX"));
        m_config.setInt(QStringLiteral("mix_outputs"), 4);
        m_config.setValue(QStringLiteral("output_motor_protocol"), QStringLiteral("PWM"));
        m_config.setInt(QStringLiteral("output_motor_rate"), 50);
        m_config.setInt(QStringLiteral("output_min_command"), 1000);
        m_config.setInt(QStringLiteral("output_min_throttle"), 1070);
        m_config.setInt(QStringLiteral("output_max_throttle"), 2000);
        for (int i = 0; i < 4; ++i) {
            m_config.set(QStringLiteral("output_%1").arg(i), {QStringLiteral("M"), QStringLiteral("N"), QStringLiteral("1000"), QStringLiteral("1500"), QStringLiteral("2000")});
        }
        updateConfigUi();
    });
    connect(armPreset, &QPushButton::clicked, this, [this]() {
        m_config.set(QStringLiteral("mode_0"), {QStringLiteral("0"), QStringLiteral("4"), QStringLiteral("1700"), QStringLiteral("2100"), QStringLiteral("0"), QStringLiteral("0")});
        updateConfigUi();
    });

    return page;
}

QWidget *MainWindow::buildReceiverTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *providerBox = new QGroupBox(QStringLiteral("Serial Receiver Provider"), page);
    auto *form = new QFormLayout(providerBox);
    m_rxProvider = new QComboBox(providerBox);
    m_rxProvider->addItem(QStringLiteral("SBUS"), RxConfig::Sbus);
    m_rxProvider->addItem(QStringLiteral("IBUS"), RxConfig::Ibus);
    m_rxProvider->addItem(QStringLiteral("CRSF / ELRS"), RxConfig::Crsf);
    m_rxSummary = new QLabel(QStringLiteral("Not loaded"), providerBox);
    form->addRow(QStringLiteral("Provider"), m_rxProvider);
    form->addRow(QStringLiteral("Current config"), m_rxSummary);

    auto *buttons = new QHBoxLayout();
    auto *read = new QPushButton(QStringLiteral("Read RX Config"), page);
    m_rxApply = new QPushButton(QStringLiteral("Apply Provider"), page);
    auto *write = new QPushButton(QStringLiteral("Write to FC && Reboot"), page);
    buttons->addWidget(read);
    buttons->addWidget(m_rxApply);
    buttons->addWidget(write);
    buttons->addStretch();

    auto *note = new QLabel(QStringLiteral(
        "Provider changes use MSP_RX_CONFIG/MSP_SET_RX_CONFIG because ESP-FC CLI does not expose serialrx_provider. "
        "Click Write to FC && Reboot after changing SBUS/IBUS/CRSF."), page);
    note->setWordWrap(true);

    layout->addWidget(providerBox);
    layout->addLayout(buttons);
    layout->addWidget(note);
    layout->addStretch();

    connect(read, &QPushButton::clicked, this, &MainWindow::requestRxConfig);
    connect(m_rxApply, &QPushButton::clicked, this, &MainWindow::applyRxProvider);
    connect(write, &QPushButton::clicked, this, &MainWindow::writeSaveReboot);
    return page;
}

QWidget *MainWindow::buildMotorsTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *configBox = new QGroupBox(QStringLiteral("Motor Output Configuration"), page);
    auto *form = new QFormLayout(configBox);
    m_motorProtocol = new QComboBox(configBox);
    m_motorProtocol->addItems({QStringLiteral("DISABLED"), QStringLiteral("PWM"), QStringLiteral("ONESHOT125"), QStringLiteral("ONESHOT42"), QStringLiteral("DSHOT150"), QStringLiteral("DSHOT300"), QStringLiteral("DSHOT600")});
    m_motorRate = new QSpinBox(configBox);
    m_motorRate->setRange(0, 8000);
    m_motorRate->setSuffix(QStringLiteral(" Hz"));
    m_minCommand = new QSpinBox(configBox);
    m_minCommand->setRange(750, 2250);
    m_minThrottle = new QSpinBox(configBox);
    m_minThrottle->setRange(750, 2250);
    m_maxThrottle = new QSpinBox(configBox);
    m_maxThrottle->setRange(750, 2250);
    m_outputLimit = new QSpinBox(configBox);
    m_outputLimit->setRange(1, 100);
    m_outputLimit->setSuffix(QStringLiteral(" %"));
    m_throttleLimitType = new QComboBox(configBox);
    m_throttleLimitType->addItems({QStringLiteral("NONE"), QStringLiteral("SCALE"), QStringLiteral("CLIP")});
    m_throttleLimitPercent = new QSpinBox(configBox);
    m_throttleLimitPercent->setRange(1, 100);
    m_throttleLimitPercent->setSuffix(QStringLiteral(" %"));

    form->addRow(QStringLiteral("Protocol"), m_motorProtocol);
    form->addRow(QStringLiteral("PWM/analog rate"), m_motorRate);
    form->addRow(QStringLiteral("Min command"), m_minCommand);
    form->addRow(QStringLiteral("Min throttle"), m_minThrottle);
    form->addRow(QStringLiteral("Max throttle"), m_maxThrottle);
    form->addRow(QStringLiteral("Output limit"), m_outputLimit);
    form->addRow(QStringLiteral("Throttle limit type"), m_throttleLimitType);
    form->addRow(QStringLiteral("Throttle limit percent"), m_throttleLimitPercent);

    auto *pinsBox = new QGroupBox(QStringLiteral("ESP32-S3 Motor Pins"), page);
    auto *pins = new QGridLayout(pinsBox);
    const QStringList labels = {
        QStringLiteral("M1 / output_0 / Rear Right"),
        QStringLiteral("M2 / output_1 / Front Right"),
        QStringLiteral("M3 / output_2 / Rear Left"),
        QStringLiteral("M4 / output_3 / Front Left"),
    };
    for (int i = 0; i < 4; ++i) {
        m_motorPins[i] = new QSpinBox(pinsBox);
        m_motorPins[i]->setRange(-1, 48);
        pins->addWidget(new QLabel(labels.at(i), pinsBox), i, 0);
        pins->addWidget(m_motorPins[i], i, 1);
    }

    auto *diagramBox = new QGroupBox(QStringLiteral("QuadX Motor Order"), page);
    auto *diagramLayout = new QVBoxLayout(diagramBox);
    auto *diagram = new QLabel(QStringLiteral(
        "<pre>          FRONT\n"
        "   M4 GPIO42       M2 GPIO40\n"
        "   Front Left      Front Right\n\n"
        "   M3 GPIO41       M1 GPIO39\n"
        "   Rear Left       Rear Right\n"
        "          REAR</pre>"), diagramBox);
    diagramLayout->addWidget(diagram);

    auto *safety = new QLabel(QStringLiteral(
        "Motor tests are intentionally not implemented yet. Keep props off and use this page for configuration and validation first."), page);
    safety->setWordWrap(true);

    m_motorValidation = new QLabel(QStringLiteral("Load a config dump to validate motors."), page);
    m_motorValidation->setWordWrap(true);

    auto *buttons = new QHBoxLayout();
    auto *load = new QPushButton(QStringLiteral("Load Dump"), page);
    auto *preset = new QPushButton(QStringLiteral("PWM QuadX Preset"), page);
    auto *apply = new QPushButton(QStringLiteral("Stage Motor Fields"), page);
    auto *write = new QPushButton(QStringLiteral("Write to FC && Reboot"), page);
    buttons->addWidget(load);
    buttons->addWidget(preset);
    buttons->addWidget(apply);
    buttons->addWidget(write);
    buttons->addStretch();

    layout->addWidget(configBox);
    layout->addWidget(pinsBox);
    layout->addWidget(diagramBox);
    layout->addWidget(safety);
    layout->addWidget(m_motorValidation);
    layout->addLayout(buttons);
    layout->addStretch();

    connect(load, &QPushButton::clicked, this, &MainWindow::loadConfigDump);
    connect(preset, &QPushButton::clicked, this, &MainWindow::applyMotorPreset);
    connect(apply, &QPushButton::clicked, this, &MainWindow::applyMotorsFromUi);
    connect(write, &QPushButton::clicked, this, &MainWindow::writeSaveReboot);
    return page;
}

QWidget *MainWindow::buildPidTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *pidBox = new QGroupBox(QStringLiteral("PID Values"), page);
    auto *pidGrid = new QGridLayout(pidBox);
    const QStringList axes = {QStringLiteral("Roll"), QStringLiteral("Pitch"), QStringLiteral("Yaw"), QStringLiteral("Level")};
    const QStringList terms = {QStringLiteral("P"), QStringLiteral("I"), QStringLiteral("D"), QStringLiteral("F")};
    for (int col = 0; col < terms.size(); ++col) {
        pidGrid->addWidget(new QLabel(terms.at(col), pidBox), 0, col + 1);
    }
    for (int row = 0; row < axes.size(); ++row) {
        pidGrid->addWidget(new QLabel(axes.at(row), pidBox), row + 1, 0);
        for (int col = 0; col < terms.size(); ++col) {
            m_pid[row][col] = new QSpinBox(pidBox);
            m_pid[row][col]->setRange(0, col == 3 ? 500 : 300);
            pidGrid->addWidget(m_pid[row][col], row + 1, col + 1);
        }
    }

    auto *filterBox = new QGroupBox(QStringLiteral("D-term / I-term"), page);
    auto *filterForm = new QFormLayout(filterBox);
    const QStringList filterTypes = {
        QStringLiteral("PT1"), QStringLiteral("BIQUAD"), QStringLiteral("PT2"), QStringLiteral("PT3"),
        QStringLiteral("NOTCH"), QStringLiteral("BPF"), QStringLiteral("FO"), QStringLiteral("NONE"),
    };
    m_dtermLpfType = new QComboBox(filterBox);
    m_dtermLpfType->addItems(filterTypes);
    m_dtermLpfFreq = new QSpinBox(filterBox);
    m_dtermLpfFreq->setRange(0, 1000);
    m_dtermLpfFreq->setSuffix(QStringLiteral(" Hz"));
    m_dtermLpf2Type = new QComboBox(filterBox);
    m_dtermLpf2Type->addItems(filterTypes);
    m_dtermLpf2Freq = new QSpinBox(filterBox);
    m_dtermLpf2Freq->setRange(0, 1000);
    m_dtermLpf2Freq->setSuffix(QStringLiteral(" Hz"));
    m_dtermNotchFreq = new QSpinBox(filterBox);
    m_dtermNotchFreq->setRange(0, 1000);
    m_dtermNotchFreq->setSuffix(QStringLiteral(" Hz"));
    m_dtermNotchCutoff = new QSpinBox(filterBox);
    m_dtermNotchCutoff->setRange(0, 1000);
    m_dtermNotchCutoff->setSuffix(QStringLiteral(" Hz"));
    m_dtermDynMin = new QSpinBox(filterBox);
    m_dtermDynMin->setRange(0, 1000);
    m_dtermDynMin->setSuffix(QStringLiteral(" Hz"));
    m_dtermDynMax = new QSpinBox(filterBox);
    m_dtermDynMax->setRange(0, 1000);
    m_dtermDynMax->setSuffix(QStringLiteral(" Hz"));
    m_itermLimit = new QSpinBox(filterBox);
    m_itermLimit->setRange(0, 1000);
    m_itermZero = new QComboBox(filterBox);
    m_itermZero->addItems({QStringLiteral("0"), QStringLiteral("1")});
    m_itermRelax = new QComboBox(filterBox);
    m_itermRelax->addItems({QStringLiteral("OFF"), QStringLiteral("RP"), QStringLiteral("RPY"), QStringLiteral("RP_INC"), QStringLiteral("RPY_INC")});
    m_itermRelaxCutoff = new QSpinBox(filterBox);
    m_itermRelaxCutoff->setRange(0, 1000);
    m_itermRelaxCutoff->setSuffix(QStringLiteral(" Hz"));

    filterForm->addRow(QStringLiteral("D-term LPF type"), m_dtermLpfType);
    filterForm->addRow(QStringLiteral("D-term LPF freq"), m_dtermLpfFreq);
    filterForm->addRow(QStringLiteral("D-term LPF2 type"), m_dtermLpf2Type);
    filterForm->addRow(QStringLiteral("D-term LPF2 freq"), m_dtermLpf2Freq);
    filterForm->addRow(QStringLiteral("D-term notch freq"), m_dtermNotchFreq);
    filterForm->addRow(QStringLiteral("D-term notch cutoff"), m_dtermNotchCutoff);
    filterForm->addRow(QStringLiteral("D-term dyn min"), m_dtermDynMin);
    filterForm->addRow(QStringLiteral("D-term dyn max"), m_dtermDynMax);
    filterForm->addRow(QStringLiteral("I-term limit"), m_itermLimit);
    filterForm->addRow(QStringLiteral("I-term zero"), m_itermZero);
    filterForm->addRow(QStringLiteral("I-term relax"), m_itermRelax);
    filterForm->addRow(QStringLiteral("I-term relax cutoff"), m_itermRelaxCutoff);

    auto *tpaBox = new QGroupBox(QStringLiteral("TPA"), page);
    auto *tpaForm = new QFormLayout(tpaBox);
    m_tpaScale = new QSpinBox(tpaBox);
    m_tpaScale->setRange(0, 100);
    m_tpaScale->setSuffix(QStringLiteral(" %"));
    m_tpaBreakpoint = new QSpinBox(tpaBox);
    m_tpaBreakpoint->setRange(1000, 2000);
    tpaForm->addRow(QStringLiteral("TPA scale"), m_tpaScale);
    tpaForm->addRow(QStringLiteral("TPA breakpoint"), m_tpaBreakpoint);

    m_pidValidation = new QLabel(QStringLiteral("Load a config dump to validate PID settings."), page);
    m_pidValidation->setWordWrap(true);

    auto *buttons = new QHBoxLayout();
    auto *load = new QPushButton(QStringLiteral("Load Dump"), page);
    auto *beginner = new QPushButton(QStringLiteral("Beginner Soft Preset"), page);
    auto *stage = new QPushButton(QStringLiteral("Stage PID Fields"), page);
    auto *write = new QPushButton(QStringLiteral("Write to FC && Reboot"), page);
    buttons->addWidget(load);
    buttons->addWidget(beginner);
    buttons->addWidget(stage);
    buttons->addWidget(write);
    buttons->addStretch();

    auto *note = new QLabel(QStringLiteral(
        "Beginner Soft stages rates/expo/limits instead of changing PID gains, because control feel is usually safer to soften there first."), page);
    note->setWordWrap(true);

    layout->addWidget(pidBox);
    layout->addWidget(filterBox);
    layout->addWidget(tpaBox);
    layout->addWidget(note);
    layout->addWidget(m_pidValidation);
    layout->addLayout(buttons);
    layout->addStretch();

    connect(load, &QPushButton::clicked, this, &MainWindow::loadConfigDump);
    connect(beginner, &QPushButton::clicked, this, &MainWindow::applyPidBeginnerPreset);
    connect(stage, &QPushButton::clicked, this, &MainWindow::stagePidFields);
    connect(write, &QPushButton::clicked, this, &MainWindow::writeSaveReboot);
    return page;
}

QWidget *MainWindow::buildFiltersTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    const QStringList filterTypes = {
        QStringLiteral("PT1"), QStringLiteral("BIQUAD"), QStringLiteral("PT2"), QStringLiteral("PT3"),
        QStringLiteral("NOTCH"), QStringLiteral("NOTCH_DF1"), QStringLiteral("BPF"), QStringLiteral("FO"),
        QStringLiteral("FIR2"), QStringLiteral("MEDIAN3"), QStringLiteral("NONE"),
    };
    auto makeFilterCombo = [&filterTypes](QWidget *parent) {
        auto *combo = new QComboBox(parent);
        combo->addItems(filterTypes);
        return combo;
    };
    auto makeHzSpin = [](QWidget *parent) {
        auto *spin = new QSpinBox(parent);
        spin->setRange(0, 1000);
        spin->setSuffix(QStringLiteral(" Hz"));
        return spin;
    };

    auto *gyroBox = new QGroupBox(QStringLiteral("Gyro Low-Pass Filters"), page);
    auto *gyroForm = new QFormLayout(gyroBox);
    m_filterGyroLpfType = makeFilterCombo(gyroBox);
    m_filterGyroLpfFreq = makeHzSpin(gyroBox);
    m_filterGyroLpf2Type = makeFilterCombo(gyroBox);
    m_filterGyroLpf2Freq = makeHzSpin(gyroBox);
    m_filterGyroLpf3Type = makeFilterCombo(gyroBox);
    m_filterGyroLpf3Freq = makeHzSpin(gyroBox);
    gyroForm->addRow(QStringLiteral("Gyro LPF type"), m_filterGyroLpfType);
    gyroForm->addRow(QStringLiteral("Gyro LPF freq"), m_filterGyroLpfFreq);
    gyroForm->addRow(QStringLiteral("Gyro LPF2 type"), m_filterGyroLpf2Type);
    gyroForm->addRow(QStringLiteral("Gyro LPF2 freq"), m_filterGyroLpf2Freq);
    gyroForm->addRow(QStringLiteral("Gyro LPF3 type"), m_filterGyroLpf3Type);
    gyroForm->addRow(QStringLiteral("Gyro LPF3 freq"), m_filterGyroLpf3Freq);

    auto *notchBox = new QGroupBox(QStringLiteral("Gyro Static Notches"), page);
    auto *notchForm = new QFormLayout(notchBox);
    m_filterGyroNotch1Freq = makeHzSpin(notchBox);
    m_filterGyroNotch1Cutoff = makeHzSpin(notchBox);
    m_filterGyroNotch2Freq = makeHzSpin(notchBox);
    m_filterGyroNotch2Cutoff = makeHzSpin(notchBox);
    notchForm->addRow(QStringLiteral("Notch 1 freq"), m_filterGyroNotch1Freq);
    notchForm->addRow(QStringLiteral("Notch 1 cutoff"), m_filterGyroNotch1Cutoff);
    notchForm->addRow(QStringLiteral("Notch 2 freq"), m_filterGyroNotch2Freq);
    notchForm->addRow(QStringLiteral("Notch 2 cutoff"), m_filterGyroNotch2Cutoff);

    auto *dynamicBox = new QGroupBox(QStringLiteral("Dynamic Gyro Filtering"), page);
    auto *dynamicForm = new QFormLayout(dynamicBox);
    m_filterDynNotch = new QCheckBox(QStringLiteral("Enable dynamic notch feature"), dynamicBox);
    m_filterGyroDynLpfMin = makeHzSpin(dynamicBox);
    m_filterGyroDynLpfMax = makeHzSpin(dynamicBox);
    m_filterGyroDynNotchQ = new QSpinBox(dynamicBox);
    m_filterGyroDynNotchQ->setRange(0, 1000);
    m_filterGyroDynNotchCount = new QSpinBox(dynamicBox);
    m_filterGyroDynNotchCount->setRange(0, 8);
    m_filterGyroDynNotchMin = makeHzSpin(dynamicBox);
    m_filterGyroDynNotchMax = makeHzSpin(dynamicBox);
    dynamicForm->addRow(m_filterDynNotch);
    dynamicForm->addRow(QStringLiteral("Dynamic LPF min"), m_filterGyroDynLpfMin);
    dynamicForm->addRow(QStringLiteral("Dynamic LPF max"), m_filterGyroDynLpfMax);
    dynamicForm->addRow(QStringLiteral("Dynamic notch Q"), m_filterGyroDynNotchQ);
    dynamicForm->addRow(QStringLiteral("Dynamic notch count"), m_filterGyroDynNotchCount);
    dynamicForm->addRow(QStringLiteral("Dynamic notch min"), m_filterGyroDynNotchMin);
    dynamicForm->addRow(QStringLiteral("Dynamic notch max"), m_filterGyroDynNotchMax);

    auto *dtermBox = new QGroupBox(QStringLiteral("D-Term Filters"), page);
    auto *dtermForm = new QFormLayout(dtermBox);
    m_filterDtermLpfType = makeFilterCombo(dtermBox);
    m_filterDtermLpfFreq = makeHzSpin(dtermBox);
    m_filterDtermLpf2Type = makeFilterCombo(dtermBox);
    m_filterDtermLpf2Freq = makeHzSpin(dtermBox);
    m_filterDtermNotchFreq = makeHzSpin(dtermBox);
    m_filterDtermNotchCutoff = makeHzSpin(dtermBox);
    m_filterDtermDynLpfMin = makeHzSpin(dtermBox);
    m_filterDtermDynLpfMax = makeHzSpin(dtermBox);
    dtermForm->addRow(QStringLiteral("D-term LPF type"), m_filterDtermLpfType);
    dtermForm->addRow(QStringLiteral("D-term LPF freq"), m_filterDtermLpfFreq);
    dtermForm->addRow(QStringLiteral("D-term LPF2 type"), m_filterDtermLpf2Type);
    dtermForm->addRow(QStringLiteral("D-term LPF2 freq"), m_filterDtermLpf2Freq);
    dtermForm->addRow(QStringLiteral("D-term notch freq"), m_filterDtermNotchFreq);
    dtermForm->addRow(QStringLiteral("D-term notch cutoff"), m_filterDtermNotchCutoff);
    dtermForm->addRow(QStringLiteral("D-term dyn min"), m_filterDtermDynLpfMin);
    dtermForm->addRow(QStringLiteral("D-term dyn max"), m_filterDtermDynLpfMax);

    auto *columns = new QHBoxLayout();
    auto *left = new QVBoxLayout();
    auto *right = new QVBoxLayout();
    left->addWidget(gyroBox);
    left->addWidget(notchBox);
    right->addWidget(dynamicBox);
    right->addWidget(dtermBox);
    columns->addLayout(left, 1);
    columns->addLayout(right, 1);

    m_filterValidation = new QLabel(QStringLiteral("Load a config dump to validate filter settings."), page);
    m_filterValidation->setWordWrap(true);

    auto *buttons = new QHBoxLayout();
    auto *load = new QPushButton(QStringLiteral("Load Dump"), page);
    auto *defaults = new QPushButton(QStringLiteral("Default"), page);
    auto *more = new QPushButton(QStringLiteral("More Filtering"), page);
    auto *strong = new QPushButton(QStringLiteral("Strong Filtering"), page);
    auto *lowLatency = new QPushButton(QStringLiteral("Low Latency"), page);
    auto *stage = new QPushButton(QStringLiteral("Stage Filter Fields"), page);
    auto *write = new QPushButton(QStringLiteral("Write to FC && Reboot"), page);
    buttons->addWidget(load);
    buttons->addWidget(defaults);
    buttons->addWidget(more);
    buttons->addWidget(strong);
    buttons->addWidget(lowLatency);
    buttons->addWidget(stage);
    buttons->addWidget(write);
    buttons->addStretch();

    auto *note = new QLabel(QStringLiteral(
        "Lower cutoff frequencies add more filtering and delay. Use stronger presets only when motors are noisy or hot; use low latency only on a clean build."), page);
    note->setWordWrap(true);

    layout->addLayout(columns);
    layout->addWidget(note);
    layout->addWidget(m_filterValidation);
    layout->addLayout(buttons);
    layout->addStretch();

    connect(load, &QPushButton::clicked, this, &MainWindow::loadConfigDump);
    connect(defaults, &QPushButton::clicked, this, &MainWindow::applyFilterDefaultPreset);
    connect(more, &QPushButton::clicked, this, &MainWindow::applyFilterMorePreset);
    connect(strong, &QPushButton::clicked, this, &MainWindow::applyFilterStrongPreset);
    connect(lowLatency, &QPushButton::clicked, this, &MainWindow::applyFilterLowLatencyPreset);
    connect(stage, &QPushButton::clicked, this, &MainWindow::stageFilterFields);
    connect(write, &QPushButton::clicked, this, &MainWindow::writeSaveReboot);
    return page;
}

QWidget *MainWindow::buildRatesTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *typeBox = new QGroupBox(QStringLiteral("Rate Type"), page);
    auto *typeForm = new QFormLayout(typeBox);
    m_rateType = new QComboBox(typeBox);
    m_rateType->addItems({
        QStringLiteral("BETAFLIGHT"), QStringLiteral("RACEFLIGHT"), QStringLiteral("KISS"),
        QStringLiteral("ACTUAL"), QStringLiteral("QUICK"),
    });
    typeForm->addRow(QStringLiteral("input_rate_type"), m_rateType);

    auto *axisBox = new QGroupBox(QStringLiteral("Axis Rates"), page);
    auto *axisGrid = new QGridLayout(axisBox);
    const QStringList axes = {QStringLiteral("Roll"), QStringLiteral("Pitch"), QStringLiteral("Yaw")};
    const QStringList columns = {QStringLiteral("Rate"), QStringLiteral("Super Rate"), QStringLiteral("Expo"), QStringLiteral("Limit")};
    for (int column = 0; column < columns.size(); ++column) {
        axisGrid->addWidget(new QLabel(columns[column], axisBox), 0, column + 1);
    }
    for (int row = 0; row < axes.size(); ++row) {
        axisGrid->addWidget(new QLabel(axes[row], axisBox), row + 1, 0);
        for (int column = 0; column < columns.size(); ++column) {
            auto *spin = new QSpinBox(axisBox);
            if (column == 0 || column == 1) {
                spin->setRange(0, 200);
            } else if (column == 2) {
                spin->setRange(0, 100);
            } else {
                spin->setRange(0, 2000);
            }
            m_rate[row][column] = spin;
            axisGrid->addWidget(spin, row + 1, column + 1);
        }
    }

    const QStringList filterTypes = {
        QStringLiteral("PT1"), QStringLiteral("BIQUAD"), QStringLiteral("PT2"), QStringLiteral("PT3"),
        QStringLiteral("FO"), QStringLiteral("NONE"),
    };
    auto makeFilterCombo = [&filterTypes](QWidget *parent) {
        auto *combo = new QComboBox(parent);
        combo->addItems(filterTypes);
        return combo;
    };
    auto makeHzSpin = [](QWidget *parent) {
        auto *spin = new QSpinBox(parent);
        spin->setRange(0, 500);
        spin->setSuffix(QStringLiteral(" Hz"));
        return spin;
    };

    auto *inputBox = new QGroupBox(QStringLiteral("Input Feel"), page);
    auto *inputForm = new QFormLayout(inputBox);
    m_inputDeadband = new QSpinBox(inputBox);
    m_inputDeadband->setRange(0, 50);
    m_inputLpfType = makeFilterCombo(inputBox);
    m_inputLpfFreq = makeHzSpin(inputBox);
    m_inputLpfFactor = new QSpinBox(inputBox);
    m_inputLpfFactor->setRange(0, 100);
    m_inputFfLpfType = makeFilterCombo(inputBox);
    m_inputFfLpfFreq = makeHzSpin(inputBox);
    inputForm->addRow(QStringLiteral("input_deadband"), m_inputDeadband);
    inputForm->addRow(QStringLiteral("input_lpf_type"), m_inputLpfType);
    inputForm->addRow(QStringLiteral("input_lpf_freq"), m_inputLpfFreq);
    inputForm->addRow(QStringLiteral("input_lpf_factor"), m_inputLpfFactor);
    inputForm->addRow(QStringLiteral("input_ff_lpf_type"), m_inputFfLpfType);
    inputForm->addRow(QStringLiteral("input_ff_lpf_freq"), m_inputFfLpfFreq);

    auto *throttleBox = new QGroupBox(QStringLiteral("Throttle Smoothing"), page);
    auto *throttleForm = new QFormLayout(throttleBox);
    m_rateThrottleLimitType = new QComboBox(throttleBox);
    m_rateThrottleLimitType->addItems({QStringLiteral("NONE"), QStringLiteral("SCALE"), QStringLiteral("CLIP")});
    m_rateThrottleLimitPercent = new QSpinBox(throttleBox);
    m_rateThrottleLimitPercent->setRange(1, 100);
    m_rateThrottleLimitPercent->setSuffix(QStringLiteral(" %"));
    m_rateMinThrottle = new QSpinBox(throttleBox);
    m_rateMinThrottle->setRange(900, 1200);
    throttleForm->addRow(QStringLiteral("mixer_throttle_limit_type"), m_rateThrottleLimitType);
    throttleForm->addRow(QStringLiteral("mixer_throttle_limit_percent"), m_rateThrottleLimitPercent);
    throttleForm->addRow(QStringLiteral("output_min_throttle"), m_rateMinThrottle);

    auto *columnsLayout = new QHBoxLayout();
    auto *left = new QVBoxLayout();
    auto *right = new QVBoxLayout();
    left->addWidget(typeBox);
    left->addWidget(axisBox);
    right->addWidget(inputBox);
    right->addWidget(throttleBox);
    columnsLayout->addLayout(left, 2);
    columnsLayout->addLayout(right, 1);

    m_rateValidation = new QLabel(QStringLiteral("Load a config dump to validate rate settings."), page);
    m_rateValidation->setWordWrap(true);

    auto *buttons = new QHBoxLayout();
    auto *load = new QPushButton(QStringLiteral("Load Dump"), page);
    auto *beginner = new QPushButton(QStringLiteral("Beginner Soft"), page);
    auto *noFlips = new QPushButton(QStringLiteral("No Flips"), page);
    auto *acro = new QPushButton(QStringLiteral("Acro Normal"), page);
    auto *smoothThrottle = new QPushButton(QStringLiteral("Smooth Throttle"), page);
    auto *stage = new QPushButton(QStringLiteral("Stage Rate Fields"), page);
    auto *write = new QPushButton(QStringLiteral("Write to FC && Reboot"), page);
    buttons->addWidget(load);
    buttons->addWidget(beginner);
    buttons->addWidget(noFlips);
    buttons->addWidget(acro);
    buttons->addWidget(smoothThrottle);
    buttons->addWidget(stage);
    buttons->addWidget(write);
    buttons->addStretch();

    auto *note = new QLabel(QStringLiteral(
        "ESP-FC does not expose throttle expo. Use SCALE throttle limiting here for coarse smoothing; use an EdgeTX throttle curve for detailed shaping."), page);
    note->setWordWrap(true);

    layout->addLayout(columnsLayout);
    layout->addWidget(note);
    layout->addWidget(m_rateValidation);
    layout->addLayout(buttons);
    layout->addStretch();

    connect(load, &QPushButton::clicked, this, &MainWindow::loadConfigDump);
    connect(beginner, &QPushButton::clicked, this, &MainWindow::applyRateBeginnerPreset);
    connect(noFlips, &QPushButton::clicked, this, &MainWindow::applyRateNoFlipsPreset);
    connect(acro, &QPushButton::clicked, this, &MainWindow::applyRateAcroNormalPreset);
    connect(smoothThrottle, &QPushButton::clicked, this, &MainWindow::applySmoothThrottlePreset);
    connect(stage, &QPushButton::clicked, this, &MainWindow::stageRateFields);
    connect(write, &QPushButton::clicked, this, &MainWindow::writeSaveReboot);
    return page;
}

QWidget *MainWindow::buildBlackboxTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *configBox = new QGroupBox(QStringLiteral("Blackbox Configuration"), page);
    auto *configForm = new QFormLayout(configBox);
    m_blackboxDev = new QComboBox(configBox);
    m_blackboxDev->addItems({QStringLiteral("NONE"), QStringLiteral("FLASH"), QStringLiteral("SDCARD"), QStringLiteral("SERIAL")});
    m_blackboxMode = new QComboBox(configBox);
    m_blackboxMode->addItems({QStringLiteral("NORMAL")});
    m_blackboxRate = new QSpinBox(configBox);
    m_blackboxRate->setRange(1, 256);
    configForm->addRow(QStringLiteral("blackbox_dev"), m_blackboxDev);
    configForm->addRow(QStringLiteral("blackbox_mode"), m_blackboxMode);
    configForm->addRow(QStringLiteral("blackbox_rate"), m_blackboxRate);

    auto *fieldsBox = new QGroupBox(QStringLiteral("Logged Fields"), page);
    auto *fieldsGrid = new QGridLayout(fieldsBox);
    const QStringList labels = {
        QStringLiteral("Accelerometer"), QStringLiteral("Altitude"), QStringLiteral("Battery"), QStringLiteral("Debug"),
        QStringLiteral("GPS"), QStringLiteral("Gyro"), QStringLiteral("Raw Gyro"), QStringLiteral("Magnetometer"),
        QStringLiteral("Motors"), QStringLiteral("PID"), QStringLiteral("RC"), QStringLiteral("RPM"),
        QStringLiteral("RSSI"), QStringLiteral("Setpoint"),
    };
    for (int i = 0; i < labels.size(); ++i) {
        m_blackboxFields[i] = new QCheckBox(labels[i], fieldsBox);
        fieldsGrid->addWidget(m_blackboxFields[i], i / 2, i % 2);
    }

    auto *serialBox = new QGroupBox(QStringLiteral("Serial Blackbox"), page);
    auto *serialForm = new QFormLayout(serialBox);
    m_blackboxSerialPort = new QComboBox(serialBox);
    m_blackboxSerialPort->addItems({
        QStringLiteral("None"), QStringLiteral("serial_0"), QStringLiteral("serial_1"),
        QStringLiteral("serial_2"), QStringLiteral("serial_soft_0"), QStringLiteral("serial_usb"),
    });
    serialForm->addRow(QStringLiteral("Port helper"), m_blackboxSerialPort);
    auto *serialNote = new QLabel(QStringLiteral(
        "This first pass does not rewrite serial functions automatically. If blackbox_dev is SERIAL, ensure one serial port function includes 128 (BLACKBOX) and does not conflict with RX_SERIAL 64."), serialBox);
    serialNote->setWordWrap(true);
    serialForm->addRow(QStringLiteral("Note"), serialNote);

    auto *statusBox = new QGroupBox(QStringLiteral("Status / Validation"), page);
    auto *statusLayout = new QVBoxLayout(statusBox);
    m_blackboxStorage = new QLabel(QStringLiteral("Storage status not queried in this first pass."), statusBox);
    m_blackboxStorage->setWordWrap(true);
    m_blackboxValidation = new QLabel(QStringLiteral("Load a config dump to validate blackbox settings."), statusBox);
    m_blackboxValidation->setWordWrap(true);
    statusLayout->addWidget(m_blackboxStorage);
    statusLayout->addWidget(m_blackboxValidation);

    auto *columns = new QHBoxLayout();
    auto *left = new QVBoxLayout();
    auto *right = new QVBoxLayout();
    left->addWidget(configBox);
    left->addWidget(serialBox);
    right->addWidget(fieldsBox);
    right->addWidget(statusBox);
    columns->addLayout(left, 1);
    columns->addLayout(right, 2);

    auto *buttons = new QHBoxLayout();
    auto *load = new QPushButton(QStringLiteral("Load Dump"), page);
    auto *off = new QPushButton(QStringLiteral("Blackbox Off"), page);
    auto *basic = new QPushButton(QStringLiteral("Basic Logging"), page);
    auto *debug = new QPushButton(QStringLiteral("Gyro/PID Debug"), page);
    auto *full = new QPushButton(QStringLiteral("Full Logging"), page);
    auto *serial = new QPushButton(QStringLiteral("Serial Blackbox"), page);
    auto *stage = new QPushButton(QStringLiteral("Stage Blackbox Fields"), page);
    auto *write = new QPushButton(QStringLiteral("Write to FC && Reboot"), page);
    buttons->addWidget(load);
    buttons->addWidget(off);
    buttons->addWidget(basic);
    buttons->addWidget(debug);
    buttons->addWidget(full);
    buttons->addWidget(serial);
    buttons->addWidget(stage);
    buttons->addWidget(write);
    buttons->addStretch();

    auto *note = new QLabel(QStringLiteral(
        "Blackbox settings are staged through CLI parameters. Flash erase and log download are intentionally deferred until storage MSP handling is added safely."), page);
    note->setWordWrap(true);

    layout->addLayout(columns);
    layout->addWidget(note);
    layout->addLayout(buttons);
    layout->addStretch();

    connect(load, &QPushButton::clicked, this, &MainWindow::loadConfigDump);
    connect(off, &QPushButton::clicked, this, &MainWindow::applyBlackboxOffPreset);
    connect(basic, &QPushButton::clicked, this, &MainWindow::applyBlackboxBasicPreset);
    connect(debug, &QPushButton::clicked, this, &MainWindow::applyBlackboxDebugPreset);
    connect(full, &QPushButton::clicked, this, &MainWindow::applyBlackboxFullPreset);
    connect(serial, &QPushButton::clicked, this, &MainWindow::applyBlackboxSerialPreset);
    connect(stage, &QPushButton::clicked, this, &MainWindow::stageBlackboxFields);
    connect(write, &QPushButton::clicked, this, &MainWindow::writeSaveReboot);
    return page;
}

QWidget *MainWindow::buildSensorsTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *top = new QHBoxLayout();
    m_attitudeView = new AttitudeView(page);
    top->addWidget(m_attitudeView, 2);

    auto *readoutBox = new QGroupBox(QStringLiteral("Live MSP Sensors"), page);
    auto *readoutForm = new QFormLayout(readoutBox);
    m_sensorLiveLabel = new QLabel(QStringLiteral("Stopped"), readoutBox);
    m_attitudeLabel = new QLabel(QStringLiteral("No MSP_ATTITUDE data"), readoutBox);
    m_accelLabel = new QLabel(QStringLiteral("No MSP_RAW_IMU data"), readoutBox);
    m_gyroLabel = new QLabel(QStringLiteral("No MSP_RAW_IMU data"), readoutBox);
    m_magLabel = new QLabel(QStringLiteral("No MSP_RAW_IMU data"), readoutBox);
    readoutForm->addRow(QStringLiteral("Live view"), m_sensorLiveLabel);
    readoutForm->addRow(QStringLiteral("Attitude"), m_attitudeLabel);
    readoutForm->addRow(QStringLiteral("Accel"), m_accelLabel);
    readoutForm->addRow(QStringLiteral("Gyro"), m_gyroLabel);
    readoutForm->addRow(QStringLiteral("Mag"), m_magLabel);
    top->addWidget(readoutBox, 1);

    auto *alignBox = new QGroupBox(QStringLiteral("Alignment / Calibration"), page);
    auto *alignLayout = new QVBoxLayout(alignBox);
    auto *alignForm = new QFormLayout();
    m_gyroAlign = new QComboBox(alignBox);
    m_gyroAlign->addItems({
        QStringLiteral("DEFAULT"), QStringLiteral("CW0"), QStringLiteral("CW90"), QStringLiteral("CW180"), QStringLiteral("CW270"),
        QStringLiteral("CW0_FLIP"), QStringLiteral("CW90_FLIP"), QStringLiteral("CW180_FLIP"), QStringLiteral("CW270_FLIP"), QStringLiteral("CUSTOM"),
    });
    alignForm->addRow(QStringLiteral("gyro_align"), m_gyroAlign);
    alignLayout->addLayout(alignForm);

    auto *alignNote = new QLabel(QStringLiteral(
        "Flat and still: one accel axis should be near +/-2048, the other accel axes near 0, and gyro axes near 0. "
        "ESP-FC applies gyro_align to both gyro and accelerometer."), alignBox);
    alignNote->setWordWrap(true);
    alignLayout->addWidget(alignNote);

    auto *buttons = new QHBoxLayout();
    auto *start = new QPushButton(QStringLiteral("Start Live"), page);
    auto *stop = new QPushButton(QStringLiteral("Stop"), page);
    auto *load = new QPushButton(QStringLiteral("Load Dump"), page);
    auto *refreshAlign = new QPushButton(QStringLiteral("Refresh gyro_align"), page);
    auto *stageAlign = new QPushButton(QStringLiteral("Stage gyro_align"), page);
    auto *write = new QPushButton(QStringLiteral("Write to FC && Reboot"), page);
    auto *calGyro = new QPushButton(QStringLiteral("Calibrate Gyro"), page);
    buttons->addWidget(start);
    buttons->addWidget(stop);
    buttons->addWidget(load);
    buttons->addWidget(refreshAlign);
    buttons->addWidget(stageAlign);
    buttons->addWidget(write);
    buttons->addWidget(calGyro);
    buttons->addStretch();

    layout->addLayout(top);
    layout->addWidget(alignBox);
    layout->addLayout(buttons);
    layout->addStretch();

    connect(start, &QPushButton::clicked, this, &MainWindow::startSensorPolling);
    connect(stop, &QPushButton::clicked, this, &MainWindow::stopSensorPolling);
    connect(load, &QPushButton::clicked, this, &MainWindow::loadConfigDump);
    connect(refreshAlign, &QPushButton::clicked, this, &MainWindow::refreshGyroAlign);
    connect(stageAlign, &QPushButton::clicked, this, &MainWindow::stageGyroAlign);
    connect(write, &QPushButton::clicked, this, &MainWindow::writeSaveReboot);
    connect(calGyro, &QPushButton::clicked, this, &MainWindow::calibrateGyro);
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
    connect(dump, &QPushButton::clicked, this, [this]() { sendCliWhenReady({QStringLiteral("dump")}); });
    connect(status, &QPushButton::clicked, this, [this]() { sendCliWhenReady({QStringLiteral("status")}); });
    connect(stats, &QPushButton::clicked, this, [this]() { sendCliWhenReady({QStringLiteral("stats")}); });

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
    setTransport(&m_serial);
    if (m_serial.open(port)) {
        setConnectedUi(true);
    } else {
        setTransport(nullptr);
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
    stopSensorPolling();
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
    sendCliWhenReady({command});
    m_cliInput->clear();
}

void MainWindow::requestStatus()
{
    sendMsp(MspCodec::Status);
}

void MainWindow::requestHandshake()
{
    sendMsp(MspCodec::ApiVersion);
    sendMsp(MspCodec::FcVariant);
    sendMsp(MspCodec::FcVersion);
    sendMsp(MspCodec::BoardInfo);
    sendMsp(MspCodec::BuildInfo);
    sendMsp(MspCodec::Status);
    sendMsp(MspCodec::RxConfigCommand);
}

void MainWindow::requestRxConfig()
{
    sendMsp(MspCodec::RxConfigCommand);
}

void MainWindow::applyRxProvider()
{
    const quint8 provider = static_cast<quint8>(m_rxProvider->currentData().toUInt());
    sendMsp(MspCodec::SetRxConfigCommand, MspCodec::encodeSetRxProvider(m_rxConfig, provider));
    sendMsp(MspCodec::RxConfigCommand);
}

void MainWindow::saveToFc()
{
    sendMsp(MspCodec::EepromWrite);
    sendCliWhenReady({QStringLiteral("save")});
}

void MainWindow::writeSaveReboot()
{
    const QStringList configCommands = m_config.toCliSetCommands();
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("Write and Reboot FC"),
        QStringLiteral("Write %1 pending config changes, save, and reboot the flight controller now? Motors must be disarmed.")
            .arg(configCommands.size()));
    if (answer != QMessageBox::Yes) {
        return;
    }

    QStringList commands = configCommands;
    commands << QStringLiteral("save") << QStringLiteral("reboot");
    sendCliWhenReady(commands);
    sendMsp(MspCodec::EepromWrite);
    sendMsp(MspCodec::Reboot);

    if (!configCommands.isEmpty()) {
        m_config.clearDirty();
        updateConfigUi();
    }
    appendLog(configCommands.isEmpty()
        ? QStringLiteral("Queued save and reboot.")
        : QStringLiteral("Queued %1 config changes, save, and reboot.").arg(configCommands.size()));
}

void MainWindow::loadConfigDump()
{
    m_captureDump = true;
    m_cliCapture.clear();
    sendCliWhenReady({QStringLiteral("dump")});
    appendLog(QStringLiteral("Requested CLI dump"));
}

void MainWindow::applyConfigChanges()
{
    const QStringList commands = m_config.toCliSetCommands();
    if (commands.isEmpty()) {
        appendLog(QStringLiteral("No config changes to apply"));
        return;
    }
    sendCliWhenReady(commands);
    appendLog(QStringLiteral("Queued %1 pending CLI settings. Use Write to FC && Reboot to persist and restart.").arg(commands.size()));
    m_config.clearDirty();
    updateConfigUi();
}

void MainWindow::rebootFc()
{
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("Reboot FC"),
        QStringLiteral("Reboot the flight controller now? Motors must be disarmed."));
    if (answer != QMessageBox::Yes) {
        return;
    }
    sendCliWhenReady({QStringLiteral("reboot")});
    sendMsp(MspCodec::Reboot);
}

void MainWindow::applyMotorsFromUi()
{
    m_config.setValue(QStringLiteral("output_motor_protocol"), m_motorProtocol->currentText());
    m_config.setInt(QStringLiteral("output_motor_rate"), m_motorRate->value());
    m_config.setInt(QStringLiteral("output_min_command"), m_minCommand->value());
    m_config.setInt(QStringLiteral("output_min_throttle"), m_minThrottle->value());
    m_config.setInt(QStringLiteral("output_max_throttle"), m_maxThrottle->value());
    m_config.setInt(QStringLiteral("mixer_output_limit"), m_outputLimit->value());
    m_config.setValue(QStringLiteral("mixer_throttle_limit_type"), m_throttleLimitType->currentText());
    m_config.setInt(QStringLiteral("mixer_throttle_limit_percent"), m_throttleLimitPercent->value());
    for (int i = 0; i < 4; ++i) {
        m_config.setInt(QStringLiteral("pin_output_%1").arg(i), m_motorPins[i]->value());
    }
    appendLog(QStringLiteral("Motor fields staged. Use Write to FC && Reboot to write them to the FC."));
    updateConfigUi();
}

void MainWindow::applyMotorPreset()
{
    m_config.setValue(QStringLiteral("mixer_type"), QStringLiteral("QUADX"));
    m_config.setInt(QStringLiteral("mix_outputs"), 4);
    m_config.setValue(QStringLiteral("output_motor_protocol"), QStringLiteral("PWM"));
    m_config.setInt(QStringLiteral("output_motor_rate"), 50);
    m_config.setInt(QStringLiteral("output_min_command"), 1000);
    m_config.setInt(QStringLiteral("output_min_throttle"), 1070);
    m_config.setInt(QStringLiteral("output_max_throttle"), 2000);
    m_config.setInt(QStringLiteral("mixer_output_limit"), 100);
    m_config.setValue(QStringLiteral("mixer_throttle_limit_type"), QStringLiteral("NONE"));
    m_config.setInt(QStringLiteral("mixer_throttle_limit_percent"), 100);
    m_config.setInt(QStringLiteral("pin_output_0"), 39);
    m_config.setInt(QStringLiteral("pin_output_1"), 40);
    m_config.setInt(QStringLiteral("pin_output_2"), 41);
    m_config.setInt(QStringLiteral("pin_output_3"), 42);
    for (int i = 0; i < 4; ++i) {
        m_config.set(QStringLiteral("output_%1").arg(i), {QStringLiteral("M"), QStringLiteral("N"), QStringLiteral("1000"), QStringLiteral("1500"), QStringLiteral("2000")});
    }
    appendLog(QStringLiteral("PWM QuadX motor preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::stagePidFields()
{
    const QStringList axes = {QStringLiteral("roll"), QStringLiteral("pitch"), QStringLiteral("yaw"), QStringLiteral("level")};
    const QStringList terms = {QStringLiteral("p"), QStringLiteral("i"), QStringLiteral("d"), QStringLiteral("f")};
    for (int row = 0; row < axes.size(); ++row) {
        for (int col = 0; col < terms.size(); ++col) {
            m_config.setInt(QStringLiteral("pid_%1_%2").arg(axes.at(row), terms.at(col)), m_pid[row][col]->value());
        }
    }
    m_config.setValue(QStringLiteral("pid_dterm_lpf_type"), m_dtermLpfType->currentText());
    m_config.setInt(QStringLiteral("pid_dterm_lpf_freq"), m_dtermLpfFreq->value());
    m_config.setValue(QStringLiteral("pid_dterm_lpf2_type"), m_dtermLpf2Type->currentText());
    m_config.setInt(QStringLiteral("pid_dterm_lpf2_freq"), m_dtermLpf2Freq->value());
    m_config.setInt(QStringLiteral("pid_dterm_notch_freq"), m_dtermNotchFreq->value());
    m_config.setInt(QStringLiteral("pid_dterm_notch_cutoff"), m_dtermNotchCutoff->value());
    m_config.setInt(QStringLiteral("pid_dterm_dyn_lpf_min"), m_dtermDynMin->value());
    m_config.setInt(QStringLiteral("pid_dterm_dyn_lpf_max"), m_dtermDynMax->value());
    m_config.setInt(QStringLiteral("pid_iterm_limit"), m_itermLimit->value());
    m_config.setValue(QStringLiteral("pid_iterm_zero"), m_itermZero->currentText());
    m_config.setValue(QStringLiteral("pid_iterm_relax"), m_itermRelax->currentText());
    m_config.setInt(QStringLiteral("pid_iterm_relax_cutoff"), m_itermRelaxCutoff->value());
    m_config.setInt(QStringLiteral("pid_tpa_scale"), m_tpaScale->value());
    m_config.setInt(QStringLiteral("pid_tpa_breakpoint"), m_tpaBreakpoint->value());
    appendLog(QStringLiteral("PID fields staged. Use Write to FC && Reboot to write them to the FC."));
    updateConfigUi();
}

void MainWindow::applyPidBeginnerPreset()
{
    m_config.setInt(QStringLiteral("input_roll_expo"), 30);
    m_config.setInt(QStringLiteral("input_pitch_expo"), 30);
    m_config.setInt(QStringLiteral("input_yaw_expo"), 20);
    m_config.setInt(QStringLiteral("input_roll_limit"), 250);
    m_config.setInt(QStringLiteral("input_pitch_limit"), 250);
    appendLog(QStringLiteral("Beginner Soft staged rate/expo/limit changes; PID gains were left unchanged."));
    updateConfigUi();
}

void MainWindow::stageFilterFields()
{
    m_config.setValue(QStringLiteral("gyro_lpf_type"), m_filterGyroLpfType->currentText());
    m_config.setInt(QStringLiteral("gyro_lpf_freq"), m_filterGyroLpfFreq->value());
    m_config.setValue(QStringLiteral("gyro_lpf2_type"), m_filterGyroLpf2Type->currentText());
    m_config.setInt(QStringLiteral("gyro_lpf2_freq"), m_filterGyroLpf2Freq->value());
    m_config.setValue(QStringLiteral("gyro_lpf3_type"), m_filterGyroLpf3Type->currentText());
    m_config.setInt(QStringLiteral("gyro_lpf3_freq"), m_filterGyroLpf3Freq->value());
    m_config.setInt(QStringLiteral("gyro_notch1_freq"), m_filterGyroNotch1Freq->value());
    m_config.setInt(QStringLiteral("gyro_notch1_cutoff"), m_filterGyroNotch1Cutoff->value());
    m_config.setInt(QStringLiteral("gyro_notch2_freq"), m_filterGyroNotch2Freq->value());
    m_config.setInt(QStringLiteral("gyro_notch2_cutoff"), m_filterGyroNotch2Cutoff->value());
    m_config.setBool(QStringLiteral("feature_dyn_notch"), m_filterDynNotch->isChecked());
    m_config.setInt(QStringLiteral("gyro_dyn_lpf_min"), m_filterGyroDynLpfMin->value());
    m_config.setInt(QStringLiteral("gyro_dyn_lpf_max"), m_filterGyroDynLpfMax->value());
    m_config.setInt(QStringLiteral("gyro_dyn_notch_q"), m_filterGyroDynNotchQ->value());
    m_config.setInt(QStringLiteral("gyro_dyn_notch_count"), m_filterGyroDynNotchCount->value());
    m_config.setInt(QStringLiteral("gyro_dyn_notch_min"), m_filterGyroDynNotchMin->value());
    m_config.setInt(QStringLiteral("gyro_dyn_notch_max"), m_filterGyroDynNotchMax->value());
    m_config.setValue(QStringLiteral("pid_dterm_lpf_type"), m_filterDtermLpfType->currentText());
    m_config.setInt(QStringLiteral("pid_dterm_lpf_freq"), m_filterDtermLpfFreq->value());
    m_config.setValue(QStringLiteral("pid_dterm_lpf2_type"), m_filterDtermLpf2Type->currentText());
    m_config.setInt(QStringLiteral("pid_dterm_lpf2_freq"), m_filterDtermLpf2Freq->value());
    m_config.setInt(QStringLiteral("pid_dterm_notch_freq"), m_filterDtermNotchFreq->value());
    m_config.setInt(QStringLiteral("pid_dterm_notch_cutoff"), m_filterDtermNotchCutoff->value());
    m_config.setInt(QStringLiteral("pid_dterm_dyn_lpf_min"), m_filterDtermDynLpfMin->value());
    m_config.setInt(QStringLiteral("pid_dterm_dyn_lpf_max"), m_filterDtermDynLpfMax->value());
    appendLog(QStringLiteral("Filter fields staged. Use Write to FC && Reboot to write them to the FC."));
    updateConfigUi();
}

void MainWindow::applyFilterDefaultPreset()
{
    m_config.setValue(QStringLiteral("gyro_lpf_type"), QStringLiteral("PT1"));
    m_config.setInt(QStringLiteral("gyro_lpf_freq"), 100);
    m_config.setValue(QStringLiteral("gyro_lpf2_type"), QStringLiteral("PT1"));
    m_config.setInt(QStringLiteral("gyro_lpf2_freq"), 213);
    m_config.setValue(QStringLiteral("gyro_lpf3_type"), QStringLiteral("FO"));
    m_config.setInt(QStringLiteral("gyro_lpf3_freq"), 150);
    m_config.setInt(QStringLiteral("gyro_notch1_freq"), 0);
    m_config.setInt(QStringLiteral("gyro_notch1_cutoff"), 0);
    m_config.setInt(QStringLiteral("gyro_notch2_freq"), 0);
    m_config.setInt(QStringLiteral("gyro_notch2_cutoff"), 0);
    m_config.setInt(QStringLiteral("gyro_dyn_lpf_min"), 170);
    m_config.setInt(QStringLiteral("gyro_dyn_lpf_max"), 425);
    m_config.setInt(QStringLiteral("gyro_dyn_notch_q"), 300);
    m_config.setInt(QStringLiteral("gyro_dyn_notch_count"), 4);
    m_config.setInt(QStringLiteral("gyro_dyn_notch_min"), 80);
    m_config.setInt(QStringLiteral("gyro_dyn_notch_max"), 400);
    m_config.setValue(QStringLiteral("pid_dterm_lpf_type"), QStringLiteral("PT1"));
    m_config.setInt(QStringLiteral("pid_dterm_lpf_freq"), 100);
    m_config.setValue(QStringLiteral("pid_dterm_lpf2_type"), QStringLiteral("PT1"));
    m_config.setInt(QStringLiteral("pid_dterm_lpf2_freq"), 100);
    m_config.setInt(QStringLiteral("pid_dterm_notch_freq"), 0);
    m_config.setInt(QStringLiteral("pid_dterm_notch_cutoff"), 0);
    appendLog(QStringLiteral("Default filter preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::applyFilterMorePreset()
{
    applyFilterDefaultPreset();
    m_config.setInt(QStringLiteral("gyro_lpf_freq"), 80);
    m_config.setInt(QStringLiteral("gyro_lpf2_freq"), 150);
    m_config.setInt(QStringLiteral("gyro_lpf3_freq"), 120);
    m_config.setInt(QStringLiteral("pid_dterm_lpf_freq"), 90);
    m_config.setInt(QStringLiteral("pid_dterm_lpf2_freq"), 90);
    appendLog(QStringLiteral("More Filtering preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::applyFilterStrongPreset()
{
    applyFilterDefaultPreset();
    m_config.setInt(QStringLiteral("gyro_lpf_freq"), 60);
    m_config.setInt(QStringLiteral("gyro_lpf2_freq"), 100);
    m_config.setInt(QStringLiteral("gyro_lpf3_freq"), 90);
    m_config.setInt(QStringLiteral("pid_dterm_lpf_freq"), 70);
    m_config.setInt(QStringLiteral("pid_dterm_lpf2_freq"), 70);
    appendLog(QStringLiteral("Strong Filtering preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::applyFilterLowLatencyPreset()
{
    applyFilterDefaultPreset();
    m_config.setInt(QStringLiteral("gyro_lpf_freq"), 120);
    m_config.setInt(QStringLiteral("gyro_lpf2_freq"), 250);
    m_config.setInt(QStringLiteral("gyro_lpf3_freq"), 180);
    m_config.setInt(QStringLiteral("pid_dterm_lpf_freq"), 120);
    m_config.setInt(QStringLiteral("pid_dterm_lpf2_freq"), 120);
    appendLog(QStringLiteral("Low Latency filter preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::stageRateFields()
{
    m_config.setValue(QStringLiteral("input_rate_type"), m_rateType->currentText());

    const QStringList axes = {QStringLiteral("roll"), QStringLiteral("pitch"), QStringLiteral("yaw")};
    const QStringList fields = {QStringLiteral("rate"), QStringLiteral("srate"), QStringLiteral("expo"), QStringLiteral("limit")};
    for (int row = 0; row < axes.size(); ++row) {
        for (int col = 0; col < fields.size(); ++col) {
            m_config.setInt(QStringLiteral("input_%1_%2").arg(axes.at(row), fields.at(col)), m_rate[row][col]->value());
        }
    }

    m_config.setInt(QStringLiteral("input_deadband"), m_inputDeadband->value());
    m_config.setValue(QStringLiteral("input_lpf_type"), m_inputLpfType->currentText());
    m_config.setInt(QStringLiteral("input_lpf_freq"), m_inputLpfFreq->value());
    m_config.setInt(QStringLiteral("input_lpf_factor"), m_inputLpfFactor->value());
    m_config.setValue(QStringLiteral("input_ff_lpf_type"), m_inputFfLpfType->currentText());
    m_config.setInt(QStringLiteral("input_ff_lpf_freq"), m_inputFfLpfFreq->value());
    m_config.setValue(QStringLiteral("mixer_throttle_limit_type"), m_rateThrottleLimitType->currentText());
    m_config.setInt(QStringLiteral("mixer_throttle_limit_percent"), m_rateThrottleLimitPercent->value());
    m_config.setInt(QStringLiteral("output_min_throttle"), m_rateMinThrottle->value());
    appendLog(QStringLiteral("Rate fields staged. Use Write to FC && Reboot to write them to the FC."));
    updateConfigUi();
}

void MainWindow::applyRateBeginnerPreset()
{
    m_config.setInt(QStringLiteral("input_roll_expo"), 30);
    m_config.setInt(QStringLiteral("input_pitch_expo"), 30);
    m_config.setInt(QStringLiteral("input_yaw_expo"), 20);
    m_config.setInt(QStringLiteral("input_roll_limit"), 250);
    m_config.setInt(QStringLiteral("input_pitch_limit"), 250);
    appendLog(QStringLiteral("Beginner Soft rate preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::applyRateNoFlipsPreset()
{
    m_config.setInt(QStringLiteral("input_roll_expo"), 35);
    m_config.setInt(QStringLiteral("input_pitch_expo"), 35);
    m_config.setInt(QStringLiteral("input_yaw_expo"), 20);
    m_config.setInt(QStringLiteral("input_roll_limit"), 200);
    m_config.setInt(QStringLiteral("input_pitch_limit"), 200);
    m_config.setInt(QStringLiteral("input_yaw_limit"), 250);
    appendLog(QStringLiteral("No Flips rate preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::applyRateAcroNormalPreset()
{
    m_config.setValue(QStringLiteral("input_rate_type"), QStringLiteral("ACTUAL"));
    m_config.setInt(QStringLiteral("input_roll_rate"), 20);
    m_config.setInt(QStringLiteral("input_roll_srate"), 40);
    m_config.setInt(QStringLiteral("input_roll_expo"), 0);
    m_config.setInt(QStringLiteral("input_roll_limit"), 1998);
    m_config.setInt(QStringLiteral("input_pitch_rate"), 20);
    m_config.setInt(QStringLiteral("input_pitch_srate"), 40);
    m_config.setInt(QStringLiteral("input_pitch_expo"), 0);
    m_config.setInt(QStringLiteral("input_pitch_limit"), 1998);
    m_config.setInt(QStringLiteral("input_yaw_rate"), 30);
    m_config.setInt(QStringLiteral("input_yaw_srate"), 36);
    m_config.setInt(QStringLiteral("input_yaw_expo"), 0);
    m_config.setInt(QStringLiteral("input_yaw_limit"), 1998);
    m_config.setInt(QStringLiteral("input_deadband"), 3);
    appendLog(QStringLiteral("Acro Normal rate preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::applySmoothThrottlePreset()
{
    m_config.setValue(QStringLiteral("mixer_throttle_limit_type"), QStringLiteral("SCALE"));
    m_config.setInt(QStringLiteral("mixer_throttle_limit_percent"), 80);
    appendLog(QStringLiteral("Smooth Throttle preset staged. Use an EdgeTX throttle curve for detailed shaping."));
    updateConfigUi();
}

void MainWindow::stageBlackboxFields()
{
    static const QStringList fieldNames = {
        QStringLiteral("blackbox_log_acc"), QStringLiteral("blackbox_log_alt"), QStringLiteral("blackbox_log_bat"),
        QStringLiteral("blackbox_log_debug"), QStringLiteral("blackbox_log_gps"), QStringLiteral("blackbox_log_gyro"),
        QStringLiteral("blackbox_log_gyro_raw"), QStringLiteral("blackbox_log_mag"), QStringLiteral("blackbox_log_motor"),
        QStringLiteral("blackbox_log_pid"), QStringLiteral("blackbox_log_rc"), QStringLiteral("blackbox_log_rpm"),
        QStringLiteral("blackbox_log_rssi"), QStringLiteral("blackbox_log_sp"),
    };

    m_config.setValue(QStringLiteral("blackbox_dev"), m_blackboxDev->currentText());
    m_config.setValue(QStringLiteral("blackbox_mode"), m_blackboxMode->currentText());
    m_config.setInt(QStringLiteral("blackbox_rate"), m_blackboxRate->value());
    for (int i = 0; i < fieldNames.size(); ++i) {
        m_config.setBool(fieldNames[i], m_blackboxFields[i] && m_blackboxFields[i]->isChecked());
    }
    appendLog(QStringLiteral("Blackbox fields staged. Use Write to FC && Reboot to write them to the FC."));
    updateConfigUi();
}

void MainWindow::applyBlackboxOffPreset()
{
    m_config.setValue(QStringLiteral("blackbox_dev"), QStringLiteral("NONE"));
    m_config.setValue(QStringLiteral("blackbox_mode"), QStringLiteral("NORMAL"));
    m_config.setInt(QStringLiteral("blackbox_rate"), 32);
    appendLog(QStringLiteral("Blackbox Off preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::applyBlackboxBasicPreset()
{
    m_config.setValue(QStringLiteral("blackbox_dev"), QStringLiteral("FLASH"));
    m_config.setValue(QStringLiteral("blackbox_mode"), QStringLiteral("NORMAL"));
    m_config.setInt(QStringLiteral("blackbox_rate"), 32);
    m_config.setBool(QStringLiteral("blackbox_log_acc"), true);
    m_config.setBool(QStringLiteral("blackbox_log_alt"), false);
    m_config.setBool(QStringLiteral("blackbox_log_bat"), true);
    m_config.setBool(QStringLiteral("blackbox_log_debug"), false);
    m_config.setBool(QStringLiteral("blackbox_log_gps"), false);
    m_config.setBool(QStringLiteral("blackbox_log_gyro"), true);
    m_config.setBool(QStringLiteral("blackbox_log_gyro_raw"), false);
    m_config.setBool(QStringLiteral("blackbox_log_mag"), false);
    m_config.setBool(QStringLiteral("blackbox_log_motor"), true);
    m_config.setBool(QStringLiteral("blackbox_log_pid"), true);
    m_config.setBool(QStringLiteral("blackbox_log_rc"), true);
    m_config.setBool(QStringLiteral("blackbox_log_rpm"), false);
    m_config.setBool(QStringLiteral("blackbox_log_rssi"), true);
    m_config.setBool(QStringLiteral("blackbox_log_sp"), true);
    appendLog(QStringLiteral("Basic Blackbox preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::applyBlackboxDebugPreset()
{
    m_config.setValue(QStringLiteral("blackbox_dev"), QStringLiteral("FLASH"));
    m_config.setValue(QStringLiteral("blackbox_mode"), QStringLiteral("NORMAL"));
    m_config.setInt(QStringLiteral("blackbox_rate"), 16);
    m_config.setBool(QStringLiteral("blackbox_log_acc"), false);
    m_config.setBool(QStringLiteral("blackbox_log_alt"), false);
    m_config.setBool(QStringLiteral("blackbox_log_bat"), true);
    m_config.setBool(QStringLiteral("blackbox_log_debug"), true);
    m_config.setBool(QStringLiteral("blackbox_log_gps"), false);
    m_config.setBool(QStringLiteral("blackbox_log_gyro"), true);
    m_config.setBool(QStringLiteral("blackbox_log_gyro_raw"), true);
    m_config.setBool(QStringLiteral("blackbox_log_mag"), false);
    m_config.setBool(QStringLiteral("blackbox_log_motor"), true);
    m_config.setBool(QStringLiteral("blackbox_log_pid"), true);
    m_config.setBool(QStringLiteral("blackbox_log_rc"), true);
    m_config.setBool(QStringLiteral("blackbox_log_rpm"), true);
    m_config.setBool(QStringLiteral("blackbox_log_rssi"), true);
    m_config.setBool(QStringLiteral("blackbox_log_sp"), true);
    appendLog(QStringLiteral("Gyro/PID Debug Blackbox preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::applyBlackboxFullPreset()
{
    static const QStringList fieldNames = {
        QStringLiteral("blackbox_log_acc"), QStringLiteral("blackbox_log_alt"), QStringLiteral("blackbox_log_bat"),
        QStringLiteral("blackbox_log_debug"), QStringLiteral("blackbox_log_gps"), QStringLiteral("blackbox_log_gyro"),
        QStringLiteral("blackbox_log_gyro_raw"), QStringLiteral("blackbox_log_mag"), QStringLiteral("blackbox_log_motor"),
        QStringLiteral("blackbox_log_pid"), QStringLiteral("blackbox_log_rc"), QStringLiteral("blackbox_log_rpm"),
        QStringLiteral("blackbox_log_rssi"), QStringLiteral("blackbox_log_sp"),
    };
    m_config.setValue(QStringLiteral("blackbox_dev"), QStringLiteral("FLASH"));
    m_config.setValue(QStringLiteral("blackbox_mode"), QStringLiteral("NORMAL"));
    m_config.setInt(QStringLiteral("blackbox_rate"), 16);
    for (const QString &name : fieldNames) {
        m_config.setBool(name, true);
    }
    appendLog(QStringLiteral("Full Blackbox preset staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::applyBlackboxSerialPreset()
{
    m_config.setValue(QStringLiteral("blackbox_dev"), QStringLiteral("SERIAL"));
    m_config.setValue(QStringLiteral("blackbox_mode"), QStringLiteral("NORMAL"));
    m_config.setInt(QStringLiteral("blackbox_rate"), 16);
    appendLog(QStringLiteral("Serial Blackbox preset staged. Check that one serial port function includes 128 (BLACKBOX)."));
    updateConfigUi();
}

void MainWindow::startSensorPolling()
{
    if (!m_transport || !m_transport->isOpen()) {
        appendLog(QStringLiteral("Connect before starting live sensors."));
        return;
    }
    m_sensorPollTimer.start();
    if (m_sensorLiveLabel) {
        m_sensorLiveLabel->setText(QStringLiteral("Polling MSP_ATTITUDE/MSP_RAW_IMU at 10 Hz"));
    }
    requestSensorFrame();
}

void MainWindow::stopSensorPolling()
{
    m_sensorPollTimer.stop();
    if (m_sensorLiveLabel) {
        m_sensorLiveLabel->setText(QStringLiteral("Stopped"));
    }
}

void MainWindow::requestSensorFrame()
{
    sendMsp(MspCodec::Attitude);
    sendMsp(MspCodec::RawImu);
}

void MainWindow::stageGyroAlign()
{
    if (!m_gyroAlign) {
        return;
    }
    m_config.setValue(QStringLiteral("gyro_align"), m_gyroAlign->currentText());
    appendLog(QStringLiteral("gyro_align staged. Use Write to FC && Reboot to write it to the FC."));
    updateConfigUi();
}

void MainWindow::refreshGyroAlign()
{
    m_pendingParamName = QStringLiteral("gyro_align");
    m_pendingParamCapture.clear();
    sendCliWhenReady({QStringLiteral("get gyro_align")});
    appendLog(QStringLiteral("Requested gyro_align from CLI"));
}

void MainWindow::calibrateGyro()
{
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("Calibrate Gyro"),
        QStringLiteral("Keep the flight controller still and level. Run gyro calibration now?"));
    if (answer != QMessageBox::Yes) {
        return;
    }
    sendCliWhenReady({QStringLiteral("cal gyro")});
}

void MainWindow::handleMspMessage(const MspMessage &message)
{
    appendLog(QStringLiteral("Received %1 (%2 bytes)").arg(MspCodec::commandName(message.command)).arg(message.payload.size()));
    if (message.command == MspCodec::ApiVersion) {
        m_firmware = MspCodec::parseApiVersion(message, m_firmware);
        updateFirmwareLabel();
    } else if (message.command == MspCodec::FcVariant) {
        m_firmware = MspCodec::parseFcVariant(message, m_firmware);
        updateFirmwareLabel();
    } else if (message.command == MspCodec::FcVersion) {
        m_firmware = MspCodec::parseFcVersion(message, m_firmware);
        updateFirmwareLabel();
    } else if (message.command == MspCodec::BoardInfo) {
        m_firmware = MspCodec::parseBoardInfo(message, m_firmware);
        updateFirmwareLabel();
    } else if (message.command == MspCodec::BuildInfo) {
        m_firmware = MspCodec::parseBuildInfo(message, m_firmware);
        updateFirmwareLabel();
    } else if (message.command == MspCodec::RxConfigCommand) {
        m_rxConfig = MspCodec::parseRxConfig(message);
        updateRxUi();
    } else if (message.command == MspCodec::Attitude) {
        m_attitude = MspCodec::parseAttitude(message);
        updateSensorUi();
    } else if (message.command == MspCodec::RawImu) {
        m_rawImu = MspCodec::parseRawImu(message);
        updateSensorUi();
    } else if (message.command == MspCodec::Status || message.command == MspCodec::StatusEx) {
        FcStatus status = MspCodec::parseStatus(message);
        m_state.setStatus(status);
        m_sensorLabel->setText(QStringLiteral("gyro=%1 accel=%2 baro=%3 mag=%4 gps=%5")
            .arg(status.gyroPresent).arg(status.accelPresent).arg(status.baroPresent).arg(status.magPresent).arg(status.gpsPresent));
        m_statusLabel->setText(QStringLiteral("loop=%1 us, cpu=%2, i2c errors=%3")
            .arg(status.loopTimeUs).arg(status.cpuLoad).arg(status.i2cErrors));
        m_armingLabel->setText(armingFlagsText(status.armingDisableFlags));
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
    if (!connected) {
        stopSensorPolling();
    }
}

void MainWindow::setTransport(ITransport *transport)
{
    if (m_transport) {
        disconnect(m_transport, nullptr, this, nullptr);
    }
    m_transport = transport;
    m_cli.setTransport(transport);
    m_msp.setTransport(transport);

    if (!m_transport) {
        return;
    }
    connect(m_transport, &ITransport::opened, this, [this]() { setConnectedUi(true); });
    connect(m_transport, &ITransport::opened, this, &MainWindow::requestHandshake);
    connect(m_transport, &ITransport::closed, this, [this]() { setConnectedUi(false); });
    connect(m_transport, &ITransport::errorOccurred, this, &MainWindow::appendLog);
}

void MainWindow::sendMsp(quint16 command, const QByteArray &payload)
{
    m_msp.request(command, payload);
}

void MainWindow::sendCliWhenReady(const QStringList &commands)
{
    if (commands.isEmpty()) {
        return;
    }
    m_pendingCliCommands += commands;
    if (m_cli.isActive()) {
        flushPendingCliCommands();
        return;
    }
    m_cli.enter();
}

void MainWindow::flushPendingCliCommands()
{
    if (!m_cli.isActive() || m_pendingCliCommands.isEmpty()) {
        return;
    }
    const QStringList commands = m_pendingCliCommands;
    m_pendingCliCommands.clear();
    for (const QString &command : commands) {
        m_cli.sendCommand(command);
    }
}

void MainWindow::updateFirmwareLabel()
{
    m_firmwareLabel->setText(m_firmware.summary());
}

void MainWindow::updateRxUi()
{
    const int index = m_rxProvider->findData(m_rxConfig.serialRxProvider);
    if (index >= 0) {
        m_rxProvider->setCurrentIndex(index);
    }
    m_rxSummary->setText(QStringLiteral("%1, min/mid/max RC %2/%3/%4, min/max check %5/%6")
        .arg(RxConfig::providerName(m_rxConfig.serialRxProvider))
        .arg(m_rxConfig.minRc)
        .arg(m_rxConfig.midRc)
        .arg(m_rxConfig.maxRc)
        .arg(m_rxConfig.minCheck)
        .arg(m_rxConfig.maxCheck));
}

void MainWindow::updateConfigUi()
{
    const int dirtyCount = m_config.dirtyParams().size();
    if (m_configSummary) {
        m_configSummary->setText(QStringLiteral("%1 params loaded, %2 pending changes")
            .arg(m_config.size())
            .arg(dirtyCount));
    }
    if (m_applyConfig) {
        m_applyConfig->setEnabled(true);
    }
    updateMotorsUi();
    updateMotorValidation();
    updatePidUi();
    updatePidValidation();
    updateFiltersUi();
    updateFilterValidation();
    updateRatesUi();
    updateRateValidation();
    updateBlackboxUi();
    updateBlackboxValidation();
    updateSensorUi();
    syncGyroAlignUiFromConfig();
    if (!m_setupValidation) {
        return;
    }

    QStringList checks;
    auto mark = [](bool ok, const QString &label) {
        return QStringLiteral("%1 %2").arg(ok ? QStringLiteral("OK:") : QStringLiteral("WARN:"), label);
    };

    checks << mark(m_state.status().gyroPresent, QStringLiteral("IMU gyro detected"));
    checks << mark(m_state.status().accelPresent, QStringLiteral("accelerometer detected"));
    checks << mark((m_state.status().armingDisableFlags & (1u << 2)) == 0, QStringLiteral("RX_FAILSAFE clear"));
    checks << mark((m_state.status().armingDisableFlags & (1u << 7)) == 0, QStringLiteral("throttle low enough to arm"));
    checks << mark(m_config.value(QStringLiteral("output_motor_protocol")) != QStringLiteral("DISABLED"), QStringLiteral("motor protocol configured"));
    checks << mark(m_config.intValue(QStringLiteral("mix_outputs"), 0) > 0 || m_config.value(QStringLiteral("mixer_type")) == QStringLiteral("QUADX"), QStringLiteral("mixer outputs configured"));
    checks << mark(m_config.values(QStringLiteral("mode_0")).value(2) != QStringLiteral("900") || m_config.values(QStringLiteral("mode_0")).value(3) != QStringLiteral("900"), QStringLiteral("arm mode configured"));

    const QStringList serial1 = m_config.values(QStringLiteral("serial_1"));
    const QStringList serial2 = m_config.values(QStringLiteral("serial_2"));
    const bool duplicateRx = serial1.value(0) == QStringLiteral("64") && serial2.value(0) == QStringLiteral("64");
    checks << mark(!duplicateRx, QStringLiteral("only one serial port uses RX_SERIAL"));
    m_setupValidation->setText(checks.join(QStringLiteral("\n")));
}

void MainWindow::updateMotorsUi()
{
    if (!m_motorProtocol) {
        return;
    }
    auto setCombo = [](QComboBox *combo, const QString &value) {
        const int index = combo->findText(value);
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
    };
    setCombo(m_motorProtocol, m_config.value(QStringLiteral("output_motor_protocol"), QStringLiteral("PWM")));
    setCombo(m_throttleLimitType, m_config.value(QStringLiteral("mixer_throttle_limit_type"), QStringLiteral("NONE")));
    m_motorRate->setValue(m_config.intValue(QStringLiteral("output_motor_rate"), 50));
    m_minCommand->setValue(m_config.intValue(QStringLiteral("output_min_command"), 1000));
    m_minThrottle->setValue(m_config.intValue(QStringLiteral("output_min_throttle"), 1070));
    m_maxThrottle->setValue(m_config.intValue(QStringLiteral("output_max_throttle"), 2000));
    m_outputLimit->setValue(m_config.intValue(QStringLiteral("mixer_output_limit"), 100));
    m_throttleLimitPercent->setValue(m_config.intValue(QStringLiteral("mixer_throttle_limit_percent"), 100));
    const int defaultPins[4] = {39, 40, 41, 42};
    for (int i = 0; i < 4; ++i) {
        m_motorPins[i]->setValue(m_config.intValue(QStringLiteral("pin_output_%1").arg(i), defaultPins[i]));
    }
}

void MainWindow::updateMotorValidation()
{
    if (!m_motorValidation) {
        return;
    }
    QStringList checks;
    auto mark = [](bool ok, const QString &label) {
        return QStringLiteral("%1 %2").arg(ok ? QStringLiteral("OK:") : QStringLiteral("WARN:"), label);
    };
    const QString protocol = m_config.value(QStringLiteral("output_motor_protocol"));
    checks << mark(!protocol.isEmpty() && protocol != QStringLiteral("DISABLED"), QStringLiteral("motor protocol is enabled"));
    checks << mark(m_config.intValue(QStringLiteral("mix_outputs"), 0) > 0 || m_config.value(QStringLiteral("mixer_type")) == QStringLiteral("QUADX"), QStringLiteral("mixer has active outputs"));
    checks << mark(m_config.intValue(QStringLiteral("output_min_command"), 0) < m_config.intValue(QStringLiteral("output_min_throttle"), 0), QStringLiteral("min command is below min throttle"));
    checks << mark(m_config.intValue(QStringLiteral("output_min_throttle"), 0) < m_config.intValue(QStringLiteral("output_max_throttle"), 0), QStringLiteral("min throttle is below max throttle"));
    checks << mark(m_config.intValue(QStringLiteral("output_motor_rate"), 0) <= 500 || !protocol.startsWith(QStringLiteral("PWM")), QStringLiteral("PWM rate is standard/compatible"));
    checks << mark(m_config.intValue(QStringLiteral("pin_output_0"), -1) != -1 && m_config.intValue(QStringLiteral("pin_output_1"), -1) != -1 && m_config.intValue(QStringLiteral("pin_output_2"), -1) != -1 && m_config.intValue(QStringLiteral("pin_output_3"), -1) != -1, QStringLiteral("all four motor pins assigned"));
    checks << QStringLiteral("INFO: Props-off confirmation will be required before future live motor tests.");
    m_motorValidation->setText(checks.join(QStringLiteral("\n")));
}

void MainWindow::updatePidUi()
{
    if (!m_pid[0][0]) {
        return;
    }
    auto setCombo = [](QComboBox *combo, const QString &value) {
        const int index = combo->findText(value);
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
    };
    const QStringList axes = {QStringLiteral("roll"), QStringLiteral("pitch"), QStringLiteral("yaw"), QStringLiteral("level")};
    const QStringList terms = {QStringLiteral("p"), QStringLiteral("i"), QStringLiteral("d"), QStringLiteral("f")};
    const int defaults[4][4] = {
        {42, 85, 30, 90},
        {46, 90, 32, 95},
        {45, 90, 0, 90},
        {55, 0, 0, 0},
    };
    for (int row = 0; row < axes.size(); ++row) {
        for (int col = 0; col < terms.size(); ++col) {
            m_pid[row][col]->setValue(m_config.intValue(QStringLiteral("pid_%1_%2").arg(axes.at(row), terms.at(col)), defaults[row][col]));
        }
    }
    setCombo(m_dtermLpfType, m_config.value(QStringLiteral("pid_dterm_lpf_type"), QStringLiteral("PT1")));
    m_dtermLpfFreq->setValue(m_config.intValue(QStringLiteral("pid_dterm_lpf_freq"), 100));
    setCombo(m_dtermLpf2Type, m_config.value(QStringLiteral("pid_dterm_lpf2_type"), QStringLiteral("PT1")));
    m_dtermLpf2Freq->setValue(m_config.intValue(QStringLiteral("pid_dterm_lpf2_freq"), 100));
    m_dtermNotchFreq->setValue(m_config.intValue(QStringLiteral("pid_dterm_notch_freq"), 0));
    m_dtermNotchCutoff->setValue(m_config.intValue(QStringLiteral("pid_dterm_notch_cutoff"), 0));
    m_dtermDynMin->setValue(m_config.intValue(QStringLiteral("pid_dterm_dyn_lpf_min"), 0));
    m_dtermDynMax->setValue(m_config.intValue(QStringLiteral("pid_dterm_dyn_lpf_max"), 0));
    m_itermLimit->setValue(m_config.intValue(QStringLiteral("pid_iterm_limit"), 30));
    setCombo(m_itermZero, m_config.value(QStringLiteral("pid_iterm_zero"), QStringLiteral("1")));
    setCombo(m_itermRelax, m_config.value(QStringLiteral("pid_iterm_relax"), QStringLiteral("RP")));
    m_itermRelaxCutoff->setValue(m_config.intValue(QStringLiteral("pid_iterm_relax_cutoff"), 15));
    m_tpaScale->setValue(m_config.intValue(QStringLiteral("pid_tpa_scale"), 10));
    m_tpaBreakpoint->setValue(m_config.intValue(QStringLiteral("pid_tpa_breakpoint"), 1650));
}

void MainWindow::updatePidValidation()
{
    if (!m_pidValidation) {
        return;
    }
    QStringList checks;
    auto mark = [](bool ok, const QString &label) {
        return QStringLiteral("%1 %2").arg(ok ? QStringLiteral("OK:") : QStringLiteral("WARN:"), label);
    };
    const int rollD = m_config.intValue(QStringLiteral("pid_roll_d"), m_pid[0][2] ? m_pid[0][2]->value() : 0);
    const int pitchD = m_config.intValue(QStringLiteral("pid_pitch_d"), m_pid[1][2] ? m_pid[1][2]->value() : 0);
    const int yawD = m_config.intValue(QStringLiteral("pid_yaw_d"), m_pid[2][2] ? m_pid[2][2]->value() : 0);
    const int dtermFreq = m_config.intValue(QStringLiteral("pid_dterm_lpf_freq"), m_dtermLpfFreq ? m_dtermLpfFreq->value() : 0);
    const bool anyMainPid = m_config.intValue(QStringLiteral("pid_roll_p"), m_pid[0][0] ? m_pid[0][0]->value() : 0) > 0
        || m_config.intValue(QStringLiteral("pid_pitch_p"), m_pid[1][0] ? m_pid[1][0]->value() : 0) > 0
        || m_config.intValue(QStringLiteral("pid_yaw_p"), m_pid[2][0] ? m_pid[2][0]->value() : 0) > 0;
    checks << mark(anyMainPid, QStringLiteral("main axis P gains are non-zero"));
    checks << mark(rollD <= 80 && pitchD <= 80, QStringLiteral("roll/pitch D values are in a conservative range"));
    checks << mark(yawD <= 30, QStringLiteral("yaw D is low or zero"));
    checks << mark(dtermFreq == 0 || dtermFreq >= 50, QStringLiteral("D-term LPF cutoff is not extremely low"));
    checks << QStringLiteral("INFO: Beginner Soft changes rates/expo/limits, not PID gains.");
    m_pidValidation->setText(checks.join(QStringLiteral("\n")));
}

void MainWindow::updateFiltersUi()
{
    if (!m_filterGyroLpfType) {
        return;
    }
    auto setCombo = [](QComboBox *combo, const QString &value) {
        const int index = combo->findText(value);
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
    };

    setCombo(m_filterGyroLpfType, m_config.value(QStringLiteral("gyro_lpf_type"), QStringLiteral("PT1")));
    m_filterGyroLpfFreq->setValue(m_config.intValue(QStringLiteral("gyro_lpf_freq"), 100));
    setCombo(m_filterGyroLpf2Type, m_config.value(QStringLiteral("gyro_lpf2_type"), QStringLiteral("PT1")));
    m_filterGyroLpf2Freq->setValue(m_config.intValue(QStringLiteral("gyro_lpf2_freq"), 213));
    setCombo(m_filterGyroLpf3Type, m_config.value(QStringLiteral("gyro_lpf3_type"), QStringLiteral("FO")));
    m_filterGyroLpf3Freq->setValue(m_config.intValue(QStringLiteral("gyro_lpf3_freq"), 150));
    m_filterGyroNotch1Freq->setValue(m_config.intValue(QStringLiteral("gyro_notch1_freq"), 0));
    m_filterGyroNotch1Cutoff->setValue(m_config.intValue(QStringLiteral("gyro_notch1_cutoff"), 0));
    m_filterGyroNotch2Freq->setValue(m_config.intValue(QStringLiteral("gyro_notch2_freq"), 0));
    m_filterGyroNotch2Cutoff->setValue(m_config.intValue(QStringLiteral("gyro_notch2_cutoff"), 0));
    m_filterDynNotch->setChecked(m_config.boolValue(QStringLiteral("feature_dyn_notch"), false));
    m_filterGyroDynLpfMin->setValue(m_config.intValue(QStringLiteral("gyro_dyn_lpf_min"), 170));
    m_filterGyroDynLpfMax->setValue(m_config.intValue(QStringLiteral("gyro_dyn_lpf_max"), 425));
    m_filterGyroDynNotchQ->setValue(m_config.intValue(QStringLiteral("gyro_dyn_notch_q"), 300));
    m_filterGyroDynNotchCount->setValue(m_config.intValue(QStringLiteral("gyro_dyn_notch_count"), 4));
    m_filterGyroDynNotchMin->setValue(m_config.intValue(QStringLiteral("gyro_dyn_notch_min"), 80));
    m_filterGyroDynNotchMax->setValue(m_config.intValue(QStringLiteral("gyro_dyn_notch_max"), 400));
    setCombo(m_filterDtermLpfType, m_config.value(QStringLiteral("pid_dterm_lpf_type"), QStringLiteral("PT1")));
    m_filterDtermLpfFreq->setValue(m_config.intValue(QStringLiteral("pid_dterm_lpf_freq"), 100));
    setCombo(m_filterDtermLpf2Type, m_config.value(QStringLiteral("pid_dterm_lpf2_type"), QStringLiteral("PT1")));
    m_filterDtermLpf2Freq->setValue(m_config.intValue(QStringLiteral("pid_dterm_lpf2_freq"), 100));
    m_filterDtermNotchFreq->setValue(m_config.intValue(QStringLiteral("pid_dterm_notch_freq"), 0));
    m_filterDtermNotchCutoff->setValue(m_config.intValue(QStringLiteral("pid_dterm_notch_cutoff"), 0));
    m_filterDtermDynLpfMin->setValue(m_config.intValue(QStringLiteral("pid_dterm_dyn_lpf_min"), 0));
    m_filterDtermDynLpfMax->setValue(m_config.intValue(QStringLiteral("pid_dterm_dyn_lpf_max"), 0));
}

void MainWindow::updateFilterValidation()
{
    if (!m_filterValidation) {
        return;
    }
    QStringList checks;
    auto mark = [](bool ok, const QString &label) {
        return QStringLiteral("%1 %2").arg(ok ? QStringLiteral("OK:") : QStringLiteral("WARN:"), label);
    };
    auto notchValid = [](int freq, int cutoff) {
        if (freq == 0 && cutoff == 0) {
            return true;
        }
        return freq > 0 && cutoff > 0 && cutoff < freq;
    };

    checks << mark(m_config.intValue(QStringLiteral("gyro_lpf_freq"), 100) >= 40, QStringLiteral("gyro LPF cutoff is not extremely low"));
    const int gyroLpf2 = m_config.intValue(QStringLiteral("gyro_lpf2_freq"), 213);
    checks << mark(gyroLpf2 == 0 || gyroLpf2 >= 40, QStringLiteral("gyro LPF2 cutoff is disabled or not extremely low"));
    const int dtermLpf = m_config.intValue(QStringLiteral("pid_dterm_lpf_freq"), 100);
    checks << mark(dtermLpf == 0 || dtermLpf >= 50, QStringLiteral("D-term LPF cutoff is disabled or not extremely low"));
    checks << mark(notchValid(m_config.intValue(QStringLiteral("gyro_notch1_freq"), 0), m_config.intValue(QStringLiteral("gyro_notch1_cutoff"), 0)), QStringLiteral("gyro notch 1 cutoff is below frequency"));
    checks << mark(notchValid(m_config.intValue(QStringLiteral("gyro_notch2_freq"), 0), m_config.intValue(QStringLiteral("gyro_notch2_cutoff"), 0)), QStringLiteral("gyro notch 2 cutoff is below frequency"));
    checks << mark(notchValid(m_config.intValue(QStringLiteral("pid_dterm_notch_freq"), 0), m_config.intValue(QStringLiteral("pid_dterm_notch_cutoff"), 0)), QStringLiteral("D-term notch cutoff is below frequency"));
    checks << mark(m_config.intValue(QStringLiteral("gyro_dyn_lpf_min"), 170) <= m_config.intValue(QStringLiteral("gyro_dyn_lpf_max"), 425), QStringLiteral("dynamic gyro LPF min is below max"));
    if (m_config.boolValue(QStringLiteral("feature_dyn_notch"), false)) {
        const int count = m_config.intValue(QStringLiteral("gyro_dyn_notch_count"), 4);
        checks << mark(count >= 1 && count <= 4, QStringLiteral("dynamic notch count is in supported range"));
        checks << mark(m_config.intValue(QStringLiteral("gyro_dyn_notch_min"), 80) <= m_config.intValue(QStringLiteral("gyro_dyn_notch_max"), 400), QStringLiteral("dynamic notch min is below max"));
    }
    checks << QStringLiteral("INFO: lower cutoff means stronger filtering and more delay.");
    m_filterValidation->setText(checks.join(QStringLiteral("\n")));
}

void MainWindow::updateRatesUi()
{
    if (!m_rateType) {
        return;
    }
    auto setCombo = [](QComboBox *combo, const QString &value) {
        const int index = combo->findText(value);
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
    };

    setCombo(m_rateType, m_config.value(QStringLiteral("input_rate_type"), QStringLiteral("ACTUAL")));

    const QStringList axes = {QStringLiteral("roll"), QStringLiteral("pitch"), QStringLiteral("yaw")};
    const QStringList fields = {QStringLiteral("rate"), QStringLiteral("srate"), QStringLiteral("expo"), QStringLiteral("limit")};
    const int defaults[3][4] = {
        {20, 40, 0, 1998},
        {20, 40, 0, 1998},
        {30, 36, 0, 1998},
    };
    for (int row = 0; row < axes.size(); ++row) {
        for (int col = 0; col < fields.size(); ++col) {
            m_rate[row][col]->setValue(m_config.intValue(QStringLiteral("input_%1_%2").arg(axes.at(row), fields.at(col)), defaults[row][col]));
        }
    }

    m_inputDeadband->setValue(m_config.intValue(QStringLiteral("input_deadband"), 3));
    setCombo(m_inputLpfType, m_config.value(QStringLiteral("input_lpf_type"), QStringLiteral("PT3")));
    m_inputLpfFreq->setValue(m_config.intValue(QStringLiteral("input_lpf_freq"), 0));
    m_inputLpfFactor->setValue(m_config.intValue(QStringLiteral("input_lpf_factor"), 50));
    setCombo(m_inputFfLpfType, m_config.value(QStringLiteral("input_ff_lpf_type"), QStringLiteral("PT3")));
    m_inputFfLpfFreq->setValue(m_config.intValue(QStringLiteral("input_ff_lpf_freq"), 0));
    setCombo(m_rateThrottleLimitType, m_config.value(QStringLiteral("mixer_throttle_limit_type"), QStringLiteral("NONE")));
    m_rateThrottleLimitPercent->setValue(m_config.intValue(QStringLiteral("mixer_throttle_limit_percent"), 100));
    m_rateMinThrottle->setValue(m_config.intValue(QStringLiteral("output_min_throttle"), 1070));
}

void MainWindow::updateRateValidation()
{
    if (!m_rateValidation) {
        return;
    }
    QStringList checks;
    auto mark = [](bool ok, const QString &label) {
        return QStringLiteral("%1 %2").arg(ok ? QStringLiteral("OK:") : QStringLiteral("WARN:"), label);
    };

    const QStringList axes = {QStringLiteral("roll"), QStringLiteral("pitch"), QStringLiteral("yaw")};
    bool axesHaveCommand = false;
    for (int row = 0; row < axes.size(); ++row) {
        const QString axis = axes.at(row);
        const int rate = m_config.intValue(QStringLiteral("input_%1_rate").arg(axis), m_rate[row][0] ? m_rate[row][0]->value() : 0);
        const int superRate = m_config.intValue(QStringLiteral("input_%1_srate").arg(axis), m_rate[row][1] ? m_rate[row][1]->value() : 0);
        const int expo = m_config.intValue(QStringLiteral("input_%1_expo").arg(axis), m_rate[row][2] ? m_rate[row][2]->value() : 0);
        const int limit = m_config.intValue(QStringLiteral("input_%1_limit").arg(axis), m_rate[row][3] ? m_rate[row][3]->value() : 0);
        axesHaveCommand = axesHaveCommand || rate > 0 || superRate > 0;
        checks << mark(limit >= 150, QStringLiteral("%1 limit allows usable authority").arg(axis));
        checks << mark(expo <= 60, QStringLiteral("%1 expo is not excessive").arg(axis));
    }

    checks << mark(axesHaveCommand, QStringLiteral("at least one rate/super-rate value is non-zero"));
    checks << mark(m_config.intValue(QStringLiteral("input_deadband"), m_inputDeadband ? m_inputDeadband->value() : 0) <= 10, QStringLiteral("input deadband is not too wide"));
    const QString throttleLimitType = m_config.value(QStringLiteral("mixer_throttle_limit_type"), m_rateThrottleLimitType ? m_rateThrottleLimitType->currentText() : QStringLiteral("NONE"));
    checks << mark(throttleLimitType != QStringLiteral("CLIP"), QStringLiteral("throttle limit uses smooth SCALE/NONE, not abrupt CLIP"));
    checks << mark(m_config.intValue(QStringLiteral("mixer_throttle_limit_percent"), m_rateThrottleLimitPercent ? m_rateThrottleLimitPercent->value() : 100) >= 50, QStringLiteral("throttle limit remains above 50%"));
    checks << QStringLiteral("INFO: ESP-FC has no throttle expo; use an EdgeTX throttle curve for detailed shaping.");
    m_rateValidation->setText(checks.join(QStringLiteral("\n")));
}

void MainWindow::updateBlackboxUi()
{
    if (!m_blackboxDev) return;
    auto setCombo = [](QComboBox *combo, const QString &value) {
        const int index = combo->findText(value);
        if (index >= 0) combo->setCurrentIndex(index);
    };
    static const QStringList fields = {
        QStringLiteral("blackbox_log_acc"), QStringLiteral("blackbox_log_alt"), QStringLiteral("blackbox_log_bat"), QStringLiteral("blackbox_log_debug"),
        QStringLiteral("blackbox_log_gps"), QStringLiteral("blackbox_log_gyro"), QStringLiteral("blackbox_log_gyro_raw"), QStringLiteral("blackbox_log_mag"),
        QStringLiteral("blackbox_log_motor"), QStringLiteral("blackbox_log_pid"), QStringLiteral("blackbox_log_rc"), QStringLiteral("blackbox_log_rpm"),
        QStringLiteral("blackbox_log_rssi"), QStringLiteral("blackbox_log_sp"),
    };
    setCombo(m_blackboxDev, m_config.value(QStringLiteral("blackbox_dev"), QStringLiteral("NONE")));
    setCombo(m_blackboxMode, m_config.value(QStringLiteral("blackbox_mode"), QStringLiteral("NORMAL")));
    m_blackboxRate->setValue(m_config.intValue(QStringLiteral("blackbox_rate"), 32));
    for (int i = 0; i < fields.size(); ++i) {
        if (m_blackboxFields[i]) m_blackboxFields[i]->setChecked(m_config.boolValue(fields.at(i), true));
    }
    QString selectedPort = QStringLiteral("None");
    const QStringList ports = {QStringLiteral("serial_0"), QStringLiteral("serial_1"), QStringLiteral("serial_2"), QStringLiteral("serial_soft_0"), QStringLiteral("serial_usb")};
    for (const QString &port : ports) {
        bool ok = false;
        const int mask = m_config.values(port).value(0).toInt(&ok);
        if (ok && (mask & 128)) { selectedPort = port; break; }
    }
    setCombo(m_blackboxSerialPort, selectedPort);
    m_blackboxStorage->setText(QStringLiteral("Storage status not queried in this first pass."));
}

void MainWindow::updateBlackboxValidation()
{
    if (!m_blackboxValidation) return;
    static const QStringList fields = {
        QStringLiteral("blackbox_log_acc"), QStringLiteral("blackbox_log_alt"), QStringLiteral("blackbox_log_bat"), QStringLiteral("blackbox_log_debug"),
        QStringLiteral("blackbox_log_gps"), QStringLiteral("blackbox_log_gyro"), QStringLiteral("blackbox_log_gyro_raw"), QStringLiteral("blackbox_log_mag"),
        QStringLiteral("blackbox_log_motor"), QStringLiteral("blackbox_log_pid"), QStringLiteral("blackbox_log_rc"), QStringLiteral("blackbox_log_rpm"),
        QStringLiteral("blackbox_log_rssi"), QStringLiteral("blackbox_log_sp"),
    };
    QStringList checks;
    auto mark = [](bool ok, const QString &label) { return QStringLiteral("%1 %2").arg(ok ? QStringLiteral("OK:") : QStringLiteral("WARN:"), label); };
    const QString dev = m_config.value(QStringLiteral("blackbox_dev"), m_blackboxDev ? m_blackboxDev->currentText() : QStringLiteral("NONE"));
    const int rate = m_config.intValue(QStringLiteral("blackbox_rate"), m_blackboxRate ? m_blackboxRate->value() : 32);
    int enabledFields = 0;
    for (int i = 0; i < fields.size(); ++i) enabledFields += m_config.boolValue(fields.at(i), m_blackboxFields[i] && m_blackboxFields[i]->isChecked()) ? 1 : 0;
    checks << mark(dev != QStringLiteral("NONE"), dev == QStringLiteral("NONE") ? QStringLiteral("blackbox logging is disabled") : QStringLiteral("blackbox device is selected"));
    checks << mark(dev == QStringLiteral("NONE") || enabledFields > 0, QStringLiteral("at least one blackbox field is enabled"));
    checks << mark(rate >= 8, QStringLiteral("blackbox rate is not extremely high"));
    checks << mark(!(enabledFields >= 12 && rate < 16), QStringLiteral("full/heavy logging is not combined with a very high rate"));
    const QStringList ports = {QStringLiteral("serial_0"), QStringLiteral("serial_1"), QStringLiteral("serial_2"), QStringLiteral("serial_soft_0"), QStringLiteral("serial_usb")};
    bool hasBlackboxSerial = false;
    bool conflictsRx = false;
    for (const QString &port : ports) {
        bool ok = false;
        const int mask = m_config.values(port).value(0).toInt(&ok);
        if (ok && (mask & 128)) { hasBlackboxSerial = true; conflictsRx = conflictsRx || (mask & 64); }
    }
    if (dev == QStringLiteral("SERIAL")) {
        checks << mark(hasBlackboxSerial, QStringLiteral("one serial port has BLACKBOX function 128"));
        checks << mark(!conflictsRx, QStringLiteral("BLACKBOX is not sharing a port with RX_SERIAL"));
    }
    if (dev == QStringLiteral("FLASH") || dev == QStringLiteral("SDCARD")) checks << QStringLiteral("INFO: storage summary/download are deferred until safe MSP storage handling is added.");
    checks << QStringLiteral("INFO: erase/download actions are intentionally not implemented in this first pass.");
    m_blackboxValidation->setText(checks.join(QStringLiteral("\n")));
}

void MainWindow::updateSensorUi()
{
    if (m_attitudeView) {
        m_attitudeView->setAttitude(m_attitude);
    }
    if (m_attitudeLabel) {
        m_attitudeLabel->setText(m_attitude.valid
            ? QStringLiteral("roll %1 deg, pitch %2 deg, yaw %3 deg")
                .arg(m_attitude.rollDeg, 0, 'f', 1)
                .arg(m_attitude.pitchDeg, 0, 'f', 1)
                .arg(m_attitude.yawDeg, 0, 'f', 0)
            : QStringLiteral("No MSP_ATTITUDE data"));
    }
    auto axisText = [](const qint16 values[3]) {
        return QStringLiteral("X %1, Y %2, Z %3").arg(values[0]).arg(values[1]).arg(values[2]);
    };
    if (m_accelLabel) {
        m_accelLabel->setText(m_rawImu.valid ? axisText(m_rawImu.acc) : QStringLiteral("No MSP_RAW_IMU data"));
    }
    if (m_gyroLabel) {
        m_gyroLabel->setText(m_rawImu.valid ? axisText(m_rawImu.gyro) : QStringLiteral("No MSP_RAW_IMU data"));
    }
    if (m_magLabel) {
        m_magLabel->setText(m_rawImu.valid ? axisText(m_rawImu.mag) : QStringLiteral("No MSP_RAW_IMU data"));
    }
}

void MainWindow::syncGyroAlignUiFromConfig()
{
    if (m_gyroAlign) {
        const QString align = m_config.value(QStringLiteral("gyro_align"), QStringLiteral("DEFAULT"));
        const int index = m_gyroAlign->findText(align);
        if (index >= 0) {
            m_gyroAlign->setCurrentIndex(index);
        }
    }
}

void MainWindow::captureCliText(const QString &text)
{
    if (!m_pendingParamName.isEmpty()) {
        m_pendingParamCapture += text;
        const QRegularExpression matchSet(QStringLiteral("\\bset\\s+(%1)\\s+(?:=\\s*)?([^\\s#>]+)")
            .arg(QRegularExpression::escape(m_pendingParamName)));
        const QRegularExpressionMatch match = matchSet.match(m_pendingParamCapture);
        if (match.hasMatch()) {
            const QString name = match.captured(1);
            const QStringList values{match.captured(2).trimmed()};
            m_config.setClean(name, values);
            appendLog(QStringLiteral("Refreshed %1: %2").arg(name, values.join(QStringLiteral(" "))));
            m_pendingParamName.clear();
            m_pendingParamCapture.clear();
            updateConfigUi();
        } else if (m_pendingParamCapture.size() > 256 || m_pendingParamCapture.contains(QStringLiteral("# "))) {
            appendLog(QStringLiteral("Waiting for %1; captured CLI: %2")
                .arg(m_pendingParamName, m_pendingParamCapture.simplified().left(220)));
        }
    }
    if (!m_captureDump) {
        return;
    }
    m_cliCapture += text;
    static const QRegularExpression dumpEnd(QStringLiteral("(^|\\n|\\r)save(\\r?\\n|$)"));
    if (!dumpEnd.match(m_cliCapture).hasMatch()) {
        return;
    }
    m_captureDump = false;
    const int count = m_config.loadFromDump(m_cliCapture);
    appendLog(QStringLiteral("Loaded %1 config params from dump").arg(count));
    updateConfigUi();
}

QString MainWindow::armingFlagsText(quint32 flags)
{
    if (flags == 0) {
        return QStringLiteral("READY");
    }
    QStringList names;
    for (qsizetype i = 0; i < static_cast<qsizetype>(ArmingFlagNames.size()); ++i) {
        if (flags & (1u << i)) {
            names << QString::fromLatin1(ArmingFlagNames[static_cast<size_t>(i)]);
        }
    }
    const quint32 knownMask = (1u << ArmingFlagNames.size()) - 1u;
    const quint32 unknown = flags & ~knownMask;
    if (unknown) {
        names << QStringLiteral("UNKNOWN(0x%1)").arg(unknown, 8, 16, QLatin1Char('0'));
    }
    return names.join(QStringLiteral(", "));
}
