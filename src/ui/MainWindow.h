#pragma once

#include "model/AppState.h"
#include "model/FcConfigStore.h"
#include "model/FirmwareInfo.h"
#include "model/RxConfig.h"
#include "model/SensorData.h"
#include "protocol/CliSession.h"
#include "protocol/MspClient.h"
#include "transport/SerialTransport.h"
#include "transport/TcpTransport.h"
#include "ui/AttitudeView.h"

#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>

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
    void requestHandshake();
    void requestRxConfig();
    void applyRxProvider();
    void saveToFc();
    void writeSaveReboot();
    void loadConfigDump();
    void applyConfigChanges();
    void rebootFc();
    void applyMotorsFromUi();
    void applyMotorPreset();
    void stagePidFields();
    void applyPidBeginnerPreset();
    void stageFilterFields();
    void applyFilterDefaultPreset();
    void applyFilterMorePreset();
    void applyFilterStrongPreset();
    void applyFilterLowLatencyPreset();
    void stageRateFields();
    void applyRateBeginnerPreset();
    void applyRateNoFlipsPreset();
    void applyRateAcroNormalPreset();
    void applySmoothThrottlePreset();
    void stageBlackboxFields();
    void applyBlackboxOffPreset();
    void applyBlackboxBasicPreset();
    void applyBlackboxDebugPreset();
    void applyBlackboxFullPreset();
    void applyBlackboxSerialPreset();
    void startSensorPolling();
    void stopSensorPolling();
    void requestSensorFrame();
    void refreshGyroAlign();
    void stageGyroAlign();
    void calibrateGyro();
    void handleMspMessage(const MspMessage &message);
    void appendLog(const QString &message);
    void setConnectedUi(bool connected);

