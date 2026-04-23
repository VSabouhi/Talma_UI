#include "trendhistorystore.h"

TrendHistoryStore::TrendHistoryStore(QObject *parent)
    : QObject(parent)
{
}

void TrendHistoryStore::appendSummary(const SummaryData &summary)
{
    Sample s;
    s.frameId = summary.frameId;
    s.uptimeS = summary.uptimeS;

    s.riskScore = summary.riskScore;
    s.riskLevel = summary.riskLevel;

    s.movementDetected = summary.movementDetected;
    s.timeSinceLastMovementS = summary.timeSinceLastMovementS;

    s.sacrumAvg = summary.sacrumAvg;
    s.sacrumPeak = summary.sacrumPeak;

    s.heelLeftAvg = summary.heelLeftAvg;
    s.heelRightAvg = summary.heelRightAvg;

    s.shouldersAvg = summary.shouldersAvg;
    s.shouldersPeak = summary.shouldersPeak;

    s.pressureExposureThreshold = summary.pressureExposureThreshold;

    s.sacrumExposureS = summary.sacrumExposureS;
    s.heelsExposureS = summary.heelsExposureS;
    s.shouldersExposureS = summary.shouldersExposureS;

    s.zonesValidMask = summary.zonesValidMask;
    s.summaryFlags = summary.summaryFlags;

    m_samples.append(s);

    // NEW: نگه داشتن فقط آخرین N نمونه
    while (m_samples.size() > m_maxSamples) {
        m_samples.removeFirst();
    }

    emit historyUpdated();
}

void TrendHistoryStore::clear()
{
    m_samples.clear();
    emit historyUpdated();
}

void TrendHistoryStore::setMaxSamples(int maxSamples)
{
    if (maxSamples < 1)
        maxSamples = 1;

    m_maxSamples = maxSamples;

    while (m_samples.size() > m_maxSamples) {
        m_samples.removeFirst();
    }

    emit historyUpdated();
}