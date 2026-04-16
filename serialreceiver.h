#pragma once
#include <QObject>
#include <QSerialPort>
#include <QByteArray>
#include <array>
#include "summarydata.h"

struct NodePacket {
    quint8 type;     // Byte2
    quint8 seq;      // Byte3
    quint8 nodeId;   // Byte4
    quint16 cycle;   // Byte5-6 (LSB first)
    quint8 flags;    // Byte7  -> bits[1:0] = node_state
    std::array<quint8, 32> sensors; // Byte8-39, each raw sensor byte
    quint8 crc0;     // Byte40
    quint8 crc1;     // Byte41
};

// packet نوع 0x20
// کل snapshot تخت 32x16 را به صورت row-major نگه می‌دارد
struct BedSnapshotPacket {
    quint8 type;        // Byte2 = 0x20
    quint8 seq;         // Byte3
    quint16 frameId;    // Byte4-5
    quint8 rows;        // Byte6
    quint8 cols;        // Byte7
    std::array<quint8, 512> values; // Byte8-519
    quint8 crc0;        // Byte520
    quint8 crc1;        // Byte521
};

// packet نوع 0x22
// status هر سلول تخت 32x16 را به صورت row-major نگه می‌دارد
struct BedStatusPacket {
    quint8 type;        // Byte2 = 0x22
    quint8 seq;         // Byte3
    quint16 frameId;    // Byte4-5
    quint8 rows;        // Byte6
    quint8 cols;        // Byte7
    std::array<quint8, 512> status; // Byte8-519
    quint8 crc0;        // Byte520
    quint8 crc1;        // Byte521
};

// packet نوع 0x30
// وضعیت همه 16 نود را یکجا نگه می‌دارد
struct NodeHealthPacket {
    quint8 type;        // Byte2 = 0x30
    quint8 seq;         // Byte3
    quint16 frameId;    // Byte4-5
    quint8 nodeCount;   // Byte6
    quint8 reserved;    // Byte7
    std::array<quint8, 16> nodeState; // Byte8-23
    quint8 crc0;        // Byte24
    quint8 crc1;        // Byte25
};

class SerialReceiver : public QObject {
    Q_OBJECT
public:
    explicit SerialReceiver(QObject *parent = nullptr);

    void attach(QSerialPort *port);
    void detach();

signals:
    void packetReceived(const NodePacket &pkt);
    void parseError(const QString &msg);
    void bedSnapshotReceived(const BedSnapshotPacket &pkt);  // برای ارسال packet نوع 0x20 به بقیه برنامه
    void bedStatusReceived(const BedStatusPacket &pkt);      // برای ارسال packet نوع 0x22 به بقیه برنامه
    void nodeHealthReceived(const NodeHealthPacket &pkt);  // برای ارسال packet نوع 0x30 به MainWindow
    void summaryReceived(const SummaryData &summary); // برای ارسال داده parse‌شده‌ی packet 0x40 به UI

private slots:
    void onReadyRead();

private:
    void processBuffer();
    bool tryParseOne(NodePacket &out);
    bool tryParseSummary(SummaryData &out);  // برای parse کردن packet نوع 0x40
    bool tryParseBedSnapshot(BedSnapshotPacket &out);  // برای parse کردن packet نوع 0x20
    bool tryParseBedStatus(BedStatusPacket &out);      // برای parse کردن packet نوع 0x22
    bool tryParseNodeHealth(NodeHealthPacket &out);   // برای parse کردن packet نوع 0x30

    QSerialPort *m_port = nullptr;
    QByteArray m_buf;

    static constexpr int PKT_LEN = 42;
    static constexpr quint8 SOF0 = 0xAA;
    static constexpr quint8 SOF1 = 0x55;
    static constexpr quint8 TYPE_NODE32 = 0x10;
    static constexpr quint8 TYPE_SUMMARY = 0x40;  // نوع packet برای Summary high-level metrics
    static constexpr quint8 TYPE_NODE_HEALTH = 0x30;  // نوع packet برای وضعیت 16 نود
    static constexpr quint8 TYPE_BED_SNAPSHOT = 0x20;  // نوع packet برای کل snapshot تخت
    static constexpr quint8 TYPE_BED_STATUS   = 0x22;  // نوع packet برای status کل تخت
};