private:
    QWidget *buildConnectionBar();
    QWidget *buildDashboardTab();
    QWidget *buildSetupTab();
    QWidget *buildReceiverTab();
    QWidget *buildMotorsTab();
    QWidget *buildPidTab();
    QWidget *buildFiltersTab();
    QWidget *buildRatesTab();
    QWidget *buildBlackboxTab();
    QWidget *buildSensorsTab();
    QWidget *buildCliTab();
    QWidget *buildPlaceholderTab(const QString &title, const QStringList &items);
    void setTransport(ITransport *transport);
    void sendMsp(quint16 command, const QByteArray &payload = {});
    void updateFirmwareLabel();
    void updateRxUi();
    void updateConfigUi();
    void updateMotorsUi();
    void updateMotorValidation();
    void updatePidUi();
    void updatePidValidation();
    void updateFiltersUi();
    void updateFilterValidation();
    void updateRatesUi();
    void updateRateValidation();
    void updateBlackboxUi();
    void updateBlackboxValidation();
    void updateSensorUi();
    void syncGyroAlignUiFromConfig();
    void captureCliText(const QString &text);
    void sendCliWhenReady(const QStringList &commands);
    void flushPendingCliCommands();
    static QString armingFlagsText(quint32 flags);

    AppState m_state;
    SerialTransport m_serial;
    TcpTransport m_tcp;
    ITransport *m_transport = nullptr;
    MspClient m_msp;
    CliSession m_cli;
    FirmwareInfo m_firmware;
    RxConfig m_rxConfig;
    AttitudeState m_attitude;
    RawImuState m_rawImu;
    FcConfigStore m_config;
    QTimer m_sensorPollTimer;
    QString m_cliCapture;
    QStringList m_pendingCliCommands;
    QString m_pendingParamCapture;
    QString m_pendingParamName;
    bool m_captureDump = false;

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
    QLabel *m_configSummary = nullptr;
    QLabel *m_setupValidation = nullptr;
    QPushButton *m_applyConfig = nullptr;
    QComboBox *m_motorProtocol = nullptr;
    QSpinBox *m_motorRate = nullptr;
    QSpinBox *m_minCommand = nullptr;
    QSpinBox *m_minThrottle = nullptr;
    QSpinBox *m_maxThrottle = nullptr;
    QSpinBox *m_outputLimit = nullptr;
    QComboBox *m_throttleLimitType = nullptr;
    QSpinBox *m_throttleLimitPercent = nullptr;
    QSpinBox *m_motorPins[4] = {};
    QLabel *m_motorValidation = nullptr;
    QSpinBox *m_pid[4][4] = {};
    QComboBox *m_dtermLpfType = nullptr;
    QSpinBox *m_dtermLpfFreq = nullptr;
    QComboBox *m_dtermLpf2Type = nullptr;
    QSpinBox *m_dtermLpf2Freq = nullptr;
    QSpinBox *m_dtermNotchFreq = nullptr;
    QSpinBox *m_dtermNotchCutoff = nullptr;
    QSpinBox *m_dtermDynMin = nullptr;
    QSpinBox *m_dtermDynMax = nullptr;
    QSpinBox *m_itermLimit = nullptr;
    QComboBox *m_itermZero = nullptr;
    QComboBox *m_itermRelax = nullptr;
    QSpinBox *m_itermRelaxCutoff = nullptr;
    QSpinBox *m_tpaScale = nullptr;
    QSpinBox *m_tpaBreakpoint = nullptr;
    QLabel *m_pidValidation = nullptr;
    QComboBox *m_filterGyroLpfType = nullptr;
    QSpinBox *m_filterGyroLpfFreq = nullptr;
    QComboBox *m_filterGyroLpf2Type = nullptr;
    QSpinBox *m_filterGyroLpf2Freq = nullptr;
    QComboBox *m_filterGyroLpf3Type = nullptr;
    QSpinBox *m_filterGyroLpf3Freq = nullptr;
    QSpinBox *m_filterGyroNotch1Freq = nullptr;
    QSpinBox *m_filterGyroNotch1Cutoff = nullptr;
    QSpinBox *m_filterGyroNotch2Freq = nullptr;
    QSpinBox *m_filterGyroNotch2Cutoff = nullptr;
    QCheckBox *m_filterDynNotch = nullptr;
    QSpinBox *m_filterGyroDynLpfMin = nullptr;
    QSpinBox *m_filterGyroDynLpfMax = nullptr;
    QSpinBox *m_filterGyroDynNotchQ = nullptr;
    QSpinBox *m_filterGyroDynNotchCount = nullptr;
    QSpinBox *m_filterGyroDynNotchMin = nullptr;
    QSpinBox *m_filterGyroDynNotchMax = nullptr;
    QComboBox *m_filterDtermLpfType = nullptr;
    QSpinBox *m_filterDtermLpfFreq = nullptr;
    QComboBox *m_filterDtermLpf2Type = nullptr;
    QSpinBox *m_filterDtermLpf2Freq = nullptr;
    QSpinBox *m_filterDtermNotchFreq = nullptr;
    QSpinBox *m_filterDtermNotchCutoff = nullptr;
    QSpinBox *m_filterDtermDynLpfMin = nullptr;
    QSpinBox *m_filterDtermDynLpfMax = nullptr;
    QLabel *m_filterValidation = nullptr;
    QComboBox *m_rateType = nullptr;
    QSpinBox *m_rate[3][4] = {};
    QSpinBox *m_inputDeadband = nullptr;
    QComboBox *m_inputLpfType = nullptr;
    QSpinBox *m_inputLpfFreq = nullptr;
    QSpinBox *m_inputLpfFactor = nullptr;
    QComboBox *m_inputFfLpfType = nullptr;
    QSpinBox *m_inputFfLpfFreq = nullptr;
    QComboBox *m_rateThrottleLimitType = nullptr;
    QSpinBox *m_rateThrottleLimitPercent = nullptr;
    QSpinBox *m_rateMinThrottle = nullptr;
    QLabel *m_rateValidation = nullptr;
    QComboBox *m_blackboxDev = nullptr;
    QComboBox *m_blackboxMode = nullptr;
    QSpinBox *m_blackboxRate = nullptr;
    QCheckBox *m_blackboxFields[14] = {};
    QComboBox *m_blackboxSerialPort = nullptr;
    QLabel *m_blackboxStorage = nullptr;
    QLabel *m_blackboxValidation = nullptr;
    AttitudeView *m_attitudeView = nullptr;
    QLabel *m_attitudeLabel = nullptr;
    QLabel *m_accelLabel = nullptr;
    QLabel *m_gyroLabel = nullptr;
    QLabel *m_magLabel = nullptr;
    QLabel *m_sensorLiveLabel = nullptr;
    QComboBox *m_gyroAlign = nullptr;
    QComboBox *m_rxProvider = nullptr;
    QLabel *m_rxSummary = nullptr;
    QPushButton *m_rxApply = nullptr;
    QPlainTextEdit *m_cliOutput = nullptr;
    QLineEdit *m_cliInput = nullptr;
};
