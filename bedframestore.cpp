#include "bedframestore.h"
#include <QDebug>


// سازنده کلاس
BedFrameStore::BedFrameStore(QObject *parent)
    : QObject(parent)
{
    // m_values و m_status با = {} از قبل صفر شده‌اند
}

// خواندن مقدار distance-like یک سلول
quint8 BedFrameStore::value(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return 0;

    return m_values[row][col];
}

// خواندن status یک سلول
quint8 BedFrameStore::status(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return 3; // خارج از بازه را disconnected در نظر می‌گیریم

    return m_status[row][col];
}

// آیا حداقل یک frame از برد گرفته‌ایم؟
bool BedFrameStore::hasFrame() const
{
    return m_hasFrame;
}

// frame id آخرین داده‌ی ذخیره‌شده
quint16 BedFrameStore::frameId() const
{
    return m_frameId;
}

// ذخیره snapshot خام تخت (packet 0x20)
void BedFrameStore::setSnapshot(quint16 frameId, const quint8 *data, int count)
{
    if (!data || count < CELLS)
        return;

    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            const int idx = row * COLS + col;
            m_values[row][col] = data[idx];
        }
    }

    // ======================================================
    // TEMP DEBUG PHASE 2:
    // Dump raw snapshot matrix exactly as stored in BedFrameStore.
    // This verifies whether corruption happens before rendering.
    // ======================================================

   /* qDebug() << "========== RAW SNAPSHOT ==========";

    for (int r = 0; r < 32; ++r)
    {
        QString line;

        for (int c = 0; c < 16; ++c)
        {
            line += QString("%1 ")
            .arg(m_values[r][c], 3, 10, QChar('0'));
        }

        qDebug().noquote() << QString("R%1: ").arg(r, 2, 10, QChar('0')) + line;
    }

    qDebug() << "==================================";

    m_frameId = frameId;
    m_hasFrame = true;

    emit snapshotUpdated(frameId);
    emit frameUpdated(frameId);*/
}

// ذخیره status تخت (packet 0x22)
void BedFrameStore::setStatus(quint16 frameId, const quint8 *data, int count)
{
    if (!data || count < CELLS)
        return;

    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            const int idx = row * COLS + col;
            m_status[row][col] = data[idx];
        }
    }

    m_frameId = frameId;
    m_hasFrame = true;

    emit statusUpdated(frameId);
    emit frameUpdated(frameId);
}
