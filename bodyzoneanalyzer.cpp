#include "bodyzoneanalyzer.h"

BodyZoneAnalyzer::ZoneStat BodyZoneAnalyzer::computeSingleZone(const BedFrameStore *store,
                                                               const QRect &gridRect)
{
    ZoneStat s;

    if (!store || !store->hasFrame())
        return s;

    int sum = 0;
    int peak = 0;
    int count = 0;

    for (int rr = gridRect.y(); rr < gridRect.y() + gridRect.height(); ++rr) {
        for (int cc = gridRect.x(); cc < gridRect.x() + gridRect.width(); ++cc) {

            if (rr < 0 || rr >= BedFrameStore::ROWS || cc < 0 || cc >= BedFrameStore::COLS)
                continue;

            const quint8 status = store->status(rr, cc);
            const bool valid = (status == 0 || status == 1);
            if (!valid)
                continue;

            const int p = int(store->value(rr, cc));

            sum += p;
            count++;

            if (p > peak)
                peak = p;
        }
    }

    s.avg = (count > 0) ? (sum / count) : 0;
    s.peak = peak;
    s.validCount = count;

    return s;
}

BodyZoneAnalyzer::Result BodyZoneAnalyzer::analyze(const BedFrameStore *store,
                                                   const BodyZones::Zones &zones)
{
    Result r;

    if (!store || !store->hasFrame() || !zones.valid)
        return r;

    r.valid = true;
    r.sacrum = computeSingleZone(store, zones.sacrumRect);
    r.leftHeel = computeSingleZone(store, zones.leftHeelRect);
    r.rightHeel = computeSingleZone(store, zones.rightHeelRect);

    return r;
}
