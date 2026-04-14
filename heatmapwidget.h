#pragma once
#include <QWidget>
#include <QColor>
#include "sensorstatus.h"
#include "bedframestore.h"   // برای خواندن داده‌های تخت از packetهای 0x20 و 0x22

class SensorStore;

class HeatmapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HeatmapWidget(QWidget *parent = nullptr);

    void setStore(SensorStore *store);
    void setBedStore(BedFrameStore *store);   // اتصال Heatmap به storage جدید BED

    void setPressureRange(int highPressureValue, int noPressureValue);
    int highPressureValue() const { return m_highPressure; }
    int noPressureValue() const { return m_noPressure; }


public slots:
    void onNodeUpdated(int nodeId);
    void onBedFrameUpdated(quint16 frameId);  // وقتی BED frame جدید رسید، Heatmap را refresh می‌کنیم

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor colorForValue(int v) const;
    QRect nodeRect(int nodeId) const;

    SensorStore *m_store = nullptr;
    BedFrameStore *m_bedStore = nullptr;   // منبع جدید داده‌ی تخت

    static constexpr int ROWS = 32;
    static constexpr int COLS = 16;

    int m_highPressure = 5;
    int m_noPressure   = 50;
    int m_cellPad = 1;
};
