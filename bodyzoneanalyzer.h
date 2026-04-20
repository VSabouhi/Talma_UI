#pragma once

#include "bedframestore.h"
#include "bodyzones.h"

class BodyZoneAnalyzer
{
public:
    struct ZoneStat
    {
        int avg = 0;
        int peak = 0;
        int validCount = 0;
    };

    struct Result
    {
        bool valid = false;

        ZoneStat sacrum;
        ZoneStat leftHeel;
        ZoneStat rightHeel;
    };

    // NEW:
    // محاسبه آمار zoneها از روی BedFrameStore و zone rectها
    static Result analyze(const BedFrameStore *store, const BodyZones::Zones &zones);

private:
    static ZoneStat computeSingleZone(const BedFrameStore *store, const QRect &gridRect);
};
