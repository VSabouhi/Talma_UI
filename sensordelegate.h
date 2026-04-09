#pragma once
#include <QStyledItemDelegate>
#include <QColor>

class SensorDelegate : public QStyledItemDelegate
{
public:
    explicit SensorDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {}

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    void setHeaderColor(const QColor &c) { m_headerColor = c; }

private:
    QColor m_headerColor = QColor(255, 180, 0);
};
