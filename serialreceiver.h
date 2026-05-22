#pragma once
#include <QObject>
#include <QSerialPort>
#include <QByteArray>
#include <array>
#include "summarydata.h"
#include <QElapsedTimer>
#include <QMap>
/*========================================================================================*/

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

// ======================================================
// Intervention Plan packet data
// Main -> UI
// TYPE = 0x51
//
// این struct اطلاعات intervention پیشنهادی Main Board را نگه می‌دارد.
// UI از این اطلاعات برای نمایش کارت approve/reject استفاده می‌کند.
// ======================================================
struct InterventionPlan
{
    // Unique ID for this intervention plan
    // UI must approve/reject only this active plan_id
    quint16 planId = 0;

    // Target board/node ID involved in the intervention
    quint8 boardId = 0;

    // Body/bed target zone
    // Example: sacrum, heel, shoulder, back, etc.
    quint8 targetZone = 0;

    // Number of motors involved in this intervention
    quint8 motorCount = 0;

    // Risk score reported by Main Board
    quint8 riskScore = 0;

    // Risk level reported by Main Board
    // Example: low/medium/high/critical depending on firmware mapping
    quint8 riskLevel = 0;

    // Recommendation code from Main Board
    // Example: heel elevation, sacrum relief, shoulder relief, etc.
    quint8 recommendationCode = 0;

    // Reason code explaining why this plan was generated
    // Example: high pressure, immobility, exposure timeout, etc.
    quint8 reasonCode = 0;
};


// ======================================================
// Intervention Result packet data
// Main -> UI
// TYPE = 0x54
//
// این struct وضعیت lifecycle اجرای intervention را نگه می‌دارد.
// Main Board بعد از approve/reject این packet را به UI می‌فرستد.
// ======================================================
struct InterventionResult
{
    // Plan ID related to this result packet
    // Note: Main sends this as uint32 in TYPE 0x54
    quint32 planId = 0;

    // Lifecycle state:
    // 0 = IDLE
    // 1 = EXECUTING
    // 2 = COMPLETED
    // 3 = FAILED
    // 4 = REJECTED
    quint8 state = 0;

    // Board/node involved in execution
    quint8 boardId = 0;

    // Number of motors executed or scheduled
    quint8 motorCount = 0;
};

/*========================================================================================*/


class SerialReceiver : public QObject {
    Q_OBJECT
public:
    explicit SerialReceiver(QObject *parent = nullptr);

    void attach(QSerialPort *port);
    void detach();
    // ======================================================
    // UI -> Main Board
    // Send intervention approve command
    // TYPE = 0x52
    //
    // Packet:
    // AA 55 52 SEQ planL planH 12 34
    // ======================================================
    void sendInterventionApprove(quint16 planId);

    // ======================================================
    // UI -> Main Board
    // Send intervention reject command
    // TYPE = 0x53
    //
    // Packet:
    // AA 55 53 SEQ planL planH 12 34
    // ======================================================
    void sendInterventionReject(quint16 planId);
    // ======================================================
    // UI -> Main Board
    // Send debug/simulation command.
    //
    // TYPE = 0x5A
    //
    // Packet:
    // AA 55 5A SEQ command_id paramL paramH 12 34
    //
    // Used for:
    // - simulation
    // - intervention workflow testing
    // - future motor/CAN diagnostics
    // ======================================================
    void sendDebugCommand(quint8 commandId, quint16 param);

signals:
    void packetReceived(const NodePacket &pkt);
    void parseError(const QString &msg);
    void bedSnapshotReceived(const BedSnapshotPacket &pkt);  // برای ارسال packet نوع 0x20 به بقیه برنامه
    void bedStatusReceived(const BedStatusPacket &pkt);      // برای ارسال packet نوع 0x22 به بقیه برنامه
    void nodeHealthReceived(const NodeHealthPacket &pkt);  // برای ارسال packet نوع 0x30 به MainWindow
    void summaryReceived(const SummaryData &summary); // برای ارسال داده parse‌شده‌ی packet 0x40 به UI

