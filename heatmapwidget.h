#pragma once
#include <QWidget>
#include <QColor>
#include "sensorstatus.h"

class SensorStore;

class HeatmapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HeatmapWidget(QWidget *parent = nullptr);

    void setStore(SensorStore *store);

    void setPressureRange(int highPressureValue, int noPressureValue);
    int highPressureValue() const { return m_highPressure; }
    int noPressureValue() const { return m_noPressure; }

public slots:
    void onNodeUpdated(int nodeId);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor colorForValue(int v) const;
    QRect nodeRect(int nodeId) const;

    SensorStore *m_store = nullptr;

    static constexpr int ROWS = 32;
    static constexpr int COLS = 16;

    int m_highPressure = 5;
    int m_noPressure   = 50;
    int m_cellPad = 1;
};
