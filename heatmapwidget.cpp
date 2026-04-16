#include "heatmapwidget.h"
#include "sensorstore.h"

#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <algorithm>

static QRect fitAspect(const QRect& r, int w, int h)
{
    const double target = double(w) / double(h);
    const double cur    = double(r.width()) / double(r.height());

    if (cur > target) {
        int newW = int(r.height() * target);
        int x = r.x() + (r.width() - newW) / 2;
        return QRect(x, r.y(), newW, r.height());
    } else {
        int newH = int(r.width() / target);
        int y = r.y() + (r.height() - newH) / 2;
        return QRect(r.x(), y, r.width(), newH);
    }
}

static QRect computeBedRectSnapped(const QRect& widgetRect, int cols, int rows,
                                   int margin, int& outCellW, int& outCellH)
{
    QRect outer = widgetRect.adjusted(margin, margin, -margin, -margin);
    QRect bed   = fitAspect(outer, cols, rows);

    outCellW = std::max(1, bed.width()  / cols);
    outCellH = std::max(1, bed.height() / rows);

    QRect snapped = bed;
    snapped.setWidth(outCellW * cols);
    snapped.setHeight(outCellH * rows);
    snapped.moveCenter(bed.center());
    return snapped;
}

HeatmapWidget::HeatmapWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
}

void HeatmapWidget::setStore(SensorStore *store)
{
    m_store = store;
    update();
}

void HeatmapWidget::setBedStore(BedFrameStore *store)
{
    // منبع جدید heatmap: snapshot/status کامل تخت
    m_bedStore = store;
    update();
}

void HeatmapWidget::setPressureRange(int highPressureValue, int noPressureValue)
{
    m_highPressure = highPressureValue;
    m_noPressure   = noPressureValue;
    update();
}

void HeatmapWidget::onNodeUpdated(int nodeId)
{
    if (!m_store) return;
    update(nodeRect(nodeId));
}

void HeatmapWidget::onBedFrameUpdated(quint16 frameId)
{
    Q_UNUSED(frameId);

    // چون کل فریم تخت عوض شده، کل widget را redraw می‌کنیم
    update();
}

QColor HeatmapWidget::colorForValue(int v) const
{
    const int a = m_highPressure;
    const int b = m_noPressure;

    if (a == b) return QColor(0, 0, 255);

    v = std::clamp(v, std::min(a, b), std::max(a, b));

    double t;
    if (a < b)
        t = 1.0 - double(v - a) / double(b - a);
    else
        t = double(v - b) / double(a - b);

    t = std::clamp(t, 0.0, 1.0);

    if (t < 0.33) {
        double k = t / 0.33;
        return QColor(0, int(255 * k), int(255 * (1.0 - k)));
    } else if (t < 0.66) {
        double k = (t - 0.33) / 0.33;
        return QColor(int(255 * k), 255, 0);
    } else {
        double k = (t - 0.66) / 0.34;
        k = std::clamp(k, 0.0, 1.0);
        return QColor(255, int(255 * (1.0 - k)), 0);
    }
}

void HeatmapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Background
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0, QColor(8, 8, 8));
    bg.setColorAt(1, QColor(18, 18, 18));
    painter.fillRect(rect(), bg);

    const int MARGIN = 20;
    int cellW = 1, cellH = 1;
    QRect bed = computeBedRectSnapped(rect(), COLS, ROWS, MARGIN, cellW, cellH);

    // Frame
    painter.setPen(QPen(QColor(50, 50, 50), 2));
    painter.drawRect(bed.adjusted(-2, -2, 2, 2));
    painter.setPen(Qt::NoPen);

    const int gap = 1;
    const int radius = 2;
    const QColor gapColor(12, 12, 12);

    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {

            // در مدل جدید، Heatmap مستقیماً از BED_SNAPSHOT و BED_STATUS می‌خواند
            quint8 value = 0;
            quint8 status = 3; // پیش‌فرض: disconnected
            bool valid = false;

            if (m_bedStore && m_bedStore->hasFrame()) {
                value = m_bedStore->value(r, c);
                status = m_bedStore->status(r, c);

                // طبق protocol:
                // 0 = OK
                // 1 = WARNING
                // 2 = ERROR
                // 3 = DISCONNECTED
                valid = (status == 0 || status == 1);
            }

            const SensorStatus sst =
                (status == 0) ? SensorStatus::Ok :
                    (status == 1) ? SensorStatus::Warning :
                    (status == 2) ? SensorStatus::Error :
                    SensorStatus::Disconnected;

            const quint8 conf = 255;

            // BED_SNAPSHOT مقدار distance-like می‌فرستد:
            // عدد کوچکتر = فشار بیشتر
            // برای Heatmap فعلی آن را به pressure-like تبدیل می‌کنیم
            value = quint8(63 - value);

            QRect cell(bed.x() + c * cellW,
                       bed.y() + r * cellH,
                       cellW,
                       cellH);

            // gap background
            if (gap > 0) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(gapColor);
                painter.drawRoundedRect(cell, radius, radius);
                cell = cell.adjusted(gap, gap, -gap, -gap);
            }

            // valid sensor -> heatmap
            if (valid) {
                QColor base = colorForValue(value);

                if (conf < 255)
                    base = base.lighter(115);

                painter.setPen(Qt::NoPen);
                painter.setBrush(base);
                painter.drawRoundedRect(cell, radius, radius);

                // warning -> thin yellow border
                if (sst == SensorStatus::Warning) {
                    painter.setPen(QPen(QColor(255, 220, 0), 2));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRoundedRect(cell.adjusted(1, 1, -1, -1), radius, radius);
                    painter.setPen(Qt::NoPen);
                }
            }
            // invalid sensor -> gray + optional X
            else {
                const auto st = styleForSensorStatus(sst);

                painter.setPen(Qt::NoPen);
                painter.setBrush(st.base);
                painter.drawRoundedRect(cell, radius, radius);

                if (st.drawX) {
                    painter.setPen(QPen(QColor(245, 245, 245, 180), 2));
                    painter.drawLine(cell.topLeft() + QPoint(2, 2),
                                     cell.bottomRight() - QPoint(2, 2));
                    painter.drawLine(cell.topRight() + QPoint(-2, 2),
                                     cell.bottomLeft() + QPoint(2, -2));
                    painter.setPen(Qt::NoPen);
                }
            }

        }
    }

    // HEAD / FOOT labels
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(bed.adjusted(0, -18, 0, 0), Qt::AlignHCenter, "HEAD");
    painter.drawText(bed.adjusted(0, 0, 0, 18),
                     Qt::AlignHCenter | Qt::AlignBottom, "FOOT");


    // ================= ZONE OVERLAY =================
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int rows = 32;
    const int cols = 16;

    // سبک خط خیلی subtle
    QPen pen(QColor(200, 200, 200, 90));
    pen.setWidth(1);
    p.setPen(pen);

    QFont f = p.font();
    f.setPointSize(8);
    p.setFont(f);

    // -------- Sacrum (مرکز تخت) --------
    {
        int r0 = 12;
        int r1 = 20;
        int c0 = 4;
        int c1 = 12;

        QRectF rect(c0 * float(cellW),
                    r0 * float(cellH),
                    (c1 - c0) * float(cellW),
                    (r1 - r0) * float(cellH));

        p.drawRoundedRect(rect, 6, 6);

        p.drawText(rect.adjusted(4, 4, -4, -4),
                   Qt::AlignTop | Qt::AlignLeft,
                   "Sacrum");
    }

    // -------- Left Heel --------
    {
        int r0 = 26;
        int r1 = 31;
        int c0 = 0;
        int c1 = 4;

        QRectF rect(c0 * float(cellW),
                    r0 * float(cellH),
                    (c1 - c0) * float(cellW),
                    (r1 - r0) * float(cellH));

        p.drawRoundedRect(rect, 4, 4);

        p.drawText(rect.adjusted(2, 2, -2, -2),
                   Qt::AlignTop | Qt::AlignLeft,
                   "L Heel");
    }

    // -------- Right Heel --------
    {
        int r0 = 26;
        int r1 = 31;
        int c0 = 12;
        int c1 = 16;

        QRectF rect(c0 * float(cellW),
                    r0 * float(cellH),
                    (c1 - c0) * float(cellW),
                    (r1 - r0) * float(cellH));

        p.drawRoundedRect(rect, 4, 4);

        p.drawText(rect.adjusted(2, 2, -2, -2),
                   Qt::AlignTop | Qt::AlignLeft,
                   "R Heel");
    }


}

QRect HeatmapWidget::nodeRect(int nodeId) const
{
    if (nodeId < 0 || nodeId >= 16) return QRect();

    const int MARGIN = 20;
    int cellW = 1, cellH = 1;
    QRect bed = computeBedRectSnapped(rect(), COLS, ROWS, MARGIN, cellW, cellH);

    const int startRow = nodeId * 2;
    QRect r(bed.x(),
            bed.y() + startRow * cellH,
            bed.width(),
            2 * cellH);

    return r.adjusted(-2, -2, +2, +2);
}
