#pragma once
#include <QtGlobal>
#include <QColor>
#include <Qt>
#include <QString>

enum class SensorStatus : quint8 {
    Ok           = 0,
    Warning      = 1,
    Error        = 2,
    Disconnected = 3
};

enum class NodeState : quint8 {
    Offline = 0,
    Online  = 1,
    Stale   = 2
};

struct SensorVisualStyle {
    QColor base;
    QColor hatch;
    QColor text;
    Qt::BrushStyle brush = Qt::NoBrush;
    bool drawX = false;
    bool drawBorder = false;
    QColor border = Qt::transparent;
};

inline quint8 sensorValueFromRaw(quint8 raw)
{
    return raw & 0x3F;
}

inline SensorStatus sensorStatusFromRaw(quint8 raw)
{
    return static_cast<SensorStatus>((raw >> 6) & 0x03);
}

inline bool isSensorValid(quint8 raw)
{
    const auto st = sensorStatusFromRaw(raw);
    return st == SensorStatus::Ok || st == SensorStatus::Warning;
}

inline quint8 sensorConfidence(quint8 raw)
{
    switch (sensorStatusFromRaw(raw)) {
    case SensorStatus::Ok:           return 255;
    case SensorStatus::Warning:      return 128;
    case SensorStatus::Error:        return 0;
    case SensorStatus::Disconnected: return 0;
    }
    return 0;
}

inline NodeState nodeStateFromFlags(quint8 flags)
{
    return static_cast<NodeState>(flags & 0x03);
}

inline QString sensorStatusText(SensorStatus st)
{
    switch (st) {
    case SensorStatus::Ok:           return "OK";
    case SensorStatus::Warning:      return "WARNING";
    case SensorStatus::Error:        return "ERROR";
    case SensorStatus::Disconnected: return "DISCONNECTED";
    }
    return "UNKNOWN";
}

inline QString sensorStatusShortLabel(SensorStatus st)
{
    switch (st) {
    case SensorStatus::Ok:           return "OK";
    case SensorStatus::Warning:      return "WRN";
    case SensorStatus::Error:        return "ERR";
    case SensorStatus::Disconnected: return "DISC";
    }
    return "UNK";
}

inline QString nodeStateText(NodeState st)
{
    switch (st) {
    case NodeState::Offline: return "OFFLINE";
    case NodeState::Online:  return "ONLINE";
    case NodeState::Stale:   return "STALE";
    }
    return "UNKNOWN";
}

inline SensorVisualStyle styleForSensorStatus(SensorStatus st)
{
    switch (st) {
    case SensorStatus::Ok:
        return {
            QColor(255,255,255),
            QColor(0,0,0,0),
            QColor(20,20,20),
            Qt::NoBrush,
            false,
            false,
            Qt::transparent
        };

    case SensorStatus::Warning:
        return {
            QColor(255,245,200),
            QColor(0,0,0,0),
            QColor(30,30,30),
            Qt::NoBrush,
            false,
            true,
            QColor(255, 210, 0)
        };

    case SensorStatus::Error:
        return {
            QColor(130, 40, 25),
            QColor(255, 170, 120),
            QColor(255,245,235),
            Qt::DiagCrossPattern,
            true,
            false,
            Qt::transparent
        };

    case SensorStatus::Disconnected:
        return {
            QColor(70, 70, 70),
            QColor(140, 140, 140),
            QColor(240,240,240),
            Qt::BDiagPattern,
            true,
            false,
            Qt::transparent
        };
    }

    return {
        QColor(80,80,80),
        QColor(120,120,120),
        QColor(255,255,255),
        Qt::Dense4Pattern,
        false,
        false,
        Qt::transparent
    };
}

inline quint8 makeRaw(quint8 value6bit, SensorStatus st)
{
    return quint8((value6bit & 0x3F) | (quint8(st) << 6));
}
