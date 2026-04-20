#include "heatmapwidget.h"
#include "sensorstore.h"

#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <algorithm>
#include <QMouseEvent>
#include <QToolTip>
/*========================================================================================*/

/*========================================================================================*/

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
/*========================================================================================*/

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
/*========================================================================================*/

HeatmapWidget::HeatmapWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
    setMouseTracking(true);   // NEW: برای گرفتن حرکت موس بدون کلیک
}
/*========================================================================================*/

void HeatmapWidget::setStore(SensorStore *store)
{
    m_store = store;
    update();
}
/*========================================================================================*/

void HeatmapWidget::setBedStore(BedFrameStore *store)
{
    // منبع جدید heatmap: snapshot/status کامل تخت
    m_bedStore = store;
    update();
}
/*========================================================================================*/

void HeatmapWidget::setPressureRange(int highPressureValue, int noPressureValue)
{
    m_highPressure = highPressureValue;
    m_noPressure   = noPressureValue;
    update();
}
/*========================================================================================*/

void HeatmapWidget::onNodeUpdated(int nodeId)
{
    if (!m_store) return;
    update(nodeRect(nodeId));
}
/*========================================================================================*/

void HeatmapWidget::onBedFrameUpdated(quint16 frameId)
{
    Q_UNUSED(frameId);

    // چون کل فریم تخت عوض شده، کل widget را redraw می‌کنیم
    update();
}
/*========================================================================================*/

