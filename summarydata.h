#ifndef SUMMARYDATA_H
#define SUMMARYDATA_H
#pragma once
#include <QtGlobal>

/* مدل داده می‌سازیم که بعداً SUMMARY 0x40 داخلش ریخته شود.
بعداً وقتی packet از UART رسید، اطلاعاتش را داخل این struct می‌ریزیم، مثل:

risk
movement
alert
recommendation
zone metrics*/

struct SummaryData
{
    quint16 frameId = 0;
    quint16 uptimeS = 0;                 // NEW: زمان از روشن شدن سیستم

    quint8 riskScore = 0;
    quint8 riskLevel = 0;

    quint8 movementDetected = 0;
    quint16 timeSinceLastMovementS = 0;

    quint8 alertActive = 0;
    quint8 alertType = 0;
    quint8 alertSeverity = 0;
    quint16 alertDurationS = 0;

    quint8 recommendationCode = 0;
    quint8 recommendationPriority = 0;

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
    quint8 zonesValidMask = 0;           // NEW
    quint8 summaryFlags = 0;             // NEW
};

Q_DECLARE_METATYPE(SummaryData);


#endif // SUMMARYDATA_H
