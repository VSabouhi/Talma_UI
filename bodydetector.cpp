#include "bodydetector.h"
#include <QtGlobal>   // NEW: برای Q_UNUSED
#include <cstring>   //برای memset
/*========================================================================================*/

/*========================================================================================*/

BodyDetector::Result BodyDetector::detect(const BedFrameStore *store,
                                          int threshold,
                                          int minActivePerRow)
{
    Q_UNUSED(threshold);
    Q_UNUSED(minActivePerRow);

    Result res;

    if (!store || !store->hasFrame())
        return res;

    static constexpr int ROWS = BedFrameStore::ROWS;
    static constexpr int COLS = BedFrameStore::COLS;
    static constexpr int MAX_CELLS = ROWS * COLS;
    static constexpr int MAX_COMPONENTS = MAX_CELLS;

    struct Component
    {
        bool used = false;
        int count = 0;
        int minR = 0, maxR = 0;
        int minC = 0, maxC = 0;
        int sumC = 0;
    };

    // ===============================
    // 1) Build mask (contact detection)
    // ===============================
    bool mask[ROWS][COLS] = {};
    int maskCount = 0;

    // NEW:
    // طبق تست تو:
    // P > 55 یعنی no-contact
    // پس فقط P <= 55 را contact حساب می‌کنیم
    const int noContactThreshold = 55;

    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {

            const quint8 status = store->status(r, c);
            const bool valid = (status == 0 || status == 1);
            if (!valid)
                continue;

            // NEW:
            // استفاده مستقیم از P (همان چیزی که در heatmap نمایش داده می‌شود)
            const int p = int(store->value(r, c));

            // NEW:
            // P کوچک یعنی بدون تماس
            const int contactThreshold = 10;

            if (p >= contactThreshold) {
                mask[r][c] = true;
                maskCount++;
            }
        }
    }

    if (maskCount == 0)
        return res;

    // ===============================
    // 2) Connected Components (8-neighborhood)
    // ===============================
    bool visited[ROWS][COLS] = {};

    const int dr[8] = {-1, -1, -1,  0, 0,  1, 1, 1};
    const int dc[8] = {-1,  0,  1, -1, 1, -1, 0, 1};

    Component comps[MAX_COMPONENTS];
    int compCount = 0;

    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {

            if (!mask[r][c] || visited[r][c])
                continue;

            int stackR[MAX_CELLS];
            int stackC[MAX_CELLS];
            int sp = 0;

            stackR[sp] = r;
            stackC[sp] = c;
            sp++;

            visited[r][c] = true;

            Component comp;
            comp.used = true;
            comp.count = 0;
            comp.minR = comp.maxR = r;
            comp.minC = comp.maxC = c;
            comp.sumC = 0;

            while (sp > 0) {
                sp--;

                const int cr = stackR[sp];
                const int cc = stackC[sp];

                comp.count++;
                comp.sumC += cc;

                comp.minR = std::min(comp.minR, cr);
                comp.maxR = std::max(comp.maxR, cr);
                comp.minC = std::min(comp.minC, cc);
                comp.maxC = std::max(comp.maxC, cc);

                for (int k = 0; k < 8; ++k) {
                    const int nr = cr + dr[k];
                    const int nc = cc + dc[k];

                    if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS)
                        continue;

                    if (visited[nr][nc])
                        continue;

                    if (!mask[nr][nc])
                        continue;

                    visited[nr][nc] = true;
                    stackR[sp] = nr;
                    stackC[sp] = nc;
                    sp++;
                }
            }

            if (compCount < MAX_COMPONENTS) {
                comps[compCount++] = comp;
            }
        }
    }

    if (compCount == 0)
        return res;

    // ===============================
    // 3) Find largest component (torso/main body)
    // ===============================
    int bestIdx = 0;
    for (int i = 1; i < compCount; ++i) {
        if (comps[i].count > comps[bestIdx].count)
            bestIdx = i;
    }

    int finalTop = comps[bestIdx].minR;
    int finalBottom = comps[bestIdx].maxR;
    int finalLeft = comps[bestIdx].minC;
    int finalRight = comps[bestIdx].maxC;
    int finalSumC = comps[bestIdx].sumC;
    int finalCount = comps[bestIdx].count;

    const int mainCenter =
        (comps[bestIdx].count > 0) ? (comps[bestIdx].sumC / comps[bestIdx].count) : -1;

    // ===============================
    // 4) Merge a head-like component if it is above and near the main body
    // ===============================
    for (int i = 0; i < compCount; ++i) {
        if (i == bestIdx)
            continue;

        const Component &cand = comps[i];

        // NEW:
        // فقط componentهای کوچک ولی معنی‌دار را بررسی می‌کنیم
        if (cand.count < 3)
            continue;

        const int candCenter = (cand.count > 0) ? (cand.sumC / cand.count) : -1;

        // NEW:
        // باید بالاتر از component اصلی باشد
        const bool isAbove = cand.maxR < comps[bestIdx].minR;

        // NEW:
        // از نظر افقی خیلی دور نباشد
        const bool horizontallyNear = std::abs(candCenter - mainCenter) <= 3;

        // NEW:
        // فاصله عمودی بین head و torso خیلی زیاد نباشد
        const int verticalGap = comps[bestIdx].minR - cand.maxR;
        const bool verticallyNear = (verticalGap >= 1 && verticalGap <= 8);

        if (isAbove && horizontallyNear && verticallyNear) {
            finalTop = std::min(finalTop, cand.minR);
            finalBottom = std::max(finalBottom, cand.maxR);
            finalLeft = std::min(finalLeft, cand.minC);
            finalRight = std::max(finalRight, cand.maxC);
            finalSumC += cand.sumC;
            finalCount += cand.count;
        }
    }

    // ===============================
    // 5) Fill result
    // ===============================
    res.valid = true;
    res.topRow = finalTop;
    res.bottomRow = finalBottom;
    res.leftCol = finalLeft;
    res.rightCol = finalRight;
    res.centerCol = (finalCount > 0) ? (finalSumC / finalCount) : -1;
    res.activeCellCount = finalCount;

    return res;
}
/*========================================================================================*/
