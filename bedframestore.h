#pragma once

#include <QObject>
#include <QtGlobal>

// این کلاس قرار است داده‌های تخت 32x16 را از packetهای
// 0x20 (BED_SNAPSHOT) و 0x22 (BED_STATUS) نگه دارد.
// فعلاً فقط storage تعریف می‌کنیم؛ هنوز parsing و UI connection نداریم.
class BedFrameStore : public QObject
{
    Q_OBJECT

public:
    static constexpr int ROWS = 32;
    static constexpr int COLS = 16;
    static constexpr int CELLS = ROWS * COLS;

    explicit BedFrameStore(QObject *parent = nullptr);

    // مقدار distance-like هر سلول
    quint8 value(int row, int col) const;

    // status هر سلول:
    // 0 = OK
    // 1 = WARNING
    // 2 = ERROR
    // 3 = DISCONNECTED
    quint8 status(int row, int col) const;

    // آیا حداقل یک frame معتبر دریافت شده؟
    bool hasFrame() const;

    // frame id آخرین داده‌ای که کامل/نیمه‌کامل ذخیره شده
    quint16 frameId() const;

    // این توابع بعداً توسط parser صدا زده می‌شوند
    void setSnapshot(quint16 frameId, const quint8 *data, int count);
    void setStatus(quint16 frameId, const quint8 *data, int count);

signals:
    // وقتی snapshot جدید می‌رسد
    void snapshotUpdated(quint16 frameId);

    // وقتی status جدید می‌رسد
    void statusUpdated(quint16 frameId);

    // وقتی هر چیزی در frame تغییر کرد
    void frameUpdated(quint16 frameId);

private:
    quint8 m_values[ROWS][COLS] = {};
    quint8 m_status[ROWS][COLS] = {};

    quint16 m_frameId = 0;
    bool m_hasFrame = false;
};
