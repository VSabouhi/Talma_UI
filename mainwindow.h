#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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

    QLabel *m_lblAlertsText = nullptr;

    QLabel *m_lblSacrum = nullptr;
    QLabel *m_lblHeels = nullptr;
    QLabel *m_lblShoulders = nullptr;

    QLabel *m_lblRecommendation = nullptr;

    void refreshPorts();
    void setConnectedUi(bool connected);
    void updateTableNode(int nodeId);
    void markAllNodesState(NodeState state);
    void updateNodeSummary(int nodeId);
    void refreshNodeCardStyles();

private slots:
    void onConnectClicked();
    void onSerialError(QSerialPort::SerialPortError e);
    void onPacket(const NodePacket &pkt);
    void updateLiveMonitoring();
    void onSummaryReceived(const SummaryData &summary);  // دریافت داده 0x40 از MCU

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // MAINWINDOW_H