QColor HeatmapWidget::colorForValue(int v) const
{
    const int a = m_highPressure;
    const int b = m_noPressure;

    // NEW:
    // اگر pressureLike خیلی بالا باشد، یعنی no-contact / background
    // و باید به‌صورت dark navy نمایش داده شود.
    const int noContactPressureThreshold = 55;

    if (v > noContactPressureThreshold) {
        return QColor(8, 18, 40);   // dark navy
    }

    // NEW: تشخیص background واقعی (no contact)


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
/*========================================================================================*/

QRect HeatmapWidget::bodyBoundsRect(const BodyDetector::Result &body,
                                    const QRect &bedRect,
                                    int cellW,
                                    int cellH) const
{
    // NEW:
    // مختصات body detector در فضای row/col است.
    // این helper آن را به مختصات واقعی رسم روی widget تبدیل می‌کند.
    if (!body.valid)
        return QRect();

    if (body.topRow < 0 || body.bottomRow < 0 ||
        body.leftCol < 0 || body.rightCol < 0)
        return QRect();

    const int x = bedRect.x() + body.leftCol * cellW;
    const int y = bedRect.y() + body.topRow * cellH;

    const int w = (body.rightCol - body.leftCol + 1) * cellW;
    const int h = (body.bottomRow - body.topRow + 1) * cellH;

    return QRect(x, y, w, h);
}
/*========================================================================================*/

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

            // NEW: نگه داشتن مقدار خام فقط برای debug/tooltip
            const quint8 rawValue = value;
            Q_UNUSED(rawValue);

            // برگشت به رفتار قبلی نمایش heatmap:
            // مقدار snapshot را به pressure-like تبدیل می‌کنیم
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

                // NEW: نمایش عدد خام snapshot برای پیدا کردن مرز واقعی no-contact
                if (m_showDebugText && !m_showZoneValues &&
                    cell.width() >= 18 && cell.height() >= 18 && m_bedStore) {
                    const int pValue = int(m_bedStore->value(r, c));
                    const int rawSnapshotValue = 63 - pValue;
                    Q_UNUSED(rawSnapshotValue);

                    QFont f = painter.font();

                    // NEW: سایز فونت وابسته به اندازه سلول (responsive)
                    int fontSize = 8;

                    if (cell.width() < 22 || cell.height() < 22)
                        fontSize = 7;

                    if (cell.width() < 18 || cell.height() < 18)
                        fontSize = 6;

                    f.setPointSize(fontSize);
                    painter.setFont(f);

                    // NEW: رنگ متن adaptive بر اساس شدت فشار برای خوانایی بهتر
                    QColor textColor = QColor(240, 240, 240);

                    if (pValue >= 20 && pValue < 45) {
                        textColor = QColor(20, 20, 20);
                    }
                    else if (pValue >= 45) {
                        textColor = QColor(250, 250, 250);
                    }
                    else {
                        textColor = QColor(235, 235, 235);
                    }

                    painter.setPen(textColor);
                    painter.drawText(cell, Qt::AlignCenter, QString("P:%1").arg(pValue));

                    painter.setPen(Qt::NoPen);
                }

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

    // NEW: debug overlay برای نمایش محدوده تقریبی بدن
    // فعلاً فقط برای توسعه و تست detector است.
    // بعداً این بخش با adaptive zone overlay جایگزین یا تکمیل می‌شود.
    if (m_bedStore && m_bedStore->hasFrame()) {
        // NEW:
        // threshold = 10
        // minActivePerRow = 4
        // تا ردیف‌هایی که فقط چند سلول پراکنده دارند، بدن حساب نشوند.
        const BodyDetector::Result body = BodyDetector::detect(m_bedStore, 10, 4);

        if (body.valid) {
            const QRect bodyRect = bodyBoundsRect(body, bed, cellW, cellH);

            if (!bodyRect.isEmpty()) {
                painter.setPen(QPen(QColor(180, 220, 255, 160), 2));
                painter.setBrush(Qt::NoBrush);

                if (m_showBodyBounds) {
                    painter.drawRoundedRect(bodyRect.adjusted(1, 1, -1, -1), 6, 6);
                }

                // NEW:
                // zoneها را همیشه محاسبه می‌کنیم تا highlight حتی وقتی overlay خاموش است هم کار کند
                const BodyZones::Zones zones = BodyZones::estimate(body);

                if (zones.valid) {

                    auto gridRectToPixelRect = [&](const QRect &gridRect) -> QRect {
                        return QRect(
                            bed.x() + gridRect.x() * cellW,
                            bed.y() + gridRect.y() * cellH,
                            gridRect.width() * cellW,
                            gridRect.height() * cellH
                            );
                    };

                    const QRect sacrumPx = gridRectToPixelRect(zones.sacrumRect);
                    const QRect leftHeelPx = gridRectToPixelRect(zones.leftHeelRect);
                    const QRect rightHeelPx = gridRectToPixelRect(zones.rightHeelRect);

                    // NEW:
                    // highlight باید مستقل از نمایش zone overlay باشد
                    if (m_hasHighlightedZone) {
                        const QRect hlPx = gridRectToPixelRect(m_highlightedZoneRect);

                        painter.setPen(QPen(QColor(255, 255, 255, 220), 2));
                        painter.setBrush(QColor(255, 255, 255, 35));
                        painter.drawRoundedRect(hlPx.adjusted(1, 1, -1, -1), 5, 5);

                        painter.setBrush(Qt::NoBrush);
                    }

                    // NEW: فقط اگر کاربر خواست، خود zone boxها نمایش داده شوند
                    if (m_showBodyZones) {
                        painter.setPen(QPen(QColor(255, 210, 120, 150), 1));
                        painter.setBrush(Qt::NoBrush);
                        painter.drawRoundedRect(sacrumPx.adjusted(1, 1, -1, -1), 4, 4);

                        painter.setPen(QPen(QColor(120, 220, 255, 150), 1));
                        painter.drawRoundedRect(leftHeelPx.adjusted(1, 1, -1, -1), 4, 4);

                        painter.drawRoundedRect(rightHeelPx.adjusted(1, 1, -1, -1), 4, 4);
                    }

                    if (m_showZoneValues) {

                        const BodyZoneAnalyzer::Result zoneStats =
                            BodyZoneAnalyzer::analyze(m_bedStore, zones);

                        const int sacrumAvg = zoneStats.sacrum.avg;
                        const int sacrumPeak = zoneStats.sacrum.peak;
                        const int leftHeelAvg = zoneStats.leftHeel.avg;
                        const int leftHeelPeak = zoneStats.leftHeel.peak;
                        const int rightHeelAvg = zoneStats.rightHeel.avg;
                        const int rightHeelPeak = zoneStats.rightHeel.peak;

                        QFont oldFont = painter.font();
                        QFont debugFont = oldFont;
                        debugFont.setPointSize(8);
                        debugFont.setBold(true);
                        painter.setFont(debugFont);

                        // NEW:
                        // رسم یک label box مستقل نزدیک zone
                        auto drawFloatingZoneLabel = [&](const QRect &anchorRect,
                                                         const QString &text,
                                                         Qt::Alignment anchorMode)
                        {
                            QFontMetrics fm(debugFont);
                            const bool compactLabel =
                                (anchorRect.width() < 42 || anchorRect.height() < 42);

                            QString drawText = text;

                            // NEW:
                            // برای labelهای کناری (heelها) عرض بیشتری می‌گیریم تا A و P جا شوند
                            const bool sideLabel =
                                (anchorMode == Qt::AlignLeft || anchorMode == Qt::AlignRight);

                            const int preferredWidth = sideLabel
                                                           ? (compactLabel ? 110 : 135)
                                                           : (compactLabel ? 90 : 120);

                            const int preferredHeight = compactLabel ? 34 : 52;

                            QRect textBox = fm.boundingRect(
                                QRect(0, 0, preferredWidth, preferredHeight),
                                Qt::AlignCenter | Qt::TextWordWrap,
                                drawText
                                );

                            textBox.adjust(-6, -4, +6, +4);

                            QPoint center;
                            if (anchorMode == Qt::AlignTop) {
                                center = QPoint(anchorRect.center().x(), anchorRect.top() - textBox.height() / 2 - 6);
                            } else if (anchorMode == Qt::AlignLeft) {
                                center = QPoint(anchorRect.left() - textBox.width() / 2 - 6, anchorRect.center().y());
                            } else { // Qt::AlignRight
                                center = QPoint(anchorRect.right() + textBox.width() / 2 + 6, anchorRect.center().y());
                            }

                            QRect box(
                                center.x() - textBox.width() / 2,
                                center.y() - textBox.height() / 2,
                                textBox.width(),
                                textBox.height()
                                );

                            // اگر box از محدوده heatmap بیرون می‌زد، clamp کن
                            QRect safeArea = bed.adjusted(2, 2, -2, -2);

                            if (box.left() < safeArea.left())
                                box.moveLeft(safeArea.left());
                            if (box.right() > safeArea.right())
                                box.moveRight(safeArea.right());
                            if (box.top() < safeArea.top())
                                box.moveTop(safeArea.top());
                            if (box.bottom() > safeArea.bottom())
                                box.moveBottom(safeArea.bottom());

                            painter.setPen(QPen(QColor(220, 235, 245, 180), 1));
                            painter.setBrush(QColor(10, 16, 28, 185));
                            painter.drawRoundedRect(box, 5, 5);

                            painter.setPen(QColor(245, 245, 245, 230));
                            painter.drawText(box.adjusted(3, 2, -3, -2),
                                             Qt::AlignCenter | Qt::TextWordWrap,
                                             drawText);
                        };

                        // Sacrum: label بالای zone
                        drawFloatingZoneLabel(
                            sacrumPx,
                            QString("Sacrum\nA:%1 P:%2").arg(sacrumAvg).arg(sacrumPeak),
                            Qt::AlignTop
                            );

                        // Left heel: label سمت چپ zone
                        drawFloatingZoneLabel(
                            leftHeelPx,
                            QString("L Heel\nA:%1 P:%2").arg(leftHeelAvg).arg(leftHeelPeak),
                            Qt::AlignLeft
                            );

                        // Right heel: label سمت راست zone
                        drawFloatingZoneLabel(
                            rightHeelPx,
                            QString("R Heel\nA:%1 P:%2").arg(rightHeelAvg).arg(rightHeelPeak),
                            Qt::AlignRight
                            );

                        painter.setFont(oldFont);
                    }
                }
            }
        }
    }
}
/*========================================================================================*/

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
/*========================================================================================*/
void HeatmapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_bedStore || !m_bedStore->hasFrame())
        return;

    const int MARGIN = 20;
    int cellW = 1, cellH = 1;
    QRect bed = computeBedRectSnapped(rect(), COLS, ROWS, MARGIN, cellW, cellH);

    QPoint pos = event->pos();

    if (!bed.contains(pos)) {
        QToolTip::hideText();
        return;
    }

    int c = (pos.x() - bed.x()) / cellW;
    int r = (pos.y() - bed.y()) / cellH;

    if (r < 0 || r >= ROWS || c < 0 || c >= COLS)
        return;

    quint8 raw = m_bedStore->value(r, c);
    quint8 pressureLike = 63 - raw;

    QString text = QString("R:%1 C:%2\nRAW:%3\nP:%4")
                       .arg(r)
                       .arg(c)
                       .arg(raw)
                       .arg(pressureLike);

    if (m_showTooltip) {
        QToolTip::showText(event->globalPosition().toPoint(), text, this);
    } else {
        QToolTip::hideText();
    }
}
/*========================================================================================*/
void HeatmapWidget::setShowDebugText(bool enabled)
{
    m_showDebugText = enabled;
    update();
}
/*========================================================================================*/

void HeatmapWidget::setShowTooltip(bool enabled)
{
    m_showTooltip = enabled;

    if (!m_showTooltip) {
        QToolTip::hideText();
    }
}
/*========================================================================================*/
void HeatmapWidget::setShowBodyBounds(bool enabled)
{
    m_showBodyBounds = enabled;
    update();
}
/*========================================================================================*/

void HeatmapWidget::setShowBodyZones(bool enabled)
{
    m_showBodyZones = enabled;
    update();
}
/*========================================================================================*/
void HeatmapWidget::setShowZoneValues(bool enabled)
{
    m_showZoneValues = enabled;
    update();
}
/*========================================================================================*/
void HeatmapWidget::setHighlightedZoneRect(const QRect &gridRect)
{
    m_highlightedZoneRect = gridRect;
    m_hasHighlightedZone = true;
    update();
}
/*========================================================================================*/
void HeatmapWidget::clearHighlightedZoneRect()
{
    m_highlightedZoneRect = QRect();
    m_hasHighlightedZone = false;
    update();
}
/*========================================================================================*/
