#pragma once
#include <QWidget>
#include <QColor>
#include "sensorstatus.h"
#include "bedframestore.h"   // برای خواندن داده‌های تخت از packetهای 0x20 و 0x22
#include "bodydetector.h"   // NEW: برای تشخیص محدوده تقریبی بدن روی تخت
#include "bodyzones.h"   // NEW: برای تخمین زون‌های adaptive از روی body bounds
#include "bodyzoneanalyzer.h"   // NEW: محاسبه reusable آمار zoneها
#include <QRect>
#include <QElapsedTimer>

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

    void setShowDebugText(bool enabled);
    void setShowTooltip(bool enabled);

    bool showDebugText() const { return m_showDebugText; }
    bool showTooltip() const { return m_showTooltip; }
    void setHighlightedZoneRect(const QRect &gridRect);   // NEW: highlight zone از بیرون
    void clearHighlightedZoneRect();                      // NEW: پاک کردن highlight
    void setShowBodyBounds(bool enabled);
    void setShowBodyZones(bool enabled);
    void setShowZoneValues(bool enabled);   // NEW: روشن/خاموش کردن مقادیر zoneها



public slots:
    void onNodeUpdated(int nodeId);
    void onBedFrameUpdated(quint16 frameId);  // وقتی BED frame جدید رسید، Heatmap را refresh می‌کنیم

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;   // NEW: نمایش مقدار سلول زیر موس

private:
    QColor colorForValue(int v) const;
    QRect nodeRect(int nodeId) const;
    QRect bodyBoundsRect(const BodyDetector::Result &body,
                         const QRect &bedRect,
                         int cellW,
                         int cellH) const;   // NEW: تبدیل body bounds از مختصات grid به مختصات رسم


    SensorStore *m_store = nullptr;
    BedFrameStore *m_bedStore = nullptr;   // منبع جدید داده‌ی تخت

    static constexpr int ROWS = 32;
    static constexpr int COLS = 16;

    int m_highPressure = 5;
    int m_noPressure   = 50;
    int m_cellPad = 1;
    bool m_debugShowRawValues = true;   // NEW: نمایش عدد خام هر سلول برای پیدا کردن مرز no-contact

    // NEW: debug flags (برای آینده UI config)
    bool m_showDebugText = true;     // نمایش اعداد داخل سلول
    bool m_showTooltip = true;       // tooltip موس
    bool m_showBodyBounds = false;   // NEW: نمایش bounding box بدن (فقط debug)
    bool m_showBodyZones = false;     // NEW: نمایش zoneها
    bool m_showZoneValues = false;   // NEW: نمایش مقادیر عددی zoneها روی خود heatmap
    QRect m_highlightedZoneRect;      // NEW: zone انتخاب‌شده برای highlight
    bool m_hasHighlightedZone = false; // NEW: آیا highlight فعال است؟
    // ======================================================
    // Repaint throttle timer.
    //
    // Limits QWidget repaint frequency during
    // high-frequency live updates.
    // ======================================================
    QElapsedTimer m_repaintLimiter;



};
