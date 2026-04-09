#include "sensordelegate.h"
#include <QPainter>
#include <QApplication>
#include "sensorstatus.h"

void SensorDelegate::paint(QPainter *painter,
                           const QStyleOptionViewItem &option,
                           const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    painter->save();
    const QRect r = opt.rect;

    const int nodeId = index.row() / 2;
    const QColor zebra = (nodeId % 2 == 0) ? QColor(255,255,255) : QColor(238,238,238);

    const quint8 raw = quint8(index.data(Qt::UserRole).toInt());
    const NodeState nodeState = static_cast<NodeState>(index.data(Qt::UserRole + 1).toInt());
    const quint8 value = sensorValueFromRaw(raw);
    const SensorStatus st = sensorStatusFromRaw(raw);
    const bool valid = isSensorValid(raw);

    if (valid) {
        QColor bg = zebra;
        if (st == SensorStatus::Warning)
            bg = QColor(255, 248, 220);

        painter->fillRect(r, bg);

        if (st == SensorStatus::Warning) {
            painter->setPen(QPen(QColor(255, 210, 0), 2));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(r.adjusted(1,1,-2,-2));
        }
    } else {
        const auto style = styleForSensorStatus(st);

        painter->fillRect(r, style.base);
        painter->setPen(Qt::NoPen);
    }

    if (nodeState == NodeState::Stale) {
        painter->fillRect(r, QColor(255, 215, 0, 25));
    } else if (nodeState == NodeState::Offline) {
        painter->fillRect(r, QColor(30, 30, 30, 55));
    }




    QFont valueFont = opt.font;
    valueFont.setBold(valid);
    painter->setFont(valueFont);

    QColor textColor;

    if (!valid) {
        textColor = styleForSensorStatus(st).text;
    }
    else {
        // برای سلول‌های valid، متن را روی زمینه روشن تیره بکش
        textColor = QColor(25, 25, 25);

        // اگر نود آفلاین بود، کمی ملایم‌تر ولی هنوز خوانا
        if (nodeState == NodeState::Offline)
            textColor = QColor(55, 55, 55);
    }

    painter->setPen(textColor);

    const QString valueText = index.data(Qt::DisplayRole).toString();
    QRect valRect = r.adjusted(2, 2, -2, -2);
    painter->drawText(valRect, Qt::AlignCenter, valueText);


    painter->restore();
}