    // ======================================================
    // Emitted when Main Board sends an intervention plan
    // TYPE = 0x51
    //
    // MainWindow will receive this signal and show
    // approve/reject UI for the nurse/operator.
    // ======================================================
    void interventionPlanReceived(const InterventionPlan &plan);

    // ======================================================
    // Emitted when Main Board sends intervention lifecycle result
    // TYPE = 0x54
    //
    // MainWindow will update execution status:
    // EXECUTING / COMPLETED / FAILED / REJECTED
    // ======================================================
    void interventionResultReceived(const InterventionResult &result);

private slots:
    void onReadyRead();

private:
    void processBuffer();
    bool tryParseOne(NodePacket &out);
    bool tryParseSummary(SummaryData &out);  // برای parse کردن packet نوع 0x40
    bool tryParseBedSnapshot(BedSnapshotPacket &out);  // برای parse کردن packet نوع 0x20
    bool tryParseBedStatus(BedStatusPacket &out);      // برای parse کردن packet نوع 0x22
    bool tryParseNodeHealth(NodeHealthPacket &out);   // برای parse کردن packet نوع 0x30
    // ======================================================
    // UI transmit sequence counter
    //
    // Every UI -> Main command gets one SEQ byte.
    // It auto-increments for approve/reject packets.
    // ======================================================
    quint8 m_uiTxSeq = 0;

    // ======================================================
    // Parse Main -> UI intervention plan packet
    // TYPE = 0x51
    //
    // Returns true if packet is valid and parsed successfully.
    // ======================================================
    bool tryParseInterventionPlan(InterventionPlan &out);


    // ======================================================
    // Parse Main -> UI intervention result packet
    // TYPE = 0x54
    //
    // Returns true if packet is valid and parsed successfully.
    // ======================================================
    bool tryParseInterventionResult(InterventionResult &out);

    // ======================================================
    // TEMP FAST-PATH handlers.
    //
    // These detect critical intervention packets directly
    // from raw UART bytes before normal parser processing.
    //
    // Reason:
    // Current UART carries both large data packets and small
    // command/event packets. Fast-path prevents intervention
    // events from being delayed behind large 0x20 / 0x22 frames.
    //
    // TODO:
    // Remove after command/data channels are separated or
    // protocol gets LEN + robust framing.
    // ======================================================
    void processInterventionPlanFastPath(const QByteArray &rx);
    void processInterventionResultFastPath(const QByteArray &rx);


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
    // ======================================================
    // Intervention / Therapy packet types
    // ======================================================

    // Main -> UI
    // Suggested intervention plan
    static constexpr quint8 TYPE_INTERVENTION_PLAN = 0x51;

    // UI -> Main
    // Nurse/operator approved intervention
    static constexpr quint8 TYPE_INTERVENTION_APPROVE = 0x52;

    // UI -> Main
    // Nurse/operator rejected intervention
    static constexpr quint8 TYPE_INTERVENTION_REJECT = 0x53;

    // Main -> UI
    // Intervention lifecycle/result packet
    static constexpr quint8 TYPE_INTERVENTION_RESULT = 0x54;

    // UI -> Main Board
    // Debug/simulation command packet
    static constexpr quint8 TYPE_DEBUG_COMMAND = 0x5A;

    // ======================================================
    // UART Packet Statistics
    //
    // Purpose:
    // Detect packet delay, seq jumps, and possible packet loss.
    //
    // This does NOT change parser behavior.
    // It only monitors successfully parsed packets.
    // ======================================================
    struct PacketStats
    {
        bool initialized = false;
        quint8 lastSeq = 0;
        quint32 rxCount = 0;
        quint32 missedCount = 0;
        qint64 lastRxMs = 0;
        qint64 maxGapMs = 0;
    };

    QElapsedTimer m_rxStatsClock;
    QMap<quint8, PacketStats> m_packetStats;

    // Called after a packet is successfully parsed.
    void updatePacketStats(quint8 type, quint8 seq, quint16 frameId = 0);
};
/*========================================================================================*/
