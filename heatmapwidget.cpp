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

            const int nodeId    = r / 2;
            const int rowInNode = r % 2;
            const int sensorIdx = rowInNode * 16 + c;

            quint8 raw = makeRaw(0, SensorStatus::Disconnected);
            NodeState nState = NodeState::Offline;

            if (m_store) {
                raw = m_store->raw(nodeId, sensorIdx);
                nState = m_store->nodeState(nodeId);
            }

            const quint8 value = sensorValueFromRaw(raw);
            const SensorStatus sst = sensorStatusFromRaw(raw);
            const bool valid = isSensorValid(raw);
            const quint8 conf = sensorConfidence(raw);

            QRect cell(bed.x() + c * cellW,
                       bed.y() + r * cellH,
                       cellW,
                       cellH);

            // اگر این نود هنوز هیچ دیتایی نگرفته، خاکستری خنثی رسم کن
            bool nodeHasData = m_store ? m_store->hasData(nodeId) : false;
            if (!nodeHasData) {
                painter.setPen(Qt::NoPen);

                // خاکستری نرم‌تر + کمی روشن‌تر
                painter.setBrush(QColor(90, 90, 90));

                painter.drawRoundedRect(cell, radius, radius);

                // یک نقطه خیلی کوچک برای زنده بودن UI
                painter.setPen(QColor(50, 50, 50));
                painter.drawPoint(cell.center());

                continue;
            }

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

            // node state overlay
            if (nState == NodeState::Stale) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 215, 0, 25));
                painter.drawRoundedRect(cell, radius, radius);
            }
            else if (nState == NodeState::Offline) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0, 90));
                painter.drawRoundedRect(cell, radius, radius);
            }
        }
    }

    // HEAD / FOOT labels
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(bed.adjusted(0, -18, 0, 0), Qt::AlignHCenter, "HEAD");
    painter.drawText(bed.adjusted(0, 0, 0, 18),
                     Qt::AlignHCenter | Qt::AlignBottom, "FOOT");
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
