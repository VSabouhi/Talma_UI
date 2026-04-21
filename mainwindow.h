#ifndef MAINWINDOW_H
#define MAINWINDOW_H
/*========================================================================================*/
#include <QMainWindow>
#include <QSerialPort>
#include "serialreceiver.h"
#include "sensorstore.h"
#include "heatmapwidget.h"
#include <QGridLayout>
#include <QVector>
#include <QFrame>
#include <QLabel>
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include "summarydata.h"  // برای دریافت داده Summary از SerialReceiver
#include "bedframestore.h"   // برای نگهداری داده‌های BED_SNAPSHOT و BED_STATUS
#include "bodydetector.h"
#include "bodyzones.h"
#include "bodyzoneanalyzer.h"
#include <QTimer>   // برای محدود کردن نرخ refresh داشبورد
#include <QRadioButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QRect>
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
    QSerialPort m_port;
    SerialReceiver m_rx;
    SensorStore m_store;
    BedFrameStore m_bedStore;   // storage جدید برای packetهای 0x20 و 0x22
    HeatmapWidget *m_heatmap = nullptr;
    QGridLayout *m_nodeSummaryLayout = nullptr;
    QVector<QFrame*> m_nodeCards;
    QVector<QLabel*> m_nodeTitles;
    QVector<QLabel*> m_nodeSubs;
    int m_selectedNode = 0;


    QWidget *m_homeContentHost = nullptr;
    QLabel *m_lblHomeTitle = nullptr;

    QFrame *m_cardHeatmap = nullptr;
    QFrame *m_cardData = nullptr;
    QFrame *m_cardSettings = nullptr;

    QWidget *m_settingsContentHost = nullptr;

    // ===== Debug Page =====
    QWidget *m_tabDebug = nullptr;                 // NEW: تب مستقل Debug
    QWidget *m_debugContentHost = nullptr;         // NEW: کانتینر اصلی محتوای Debug
    QCheckBox *m_chkShowDebugText = nullptr;       // NEW: نمایش text دیباگ داخل سلول‌ها
    QCheckBox *m_chkShowTooltip = nullptr;         // NEW: نمایش tooltip زیر موس
    QCheckBox *m_chkShowBodyBounds = nullptr;
    QCheckBox *m_chkShowBodyZones = nullptr;
    QCheckBox *m_chkShowZoneValues = nullptr;   // NEW: نمایش مقادیر zoneها روی heatmap
    // ===== Dashboard Zone Stats Mode =====
    QGroupBox *m_grpZoneStatsMode = nullptr;         // NEW: گروه mode انتخاب منبع zone stats
    QRadioButton *m_radZoneStatsDevice = nullptr;    // NEW: فقط summary دستگاه
    QRadioButton *m_radZoneStatsAdaptive = nullptr;  // NEW: فقط adaptive analyzer
    QRadioButton *m_radZoneStatsCompare = nullptr;   // NEW: مقایسه هر دو
    QLabel *m_lblDebugTitle = nullptr;             // NEW: عنوان صفحه Debug
    // ===== Debug Data Panel =====
    QGroupBox *m_grpDebugData = nullptr;          // NEW: پنل داده‌های عددی debug
    QLabel *m_lblDbgFrame = nullptr;              // NEW: frame id
    QLabel *m_lblDbgSync = nullptr;               // NEW: sync status
    QLabel *m_lblDbgSource = nullptr;             // NEW: source mode (Device/Adaptive/Compare)
    QLabel *m_lblDbgBody = nullptr;               // NEW: body bounds
    QLabel *m_lblDbgSacrum = nullptr;             // NEW: sacrum stats
    QLabel *m_lblDbgHeelLeft = nullptr;           // NEW: left heel stats
    QLabel *m_lblDbgHeelRight = nullptr;          // NEW: right heel stats
    QRect m_dbgSacrumRect;      // NEW: rect فعلی sacrum برای hover highlight
    QRect m_dbgLeftHeelRect;    // NEW: rect فعلی left heel برای hover highlight
    QRect m_dbgRightHeelRect;   // NEW: rect فعلی right heel برای hover highlight

    QComboBox *m_cmbPortSettings = nullptr;
    QPushButton *m_btnRefreshSettings = nullptr;
    QPushButton *m_btnConnectSettings = nullptr;
    QLabel *m_lblStatusSettings = nullptr;

    QSpinBox *m_spHighSettings = nullptr;
    QSpinBox *m_spNoSettings = nullptr;

    // ===== Live Monitoring UI =====
    QLabel *m_lblRiskLive = nullptr;
    QLabel *m_lblMovementLive = nullptr;
    QLabel *m_lblFrameSync = nullptr;   // نمایش وضعیت sync بین packetهای یک frame

    SummaryData m_lastSummary;         // آخرین Summary دریافت‌شده از MCU
    bool m_hasSummary = false;         // آیا تاکنون Summary معتبر گرفته‌ایم؟
    bool m_useAdaptiveZoneStats = true;   // NEW: اگر true باشد، zone cards از analyzer تغذیه می‌شوند
    bool m_compareZoneStats = false;   // NEW: اگر true باشد، zone cards مقایسه دو منبع را نشان می‌دهند
    QTimer *m_summaryUiTimer = nullptr; // تایمر برای stable کردن refresh داشبورد
    qint64 m_lastSummaryRxMs = -1;   // زمان آخرین دریافت Summary برای جلوگیری از خالی شدن لحظه‌ای داشبورد
    QLabel *m_lblAlertsText = nullptr;

    QLabel *m_lblSacrum = nullptr;
    QLabel *m_lblHeelLeft = nullptr;   // نمایش فشار پاشنه چپ
    QLabel *m_lblHeelRight = nullptr;  // نمایش فشار پاشنه راست

    QLabel *m_lblRecommendation = nullptr;

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

    FrameSyncState m_frameSync;   // نگهداری وضعیت sync بین 0x20 / 0x22 / 0x30 / 0x40

    void refreshPorts();
    void setConnectedUi(bool connected);
    void renderSummaryToDashboard();   // آخرین Summary cache شده را با نرخ کنترل‌شده روی UI نمایش می‌دهد
    void renderAdaptiveZoneStats();   // NEW: محاسبه و نمایش zone stats از خود heatmap
    void renderComparedZoneStats();   // NEW: نمایش Device / Adaptive / Delta روی zone cards
    void renderDebugDataPanel();   // NEW: به‌روزرسانی پنل داده‌های مهندسی در تب Debug
    bool isCurrentFrameSynchronized() const;   // آیا همه packetهای لازم روی یک frameId هستند؟
    void updateTableNode(int nodeId);
    void markAllNodesState(NodeState state);
    void updateNodeSummary(int nodeId);
    void refreshNodeCardStyles();

private slots:
    void onConnectClicked();
    void onSerialError(QSerialPort::SerialPortError e);
    void onPacket(const NodePacket &pkt);
    void onBedSnapshot(const BedSnapshotPacket &pkt);  // دریافت snapshot تخت از packet 0x20
    void onBedStatus(const BedStatusPacket &pkt);      // دریافت status تخت از packet 0x22
    void onNodeHealth(const NodeHealthPacket &pkt);  // دریافت وضعیت 16 نود از packet 0x30
    void updateLiveMonitoring();
    void onSummaryReceived(const SummaryData &summary);  // دریافت داده 0x40 از MCU

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // MAINWINDOW_H
