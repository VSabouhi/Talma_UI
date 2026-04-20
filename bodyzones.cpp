#include "bodyzones.h"
#include <algorithm>

BodyZones::Zones BodyZones::estimate(const BodyDetector::Result &body)
{
    Zones z;

    if (!body.valid)
        return z;

    const int bodyTop = body.topRow;
    const int bodyBottom = body.bottomRow;
    const int bodyLeft = body.leftCol;
    const int bodyRight = body.rightCol;
    const int bodyCenter = body.centerCol;

    const int bodyHeight = bodyBottom - bodyTop + 1;
    const int bodyWidth = bodyRight - bodyLeft + 1;

    if (bodyHeight <= 0 || bodyWidth <= 0)
        return z;

    // NEW:
    // sacrum را در بخش میانی-پایینی بدن تخمین می‌زنیم
    const int sacrumTop = bodyTop + int(bodyHeight * 0.55);
    const int sacrumBottom = bodyTop + int(bodyHeight * 0.78);

    const int sacrumHalfWidth = std::max(1, bodyWidth / 5);
    const int sacrumLeft = std::max(bodyLeft, bodyCenter - sacrumHalfWidth);
    const int sacrumRight = std::min(bodyRight, bodyCenter + sacrumHalfWidth);

    // NEW:
    // heelها را نزدیک انتهای پایین بدن می‌گیریم
    const int heelTop = bodyTop + int(bodyHeight * 0.88);
    const int heelBottom = bodyBottom;

    const int heelWidth = std::max(1, bodyWidth / 4);

    const int leftHeelLeft = bodyLeft;
    const int leftHeelRight = std::min(bodyRight, bodyLeft + heelWidth - 1);

    const int rightHeelRight = bodyRight;
    const int rightHeelLeft = std::max(bodyLeft, bodyRight - heelWidth + 1);

    z.valid = true;

    z.sacrumRect = QRect(
        sacrumLeft,
        sacrumTop,
        sacrumRight - sacrumLeft + 1,
        std::max(1, sacrumBottom - sacrumTop + 1)
        );

    z.leftHeelRect = QRect(
        leftHeelLeft,
        heelTop,
        leftHeelRight - leftHeelLeft + 1,
        std::max(1, heelBottom - heelTop + 1)
        );

    z.rightHeelRect = QRect(
        rightHeelLeft,
        heelTop,
        rightHeelRight - rightHeelLeft + 1,
        std::max(1, heelBottom - heelTop + 1)
        );

    return z;
}
