#pragma once

#include <QRect>
#include "bodydetector.h"

// NEW:
// این کلاس از روی body bounds، زون‌های تقریبی بدن را به‌صورت adaptive می‌سازد.
// فعلاً فقط sacrum و heelها را تخمین می‌زنیم.
class BodyZones
{
public:
    struct Zones
    {
        bool valid = false;

        QRect sacrumRect;      // grid coordinates: col/row based
        QRect leftHeelRect;    // grid coordinates
        QRect rightHeelRect;   // grid coordinates
    };

    static Zones estimate(const BodyDetector::Result &body);
};
