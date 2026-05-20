#ifndef MAINWINDOW_H
#define MAINWINDOW_H
/*========================================================================================*/
#include <QMainWindow>
#include <QSerialPort>
#include <QGridLayout>
#include <QVector>
#include <QFrame>
#include <QLabel>
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QRadioButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QRect>
#include <QEvent>

#include <QtCharts>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QChart>

#include "serialreceiver.h"
#include "sensorstore.h"
#include "heatmapwidget.h"
#include "summarydata.h"        // برای دریافت داده Summary از SerialReceiver
#include "bedframestore.h"      // برای نگهداری داده‌های BED_SNAPSHOT و BED_STATUS
#include "trendhistorystore.h"
#include "bodydetector.h"
#include "bodyzones.h"
#include "bodyzoneanalyzer.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
/*========================================================================================*/

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    /* ====================== Core Data Pipeline ====================== */
    QSerialPort m_port;
    SerialReceiver m_rx;
    SensorStore m_store;
    BedFrameStore m_bedStore;           // storage برای packetهای 0x20 و 0x22
    TrendHistoryStore m_trendHistory;   // history برای trend/analytics

    /* ====================== Main Heatmap / Data UI ====================== */
    HeatmapWidget *m_heatmap = nullptr;
    QGridLayout *m_nodeSummaryLayout = nullptr;
    QVector<QFrame*> m_nodeCards;
    QVector<QLabel*> m_nodeTitles;
    QVector<QLabel*> m_nodeSubs;
    int m_selectedNode = 0;

    /* ====================== Home Page ====================== */
    QWidget *m_homeContentHost = nullptr;
    QLabel *m_lblHomeTitle = nullptr;

    QFrame *m_cardHeatmap = nullptr;
    QFrame *m_cardData = nullptr;
    QFrame *m_cardSettings = nullptr;

    /* ====================== Settings Page ====================== */
    QWidget *m_settingsContentHost = nullptr;
    QComboBox *m_cmbPortSettings = nullptr;
    QPushButton *m_btnRefreshSettings = nullptr;
    QPushButton *m_btnConnectSettings = nullptr;
    QLabel *m_lblStatusSettings = nullptr;

    QSpinBox *m_spHighSettings = nullptr;
    QSpinBox *m_spNoSettings = nullptr;

    /* ====================== Debug Page ====================== */
    QWidget *m_tabDebug = nullptr;
    QWidget *m_debugContentHost = nullptr;
    QLabel *m_lblDebugTitle = nullptr;

    QCheckBox *m_chkShowDebugText = nullptr;
    QCheckBox *m_chkShowTooltip = nullptr;
    QCheckBox *m_chkShowBodyBounds = nullptr;
    QCheckBox *m_chkShowBodyZones = nullptr;
    QCheckBox *m_chkShowZoneValues = nullptr;

    // ===== Analytics Page =====
    QWidget *m_tabAnalytics = nullptr;          // تب مستقل Analytics
    QWidget *m_analyticsContentHost = nullptr;  // کانتینر اصلی محتوای Analytics
    QLabel *m_lblAnalyticsTitle = nullptr;      // عنوان صفحه Analytics

    // ===== Analytics Clinical Summary =====
    QWidget *m_analyticsSummaryRow = nullptr;          // ردیف summary بالای نمودارها
    QLabel *m_lblClinicalLastMove = nullptr;          // زمان آخرین حرکت
    QLabel *m_lblClinicalTopZone = nullptr;           // پرریسک‌ترین ناحیه
    QLabel *m_lblClinicalMaxExposure = nullptr;       // بیشترین زمان exposure
    QLabel *m_lblClinicalAction = nullptr;            // اقدام پیشنهادی

    // ===== Dashboard Zone Stats Mode =====
    QGroupBox *m_grpZoneStatsMode = nullptr;
    QRadioButton *m_radZoneStatsDevice = nullptr;
    QRadioButton *m_radZoneStatsAdaptive = nullptr;
    QRadioButton *m_radZoneStatsCompare = nullptr;

    // ===== Debug Data Panel =====
    QGroupBox *m_grpDebugData = nullptr;
    QLabel *m_lblDbgFrame = nullptr;
    QLabel *m_lblDbgSync = nullptr;
    QLabel *m_lblDbgSource = nullptr;
    QLabel *m_lblDbgBody = nullptr;
    QLabel *m_lblDbgSacrum = nullptr;
    QLabel *m_lblDbgHeelLeft = nullptr;
    QLabel *m_lblDbgHeelRight = nullptr;

    QRect m_dbgSacrumRect;
    QRect m_dbgLeftHeelRect;
    QRect m_dbgRightHeelRect;

    // ======================================================
    // Main Board Debug Console
    //
    // UI controls for:
    // - UART protocol simulation
    // - intervention workflow testing
    // - fake packet generation
    // - firmware debug commands
    // - future CAN/motor diagnostics
    //
    // These widgets belong to the Debug page.
    //
    // IMPORTANT:
    // This section is intended for:
    // - development
    // - validation
    // - engineering/service workflows
    //
    // In future production builds these controls
    // can be hidden or restricted.
    // ======================================================

    // Main debug console group container
    QGroupBox *m_grpMainBoardDebug = nullptr;


    // ======================================================
    // Master enable switch for Main Board debug commands.
    //
    // When disabled:
    // UI must NOT send simulation/debug commands
    // to Main Board.
    // ======================================================
    QCheckBox *m_chkEnableMainBoardDebug = nullptr;


    // ======================================================
    // Simulation/debug command buttons.
    //
    // These buttons will later trigger:
    // TYPE = 0x5A DEBUG_COMMAND
    //
    // for intervention and protocol testing.
    // ======================================================
    QPushButton *m_btnDbgTestPlan = nullptr;

    QPushButton *m_btnDbgResultExecuting = nullptr;

    QPushButton *m_btnDbgResultCompleted = nullptr;

    QPushButton *m_btnDbgResultFailed = nullptr;


    // ======================================================
    // Runtime protocol/debug status labels.
    //
    // These labels will later display:
    // - last transmitted debug command
    // - last received simulation packet
    // - current debug state
    // ======================================================
    QLabel *m_lblDbgMainBoardStatus = nullptr;

    QLabel *m_lblDbgLastTx = nullptr;

    QLabel *m_lblDbgLastRx = nullptr;


    /* ====================== Live Monitoring / Dashboard UI ====================== */
    QLabel *m_lblRiskLive = nullptr;
    QLabel *m_lblMovementLive = nullptr;
    QLabel *m_lblFrameSync = nullptr;

    QLabel *m_lblAlerts = nullptr;
    QLabel *m_lblSacrum = nullptr;
    QLabel *m_lblHeelLeft = nullptr;
    QLabel *m_lblHeelRight = nullptr;
    QLabel *m_lblRecommendation = nullptr;

    // ======================================================
    // Intervention Workflow UI
    //
    // Displays Main Board suggested intervention plans.
    //
    // Flow:
    // Main Board -> TYPE 0x51 INTERVENTION_PLAN
    // UI displays pending intervention card
    // User -> Approve / Reject
    // UI -> TYPE 0x52 / 0x53
    //
    // Future:
    // - lifecycle states
    // - execution progress
    // - intervention history
    // - before/after analytics
    // ======================================================

    // Main intervention container
    QGroupBox *m_grpInterventionCard = nullptr;

    // Runtime intervention info labels
    QLabel *m_lblInterventionPlanId = nullptr;
    QLabel *m_lblInterventionTargetZone = nullptr;
    QLabel *m_lblInterventionRisk = nullptr;
    QLabel *m_lblInterventionMotorCount = nullptr;
    QLabel *m_lblInterventionStatus = nullptr;

    // User actions
    QPushButton *m_btnApproveIntervention = nullptr;
    QPushButton *m_btnRejectIntervention = nullptr;

    // Current pending plan cache
    quint16 m_pendingInterventionPlanId = 0;
    bool m_hasPendingIntervention = false;

    /* ====================== Alert Blink State ====================== */
    QTimer *m_alertBlinkTimer = nullptr;   // تایمر blink برای alertهای critical
    bool m_alertBlinkOn = true;            // وضعیت فعلی روشن/خاموش blink
    bool m_alertBlinkActive = false;       // آیا blink باید فعال باشد یا نه
    QString m_currentAlertStyle;           // استایل پایه‌ی alert در حالت visible

    /* ====================== Alert Pulse State ====================== */
    QGraphicsOpacityEffect *m_alertOpacityEffect = nullptr;   // افکت opacity برای pulse
    QPropertyAnimation *m_alertPulseAnim = nullptr;           // انیمیشن نرم کم/زیاد شدن opacity
    /* ====================== Mini Trend Chart ====================== */
    QFrame *m_cardRiskTrendMini = nullptr;
    QChartView *m_riskTrendChartView = nullptr;
    QLineSeries *m_riskTrendSeries = nullptr;

    /* ====================== Summary / Dashboard State ====================== */
    SummaryData m_lastSummary;
    bool m_hasValidSummary = false;
    bool m_useAdaptiveZoneStats = true;   // اگر true باشد، zone cards از analyzer تغذیه می‌شوند
    bool m_compareZoneStats = false;      // اگر true باشد، zone cards مقایسه دو منبع را نشان می‌دهند
    QTimer *m_summaryUiTimer = nullptr;
    qint64 m_lastSummaryRxMs = -1;

    /* ====================== Frame Sync State ====================== */
    struct FrameSyncState
    {
        quint16 snapshotFrameId = 0;
        quint16 statusFrameId = 0;
        quint16 nodeHealthFrameId = 0;
        quint16 summaryFrameId = 0;

        bool hasSnapshot = false;
        bool hasStatus = false;
        bool hasNodeHealth = false;
        bool hasSummary = false;
    };

    FrameSyncState m_frameSync;

    /* ====================== Internal Helpers ====================== */
    void refreshPorts();
    void setConnectedUi(bool connected);
    void renderSummaryToDashboard(const SummaryData &summary);
    void renderAdaptiveZoneStats();
    void renderComparedZoneStats();
    void renderDebugDataPanel();
    void updateMiniRiskTrendChart();
    bool isCurrentFrameSynchronized() const;
    void updateTableNode(int nodeId);
    void markAllNodesState(NodeState state);
    void updateNodeSummary(int nodeId);
    void refreshNodeCardStyles();

    // ===== Analytics Charts =====
    QChartView *m_chartRiskView = nullptr;
    QLineSeries *m_seriesRisk = nullptr;
    QLineSeries *m_seriesRiskThreshold = nullptr;   // خط threshold برای Risk Trend

    QChartView *m_chartZonesView = nullptr;
    QLineSeries *m_seriesSacrum = nullptr;
    QLineSeries *m_seriesHeel = nullptr;
    QLineSeries *m_seriesShoulders = nullptr;

    QChartView *m_chartExposureView = nullptr;
    QBarSeries *m_seriesExposure = nullptr;


private slots:
    void onConnectClicked();
    void onSerialError(QSerialPort::SerialPortError e);
    void onPacket(const NodePacket &pkt);
    void onBedSnapshot(const BedSnapshotPacket &pkt);
    void onBedStatus(const BedStatusPacket &pkt);
    void onNodeHealth(const NodeHealthPacket &pkt);
    void updateLiveMonitoring();
    void onSummaryReceived(const SummaryData &summary);
    // ======================================================
    // Main Board -> UI
    // Called when SerialReceiver decodes TYPE 0x51
    // INTERVENTION_PLAN.
    //
    // This is the first UI-level entry point for
    // intervention workflow.
    // ======================================================
    void onInterventionPlanReceived(const InterventionPlan &plan);
    // ======================================================
    // Main Board -> UI
    // Called when SerialReceiver decodes TYPE 0x54
    // INTERVENTION_RESULT.
    //
    // Updates intervention lifecycle status on UI card.
    // ======================================================
    void onInterventionResultReceived(const InterventionResult &result);
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // MAINWINDOW_H