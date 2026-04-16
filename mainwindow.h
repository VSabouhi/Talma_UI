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
#include "summarydata.h"  // برای دریافت داده Summary از SerialReceiver
#include "bedframestore.h"   // برای نگهداری داده‌های BED_SNAPSHOT و BED_STATUS
#include <QTimer>   // برای محدود کردن نرخ refresh داشبورد
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
