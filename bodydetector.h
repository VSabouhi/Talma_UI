#pragma once

#include "bedframestore.h"

// NEW:
// این کلاس برای تشخیص محدوده تقریبی بدن روی تخت استفاده می‌شود.
// فعلاً فقط یک detection ساده بر اساس سلول‌های active داریم.
// بعداً zone estimation و overlay adaptive بر پایه همین خروجی ساخته می‌شود.
class BodyDetector
{
public:
    struct Result
    {
        bool valid = false;

        int topRow = -1;
        int bottomRow = -1;

        int leftCol = -1;
        int rightCol = -1;

        int centerCol = -1;

        int activeCellCount = 0;


        int firstActiveRowCount = 0;   // NEW: تعداد active cell در اولین ردیف معتبر بدن
        int lastActiveRowCount = 0;    // NEW: تعداد active cell در آخرین ردیف معتبر بدن

        // NEW: debug fields برای بررسی رفتار detector
        int debugMaxPressure = 0;
        int debugMinPressure = 0;
        int debugAdaptiveThreshold = 0;
        int debugMaskCount = 0;
        int debugComponentCount = 0;
        int debugBestComponentSize = 0;
    };

    // NEW:
    // minActivePerRow باعث می‌شود ردیفی که فقط 1 یا 2 سلول active دارد
    // به‌عنوان شروع/پایان بدن پذیرفته نشود.
    static Result detect(const BedFrameStore *store,
                         int threshold = 10,
                         int minActivePerRow = 4);
};
