#pragma once

#include <QObject>
#include <QVector>
#include "summarydata.h"

// NEW:
// این کلاس history نمونه‌های SUMMARY را نگه می‌دارد
// تا بعداً برای chart / analytics استفاده شوند.
class TrendHistoryStore : public QObject
{
    Q_OBJECT

public:
    struct Sample
    {
        quint16 frameId = 0;
        quint16 uptimeS = 0;

        quint8 riskScore = 0;
        quint8 riskLevel = 0;

        quint8 movementDetected = 0;
        quint16 timeSinceLastMovementS = 0;

        quint8 sacrumAvg = 0;
        quint8 sacrumPeak = 0;

        quint8 heelLeftAvg = 0;
        quint8 heelRightAvg = 0;

        quint8 shouldersAvg = 0;
        quint8 shouldersPeak = 0;

        quint8 pressureExposureThreshold = 0;

        quint16 sacrumExposureS = 0;
        quint16 heelsExposureS = 0;
        quint16 shouldersExposureS = 0;

        quint8 zonesValidMask = 0;
        quint8 summaryFlags = 0;
    };

    explicit TrendHistoryStore(QObject *parent = nullptr);

    void appendSummary(const SummaryData &summary);
    void clear();

    const QVector<Sample>& samples() const { return m_samples; }
    int size() const { return m_samples.size(); }

    // NEW: محدودیت history برای سبک ماندن UI
    void setMaxSamples(int maxSamples);
    int maxSamples() const { return m_maxSamples; }

signals:
    void historyUpdated();

private:
    QVector<Sample> m_samples;
    int m_maxSamples = 600;   // حدود 10 دقیقه در 1Hz
};