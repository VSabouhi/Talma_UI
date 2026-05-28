#include "serialreceiver.h"
#include "talma_debug.h"
#include <QDebug>

/*========================================================================================*/

/*========================================================================================*/

SerialReceiver::SerialReceiver(QObject *parent) : QObject(parent)
{
    m_buf.reserve(4096);
}
/*========================================================================================*/

void SerialReceiver::attach(QSerialPort *port)
{
    if (m_port == port) return;

    detach();
    m_port = port;

    if (m_port) {
        connect(m_port, &QSerialPort::readyRead,
                this, &SerialReceiver::onReadyRead);
        m_buf.clear();
    }
}
/*========================================================================================*/

void SerialReceiver::detach()
{
    if (!m_port) return;

    disconnect(m_port, &QSerialPort::readyRead,
               this, &SerialReceiver::onReadyRead);

    m_port = nullptr;
    m_buf.clear();
}
/*========================================================================================*/

void SerialReceiver::onReadyRead()
{
    if (!m_port)
        return;

    const QByteArray rx = m_port->readAll();

    if (rx.isEmpty())
        return;


    // ======================================================
    // TEMP DEBUG PHASE 7:
    // Monitor distances between packet headers in the raw UART
    // stream before parser buffering. This reveals the actual
    // packet length produced by the Main Board.
    // ======================================================
   /* static QByteArray rawMonitor;
    rawMonitor.append(rx);

    while (true) {
        const int first = rawMonitor.indexOf(QByteArray::fromHex("AA55"));

        if (first < 0) {
            if (rawMonitor.size() > 1)
                rawMonitor = rawMonitor.right(1);
            break;
        }

        if (first > 0)
            rawMonitor.remove(0, first);

        const int second = rawMonitor.indexOf(QByteArray::fromHex("AA55"), 2);

        if (second < 0) {
            if (rawMonitor.size() > 2048)
                rawMonitor.remove(0, rawMonitor.size() - 2048);
            break;
        }

        const quint8 type = quint8(rawMonitor[2]);
        const quint8 seq = quint8(rawMonitor[3]);

        qWarning() << "[RAW PACKET DISTANCE]"
                   << "type =" << QString("0x%1")
                                      .arg(type, 2, 16, QLatin1Char('0'))
                                      .toUpper()
                   << "seq =" << seq
                   << "distanceToNextSof =" << second
                   << "footer =" << QString("0x%1 0x%2")
                                        .arg(quint8(rawMonitor[second - 2]), 2, 16, QLatin1Char('0'))
                                        .arg(quint8(rawMonitor[second - 1]), 2, 16, QLatin1Char('0'))
                                        .toUpper();

        rawMonitor.remove(0, second);
    }*/

    // ======================================================
    // TEMP FAST-PATH:
    // Critical intervention events are checked before normal
    // parser processing to avoid delay behind large packets.
    // ======================================================
    processInterventionPlanFastPath(rx);
    processInterventionResultFastPath(rx);

    // ======================================================
    // Normal parser path
    // ======================================================
    m_buf.append(rx);
    processBuffer();
}
/*========================================================================================*/

// ======================================================
// TEMP FAST-PATH:
// Detect exact fake INTERVENTION_PLAN packet directly
// from raw UART stream.
// ======================================================
void SerialReceiver::processInterventionPlanFastPath(const QByteArray &rx)
{

    static QByteArray planRxMonitorBuffer;

    planRxMonitorBuffer.append(rx);

    if (planRxMonitorBuffer.size() > 256)
        planRxMonitorBuffer.remove(0, planRxMonitorBuffer.size() - 256);

    const QByteArray expectedPlan =
        QByteArray::fromHex("AA555100010002030C800301021234");

    if (!planRxMonitorBuffer.contains(expectedPlan))
        return;

    #if TALMA_DEBUG_UART
        qDebug() << "[UART FAST-PATH] TEST INTERVENTION_PLAN detected";
    #endif

    InterventionPlan plan;
    plan.planId = 1;
    plan.boardId = 2;
    plan.targetZone = 3;
    plan.motorCount = 12;
    plan.riskScore = 128;
    plan.riskLevel = 3;
    plan.recommendationCode = 1;
    plan.reasonCode = 2;

    emit interventionPlanReceived(plan);

    planRxMonitorBuffer.clear();
}

/*========================================================================================*/

// ======================================================
// TEMP FAST-PATH:
// Detect and decode INTERVENTION_RESULT packets directly
// from raw UART stream.
// ======================================================
void SerialReceiver::processInterventionResultFastPath(const QByteArray &rx)
{

    static QByteArray resultRxMonitorBuffer;

    resultRxMonitorBuffer.append(rx);

    if (resultRxMonitorBuffer.size() > 256)
        resultRxMonitorBuffer.remove(0, resultRxMonitorBuffer.size() - 256);

    const int resultIdx =
        resultRxMonitorBuffer.indexOf(QByteArray::fromHex("AA5554"));

    if (resultIdx < 0)
        return;

    static constexpr int RESULT_LEN = 13;

    if (resultRxMonitorBuffer.size() < resultIdx + RESULT_LEN)
        return;

    const QByteArray pkt = resultRxMonitorBuffer.mid(resultIdx, RESULT_LEN);

    if ((quint8)pkt[11] != 0x12 || (quint8)pkt[12] != 0x34)
        return;

    InterventionResult result;

    result.planId =
        quint32((quint8)pkt[4]) |
        (quint32((quint8)pkt[5]) << 8) |
        (quint32((quint8)pkt[6]) << 16) |
        (quint32((quint8)pkt[7]) << 24);

    result.state = (quint8)pkt[8];
    result.boardId = (quint8)pkt[9];
    result.motorCount = (quint8)pkt[10];

    #if TALMA_DEBUG_UART
        qDebug() << "[UART FAST-PATH] INTERVENTION_RESULT"
                 << "plan_id =" << result.planId
                 << "state =" << result.state
                 << "board =" << result.boardId
                 << "motors =" << result.motorCount;
    #endif

    emit interventionResultReceived(result);

    resultRxMonitorBuffer.remove(0, resultIdx + RESULT_LEN);
}

/*========================================================================================*/

// ======================================================
// UART Packet Statistics
//
// Tracks:
// - received packet count per TYPE
// - SEQ jumps / possible missed packets
// - time gap between packets of same TYPE
//
// This function is called only after successful parsing.
// ======================================================
void SerialReceiver::updatePacketStats(quint8 type, quint8 seq, quint16 frameId)
{
    if (!m_rxStatsClock.isValid())
        m_rxStatsClock.start();

    const qint64 nowMs = m_rxStatsClock.elapsed();

    PacketStats &stats = m_packetStats[type];

    if (!stats.initialized) {
        stats.initialized = true;
        stats.lastSeq = seq;
        stats.rxCount = 1;
        stats.lastRxMs = nowMs;

        #if TALMA_DEBUG_SERIAL_STATS
                qDebug() << "[SERIAL STATS INIT]"
                         << "type =" << QString("0x%1")
                                            .arg(type, 2, 16, QLatin1Char('0'))
                                            .toUpper()
                         << "seq =" << seq
                         << "frame =" << frameId;
        #endif

        return;
    }

   /* const quint8 expectedSeq = quint8(stats.lastSeq + 1);

    if (seq != expectedSeq) {
        const quint8 missed = quint8(seq - expectedSeq);

        stats.missedCount += missed;

        qWarning() << "[SERIAL SEQ JUMP]"
                   << "type =" << QString("0x%1").arg(type, 2, 16, QLatin1Char('0')).toUpper()
                   << "expected =" << expectedSeq
                   << "got =" << seq
                   << "missed =" << missed
                   << "totalMissed =" << stats.missedCount
                   << "frame =" << frameId;
    }*/

    const qint64 gapMs = nowMs - stats.lastRxMs;

    if (gapMs > stats.maxGapMs)
        stats.maxGapMs = gapMs;

    // Log only abnormal delay to keep terminal clean.
    if (gapMs > 1500) {
        #if TALMA_DEBUG_SERIAL_STATS
                qWarning() << "[SERIAL GAP]"
                           << "type =" << QString("0x%1")
                                              .arg(type, 2, 16, QLatin1Char('0'))
                                              .toUpper()
                           << "gapMs =" << gapMs
                           << "maxGapMs =" << stats.maxGapMs
                           << "frame =" << frameId;
        #endif
    }

    stats.lastSeq = seq;
    stats.rxCount++;
    stats.lastRxMs = nowMs;
}
/*========================================================================================*/

void SerialReceiver::processBuffer()
{
    while (true) {
        int sof = -1;
        for (int i = 0; i + 1 < m_buf.size(); ++i) {
            if ((quint8)m_buf[i] == SOF0 && (quint8)m_buf[i + 1] == SOF1) {
                sof = i;
                break;
            }
        }

        if (sof < 0) {
            if (!m_buf.isEmpty() && (quint8)m_buf.back() == SOF0)
                m_buf = m_buf.right(1);
            else
                m_buf.clear();
            return;
        }

        if (sof > 0)
            m_buf.remove(0, sof);

        // فعلاً فقط برای خواندن TYPE به 3 بایت اول نیاز داریم
        if (m_buf.size() < 3)
            return;


        const quint8 type = quint8(m_buf[2]);

        // ======================================================
        // Safety guard against parser deadlock.
        //
        // If UART stream becomes misaligned and parser waits forever
        // for a large packet (0x20 / 0x22), buffer may grow endlessly.
        //
        // Instead of freezing UI updates, slowly resync by dropping
        // one byte at a time only when buffer becomes abnormally large.
        // ======================================================
        if (m_buf.size() > 8192) {
            qWarning() << "[SERIAL] buffer overflow, resync";

            emit parseError("Serial buffer overflow/resync");

            m_buf.remove(0, 1);
            continue;
        }
        // ======================================================
        // TEMP DEBUG:
        // Verify parser reaches TYPE 0x51 branch.
        // ======================================================
        if (type == TYPE_INTERVENTION_PLAN) {
           // qDebug() << "[PARSER] TYPE_INTERVENTION_PLAN detected";
        }

        // ---------------- NODE32 ----------------
        if (type == TYPE_NODE32) {
            if (m_buf.size() < PKT_LEN)
                return;

            NodePacket pkt;
            if (!tryParseOne(pkt)) {
                m_buf.remove(0, 1);
                emit parseError("Invalid NODE32 packet, resyncing...");
                continue;
            }

            m_buf.remove(0, PKT_LEN);
            emit packetReceived(pkt);
        }
        // ---------------- BED SNAPSHOT (0x20) ----------------
        else if (type == TYPE_BED_SNAPSHOT) {
            static constexpr int BED_PKT_LEN = 522;

            if (m_buf.size() < BED_PKT_LEN)
                return;

            // ======================================================
            // TEMP DEBUG PHASE 3:
            // Detect packet headers inside the expected BED_SNAPSHOT
            // payload area. If 0xAA 0x55 appears before byte 520,
            // the UI parser is consuming bytes from another packet as
            // snapshot payload, or the expected packet length is wrong.
            // ======================================================
           /* for (int i = 8; i < 520; ++i) {
                if (quint8(m_buf[i]) == SOF0 && quint8(m_buf[i + 1]) == SOF1) {
                    qWarning() << "[BED_SNAPSHOT PAYLOAD SOF]"
                               << "offset =" << i
                               << "typeAfterSof =" << QString("0x%1")
                                                          .arg(quint8(m_buf[i + 2]), 2, 16, QLatin1Char('0'))
                                                          .toUpper()
                               << "currentSeq =" << quint8(m_buf[3])
                               << "currentFrame =" << (quint16(quint8(m_buf[4])) |
                                                       (quint16(quint8(m_buf[5])) << 8));
                }
            }*/


            BedSnapshotPacket pkt;
            if (!tryParseBedSnapshot(pkt)) {
                // ======================================================
                // TEMP DEBUG PHASE 5:
                // Fast resync after an invalid large BED_SNAPSHOT packet.
                // If the parser locked onto a false AA 55 20 sequence,
                // dropping only one byte is too slow and may keep the UI
                // inside corrupted 522-byte windows.
                // ======================================================
                int nextSof = -1;

                for (int i = 2; i + 1 < m_buf.size(); ++i) {
                    if (quint8(m_buf[i]) == SOF0 && quint8(m_buf[i + 1]) == SOF1) {
                        nextSof = i;
                        break;
                    }
                }

                if (nextSof > 0) {
                    qWarning() << "[BED_SNAPSHOT RESYNC]"
                               << "dropBytes =" << nextSof
                               << "nextType =" << QString("0x%1")
                                                      .arg(quint8(m_buf[nextSof + 2]), 2, 16, QLatin1Char('0'))
                                                      .toUpper();

                    m_buf.remove(0, nextSof);
                } else {
                    m_buf.remove(0, 1);
                }

                emit parseError("Invalid BED_SNAPSHOT packet, resyncing...");
                continue;
            }

            m_buf.remove(0, BED_PKT_LEN);

            updatePacketStats(TYPE_BED_SNAPSHOT, pkt.seq, pkt.frameId);

            emit bedSnapshotReceived(pkt);

            #if TALMA_DEBUG_UART
                qDebug() << "[RX OK] BED_SNAPSHOT"
                         << "frame =" << pkt.frameId
                         << "seq =" << pkt.seq;
            #endif

        }
        // ---------------- BED STATUS (0x22) ----------------
        else if (type == TYPE_BED_STATUS) {
            static constexpr int BED_PKT_LEN = 522;

            if (m_buf.size() < BED_PKT_LEN)
                return;

            BedStatusPacket pkt;
            if (!tryParseBedStatus(pkt)) {
                // ======================================================
                // TEMP DEBUG PHASE 5:
                // Fast resync after an invalid large BED_STATUS packet.
                // This prevents the parser from staying locked inside a
                // corrupted 522-byte window.
                // ======================================================
                int nextSof = -1;

                for (int i = 2; i + 1 < m_buf.size(); ++i) {
                    if (quint8(m_buf[i]) == SOF0 && quint8(m_buf[i + 1]) == SOF1) {
                        nextSof = i;
                        break;
                    }
                }

                if (nextSof > 0) {
                    qWarning() << "[BED_STATUS RESYNC]"
                               << "dropBytes =" << nextSof
                               << "nextType =" << QString("0x%1")
                                                      .arg(quint8(m_buf[nextSof + 2]), 2, 16, QLatin1Char('0'))
                                                      .toUpper();

                    m_buf.remove(0, nextSof);
                } else {
                    m_buf.remove(0, 1);
                }

                emit parseError("Invalid BED_STATUS packet, resyncing...");
                continue;
            }

            m_buf.remove(0, BED_PKT_LEN);

            updatePacketStats(TYPE_BED_STATUS, pkt.seq, pkt.frameId);

            emit bedStatusReceived(pkt);

            #if TALMA_DEBUG_UART
                qDebug() << "[RX OK] BED_STATUS"
                         << "frame =" << pkt.frameId
                         << "seq =" << pkt.seq;
            #endif
        }
        // ---------------- NODE HEALTH (0x30) ----------------
        else if (type == TYPE_NODE_HEALTH) {
            static constexpr int NODE_HEALTH_LEN = 26;

            if (m_buf.size() < NODE_HEALTH_LEN)
                return;

            NodeHealthPacket nh;
            if (!tryParseNodeHealth(nh)) {
                m_buf.remove(0, 1);
                emit parseError("Invalid NODE_HEALTH packet, resyncing...");
                continue;
            }

            // توجه:
            // tryParseNodeHealth خودش packet را از buffer حذف می‌کند
            updatePacketStats(TYPE_NODE_HEALTH, nh.seq, nh.frameId);

            emit nodeHealthReceived(nh);
        }
        // ---------------- SUMMARY (0x40) ----------------
        else if (type == TYPE_SUMMARY) {
            static constexpr int SUMMARY_LEN = 37;

            if (m_buf.size() < SUMMARY_LEN)
                return;

            SummaryData summary;
            if (!tryParseSummary(summary)) {
                m_buf.remove(0, 1);
                emit parseError("Invalid SUMMARY packet, resyncing...");
                continue;
            }

            // Save SEQ before removing packet from buffer.
            const quint8 seq = quint8(m_buf[3]);

            m_buf.remove(0, SUMMARY_LEN);

            updatePacketStats(TYPE_SUMMARY, seq, summary.frameId);

            emit summaryReceived(summary);


           /* qDebug() << "SUMMARY parsed:"
                     << "frameId =" << summary.frameId
                     << "risk =" << summary.riskScore
                     << "movement =" << summary.timeSinceLastMovementS;*/


            #if TALMA_DEBUG_UART
            qDebug() << "[RX OK] SUMMARY"
                     << "frame =" << summary.frameId;
            #endif

        }
        // ---------------- INTERVENTION PLAN (0x51) ----------------
        else if (type == TYPE_INTERVENTION_PLAN) {
            // Packet:
            // AA 55 51 SEQ
            // planL planH
            // board_id
            // target_zone
            // motor_count
            // risk_score
            // risk_level
            // recommendation_code
            // reason_code
            // 12 34
            //
            // Total length = 15 bytes

            //qDebug() << "[PARSER] Entered INTERVENTION_PLAN branch";


            static constexpr int INTERVENTION_PLAN_LEN = 15;

            // هنوز کل packet نرسیده؛ صبر کن تا بقیه byteها برسند.
            if (m_buf.size() < INTERVENTION_PLAN_LEN)
                return;

            InterventionPlan plan;

            if (!tryParseInterventionPlan(plan)) {
                qWarning() << "[INTERVENTION_PLAN] invalid packet:"
                           << m_buf.left(INTERVENTION_PLAN_LEN).toHex(' ');

                m_buf.remove(0, 1);
                emit parseError("Invalid INTERVENTION_PLAN packet, resyncing...");
                continue;
            }

            emit interventionPlanReceived(plan);
        }

        // ---------------- INTERVENTION RESULT (0x54) ----------------
        else if (type == TYPE_INTERVENTION_RESULT) {
            // Packet:
            // AA 55 54 SEQ plan0 plan1 plan2 plan3 state board_id motor_count 12 34
            //
            // Main Board reports lifecycle state:
            // EXECUTING / COMPLETED / FAILED / REJECTED
            InterventionResult result;

            if (!tryParseInterventionResult(result)) {
                m_buf.remove(0, 1);
                emit parseError("Invalid INTERVENTION_RESULT packet, resyncing...");
                continue;
            }

            emit interventionResultReceived(result);
        }
        // ---------------- UNKNOWN ----------------
        else {
            m_buf.remove(0, 1);
            emit parseError(QString("Unknown packet type: 0x%1")
                                .arg(type, 2, 16, QLatin1Char('0')).toUpper());
        }
    }
}
/*========================================================================================*/

bool SerialReceiver::tryParseOne(NodePacket &out)
{
    if (m_buf.size() < PKT_LEN) return false;
    if ((quint8)m_buf[0] != SOF0 || (quint8)m_buf[1] != SOF1) return false;

    out.type   = (quint8)m_buf[2];
    out.seq    = (quint8)m_buf[3];
    out.nodeId = (quint8)m_buf[4];
    out.cycle  = quint16((quint8)m_buf[5]) | (quint16((quint8)m_buf[6]) << 8);
    out.flags  = (quint8)m_buf[7];

    if (out.type != TYPE_NODE32) return false;
    if (out.nodeId >= 16) return false;

    for (int i = 0; i < 32; ++i)
        out.sensors[i] = (quint8)m_buf[8 + i];

    out.crc0 = (quint8)m_buf[40];
    out.crc1 = (quint8)m_buf[41];

    return true;
}
/*========================================================================================*/

bool SerialReceiver::tryParseBedSnapshot(BedSnapshotPacket &out)
{
    static constexpr int BED_PKT_LEN = 522;

    // هنوز کل packet نرسیده
    if (m_buf.size() < BED_PKT_LEN)
        return false;

    // هدر باید درست باشد
    if ((quint8)m_buf[0] != SOF0 || (quint8)m_buf[1] != SOF1)
        return false;

    // این parser فقط برای 0x20 است
    if ((quint8)m_buf[2] != TYPE_BED_SNAPSHOT)
        return false;

    out.type = (quint8)m_buf[2];
    out.seq = (quint8)m_buf[3];
    out.frameId = quint16((quint8)m_buf[4]) | (quint16((quint8)m_buf[5]) << 8);
    out.rows = (quint8)m_buf[6];
    out.cols = (quint8)m_buf[7];

    // فعلاً انتظار داریم دقیقاً 32x16 باشد
    if (out.rows != 32 || out.cols != 16)
        return false;

    for (int i = 0; i < 512; ++i)
        out.values[i] = (quint8)m_buf[8 + i];

    out.crc0 = (quint8)m_buf[520];
    out.crc1 = (quint8)m_buf[521];

    // ======================================================
    // TEMP DEBUG PHASE 4:
    // Validate fixed packet footer.
    // Without this check, a false AA 55 20 sequence inside the
    // UART stream can be accepted as a valid BED_SNAPSHOT.
    // ======================================================
    if (out.crc0 != 0x12 || out.crc1 != 0x34) {
        qWarning() << "[BED_SNAPSHOT INVALID FOOTER]"
                   << "seq =" << out.seq
                   << "frame =" << out.frameId
                   << "crc0 =" << QString("0x%1").arg(out.crc0, 2, 16, QLatin1Char('0')).toUpper()
                   << "crc1 =" << QString("0x%1").arg(out.crc1, 2, 16, QLatin1Char('0')).toUpper();
        return false;
    }

    return true;
}
/*========================================================================================*/
bool SerialReceiver::tryParseBedStatus(BedStatusPacket &out)
{
    static constexpr int BED_PKT_LEN = 522;

    // هنوز کل packet نرسیده
    if (m_buf.size() < BED_PKT_LEN)
        return false;

    // هدر باید درست باشد
    if ((quint8)m_buf[0] != SOF0 || (quint8)m_buf[1] != SOF1)
        return false;

    // این parser فقط برای 0x22 است
    if ((quint8)m_buf[2] != TYPE_BED_STATUS)
        return false;

    out.type = (quint8)m_buf[2];
    out.seq = (quint8)m_buf[3];
    out.frameId = quint16((quint8)m_buf[4]) | (quint16((quint8)m_buf[5]) << 8);
    out.rows = (quint8)m_buf[6];
    out.cols = (quint8)m_buf[7];

    // فعلاً انتظار داریم دقیقاً 32x16 باشد
    if (out.rows != 32 || out.cols != 16)
        return false;

    for (int i = 0; i < 512; ++i)
        out.status[i] = (quint8)m_buf[8 + i];

    out.crc0 = (quint8)m_buf[520];
    out.crc1 = (quint8)m_buf[521];

    // ======================================================
    // TEMP DEBUG PHASE 4:
    // Validate fixed packet footer.
    // Without this check, a false AA 55 22 sequence inside the
    // UART stream can be accepted as a valid BED_STATUS.
    // ======================================================
    if (out.crc0 != 0x12 || out.crc1 != 0x34) {
        qWarning() << "[BED_STATUS INVALID FOOTER]"
                   << "seq =" << out.seq
                   << "frame =" << out.frameId
                   << "crc0 =" << QString("0x%1").arg(out.crc0, 2, 16, QLatin1Char('0')).toUpper()
                   << "crc1 =" << QString("0x%1").arg(out.crc1, 2, 16, QLatin1Char('0')).toUpper();
        return false;
    }

    return true;
}

/*========================================================================================*/

bool SerialReceiver::tryParseNodeHealth(NodeHealthPacket &out)
{
    // طول کل packet باید حداقل 26 بایت باشد
    if (m_buf.size() < 26)
        return false;

    const quint8 *d = reinterpret_cast<const quint8*>(m_buf.constData());

    // بررسی header
    if (d[0] != 0xAA || d[1] != 0x55)
        return false;

    if (d[2] != TYPE_NODE_HEALTH)
        return false;

    // پر کردن struct
    out.type = d[2];
    out.seq  = d[3];

    out.frameId = quint16(d[4]) | (quint16(d[5]) << 8);

    out.nodeCount = d[6];
    out.reserved  = d[7];

    for (int i = 0; i < 16; ++i) {
        out.nodeState[i] = d[8 + i];
    }

    out.crc0 = d[24];
    out.crc1 = d[25];

    // حذف packet از buffer
    m_buf.remove(0, 26);

    return true;
}
/*========================================================================================*/

bool SerialReceiver::tryParseSummary(SummaryData &out)
{
    static constexpr int SUMMARY_LEN = 37;

    // اگر هنوز کل packet نرسیده، parse نکن
    if (m_buf.size() < SUMMARY_LEN)
        return false;

    // header باید درست باشد
    if ((quint8)m_buf[0] != SOF0 || (quint8)m_buf[1] != SOF1)
        return false;

    // این parser فقط برای packet نوع SUMMARY است
    const quint8 type = (quint8)m_buf[2];
    if (type != TYPE_SUMMARY)
        return false;

    // فیلدهای packet طبق protocol specification
    out.frameId = quint16((quint8)m_buf[4]) | (quint16((quint8)m_buf[5]) << 8);

    out.frameId = quint16((quint8)m_buf[4]) | (quint16((quint8)m_buf[5]) << 8);

    out.uptimeS = quint16((quint8)m_buf[6]) | (quint16((quint8)m_buf[7]) << 8);

    out.riskScore = (quint8)m_buf[8];
    out.riskLevel = (quint8)m_buf[9];

    out.movementDetected = (quint8)m_buf[10];
    out.timeSinceLastMovementS = quint16((quint8)m_buf[11])
                                 | (quint16((quint8)m_buf[12]) << 8);

    out.alertActive = (quint8)m_buf[13];
    out.alertType = (quint8)m_buf[14];
    out.alertSeverity = (quint8)m_buf[15];
    out.alertDurationS = quint16((quint8)m_buf[16])
                         | (quint16((quint8)m_buf[17]) << 8);

    out.recommendationCode = (quint8)m_buf[18];
    out.recommendationPriority = (quint8)m_buf[19];

    out.sacrumAvg = (quint8)m_buf[20];
    out.sacrumPeak = (quint8)m_buf[21];

    out.heelLeftAvg = (quint8)m_buf[22];
    out.heelRightAvg = (quint8)m_buf[23];

    out.shouldersAvg = (quint8)m_buf[24];
    out.shouldersPeak = (quint8)m_buf[25];

    out.pressureExposureThreshold = (quint8)m_buf[26];

    out.sacrumExposureS = quint16((quint8)m_buf[27])
                          | (quint16((quint8)m_buf[28]) << 8);

    out.heelsExposureS = quint16((quint8)m_buf[29])
                         | (quint16((quint8)m_buf[30]) << 8);

    out.shouldersExposureS = quint16((quint8)m_buf[31])
                             | (quint16((quint8)m_buf[32]) << 8);

    out.zonesValidMask = (quint8)m_buf[33];
    out.summaryFlags = (quint8)m_buf[34];

    // فعلاً CRC روی برد placeholder است، پس اینجا validate واقعی نمی‌کنیم
    return true;
}
/*========================================================================================*/
// ======================================================
// UI -> Main Board
// Send intervention approve command
//
// TYPE = 0x52
//
// Packet format:
// Byte0 = 0xAA
// Byte1 = 0x55
// Byte2 = 0x52
// Byte3 = SEQ
// Byte4 = plan_id low byte
// Byte5 = plan_id high byte
// Byte6 = 0x12
// Byte7 = 0x34
//
// Meaning:
// Nurse/operator approved the currently pending plan.
// ======================================================
void SerialReceiver::sendInterventionApprove(quint16 planId)
{
    QByteArray packet;

    // Header
    packet.append(char(0xAA));
    packet.append(char(0x55));

    // Packet type: APPROVE_INTERVENTION
    packet.append(char(0x52));

    // UI transmit sequence number
    packet.append(char(m_uiTxSeq++));

    // plan_id, little-endian
    packet.append(char(planId & 0xFF));
    packet.append(char((planId >> 8) & 0xFF));

    // Dummy/static CRC used by current Main Board firmware
    packet.append(char(0x12));
    packet.append(char(0x34));

    // Send only when serial port is valid and open
    if (m_port && m_port->isOpen()) {
        m_port->write(packet);

        qDebug() << "[UI->MAIN] APPROVE sent"
                 << "plan_id =" << planId
                 << "raw =" << packet.toHex(' ');
    } else {
        qWarning() << "[UI->MAIN] APPROVE not sent: serial port is closed";
    }
}


// ======================================================
// UI -> Main Board
// Send intervention reject command
//
// TYPE = 0x53
//
// Packet format:
// Byte0 = 0xAA
// Byte1 = 0x55
// Byte2 = 0x53
// Byte3 = SEQ
// Byte4 = plan_id low byte
// Byte5 = plan_id high byte
// Byte6 = 0x12
// Byte7 = 0x34
//
// Meaning:
// Nurse/operator rejected the currently pending plan.
// ======================================================
void SerialReceiver::sendInterventionReject(quint16 planId)
{
    QByteArray packet;

    // Header
    packet.append(char(0xAA));
    packet.append(char(0x55));

    // Packet type: REJECT_INTERVENTION
    packet.append(char(0x53));

    // UI transmit sequence number
    packet.append(char(m_uiTxSeq++));

    // plan_id, little-endian
    packet.append(char(planId & 0xFF));
    packet.append(char((planId >> 8) & 0xFF));

    // Dummy/static CRC used by current Main Board firmware
    packet.append(char(0x12));
    packet.append(char(0x34));

    // Send only when serial port is valid and open
    if (m_port && m_port->isOpen()) {
        m_port->write(packet);

        qDebug() << "[UI->MAIN] REJECT sent"
                 << "plan_id =" << planId
                 << "raw =" << packet.toHex(' ');
    } else {
        qWarning() << "[UI->MAIN] REJECT not sent: serial port is closed";
    }
}


// ======================================================
// UI -> Main Board
// Send debug/simulation command.
//
// TYPE = 0x5A
//
// Packet format:
// Byte0 = 0xAA
// Byte1 = 0x55
// Byte2 = 0x5A
// Byte3 = SEQ
// Byte4 = command_id
// Byte5 = param low byte
// Byte6 = param high byte
// Byte7 = 0x12
// Byte8 = 0x34
//
// command_id examples:
// 1 = ask Main Board to send fake INTERVENTION_PLAN
//
// This is used only for development/debug/service tools.
// ======================================================
void SerialReceiver::sendDebugCommand(quint8 commandId, quint16 param)
{
    QByteArray packet;

    // Header
    packet.append(char(0xAA));
    packet.append(char(0x55));

    // Packet type: DEBUG_COMMAND
    packet.append(char(TYPE_DEBUG_COMMAND));

    // UI TX sequence counter
    packet.append(char(m_uiTxSeq++));

    // Debug command ID
    packet.append(char(commandId));

    // Debug parameter, 8-bit only.
    // Current use: test plan_id / simulation mode selector.
    packet.append(char(param & 0xFF));

    // Dummy/static CRC
    packet.append(char(0x12));
    packet.append(char(0x34));

    if (m_port && m_port->isOpen()) {
        m_port->write(packet);

        qDebug() << "[UI->MAIN] DEBUG_COMMAND sent"
                 << "cmd =" << commandId
                 << "param =" << param
                 << "raw =" << packet.toHex(' ');
    } else {
        qWarning() << "[UI->MAIN] DEBUG_COMMAND not sent: serial port is closed";
    }
}


// ======================================================
// Send runtime therapy/risk preset selection command
// to the Main Board.
//
// Packet format:
// AA 55 60 SEQ preset 00 12 34
// ======================================================
void SerialReceiver::sendTherapyPreset(quint8 preset)
{
    QByteArray packet;

    packet.append(char(0xAA));
    packet.append(char(0x55));
    packet.append(char(TYPE_THERAPY_PRESET));
    packet.append(char(m_uiTxSeq++));
    packet.append(char(preset));
    packet.append(char(0x00));
    packet.append(char(0x12));
    packet.append(char(0x34));

    if (m_port && m_port->isOpen()) {

        // ======================================================
        // Send packet to Main Board UART command channel
        // ======================================================
        m_port->write(packet);

        qDebug() << "[UI->MAIN] THERAPY_PRESET sent"
                 << "preset =" << preset
                 << "raw =" << packet.toHex(' ');

    } else {

        qWarning() << "[UI->MAIN] THERAPY_PRESET not sent:"
                   << "serial port is closed";
    }
}

// ======================================================
// Send runtime synthetic test pattern selection command
// to the Main Board.
//
// Packet format:
// AA 55 61 SEQ pattern_id 00 12 34
// ======================================================
void SerialReceiver::sendTestPattern(quint8 patternId)
{
    QByteArray packet;

    packet.append(char(0xAA));
    packet.append(char(0x55));
    packet.append(char(TYPE_TEST_PATTERN));
    packet.append(char(m_uiTxSeq++));
    packet.append(char(patternId));
    packet.append(char(0x00));
    packet.append(char(0x12));
    packet.append(char(0x34));

    if (m_port && m_port->isOpen()) {

        // ======================================================
        // Send packet to Main Board UART command channel
        // ======================================================
        m_port->write(packet);

        qDebug() << "[UI->MAIN] TEST_PATTERN sent"
                 << "pattern =" << patternId
                 << "raw =" << packet.toHex(' ');

    } else {

        qWarning() << "[UI->MAIN] TEST_PATTERN not sent:"
                   << "serial port is closed";
    }
}


/*========================================================================================*/
// ======================================================
// Main -> UI
// Decode intervention result packet
//
// TYPE = 0x54
//
// Implemented packet format:
// AA 55 54 SEQ
// plan0 plan1 plan2 plan3
// state
// board_id
// motor_count
// 12 34
//
// Payload:
// plan_id     : uint32 little-endian
// state       : uint8
// board_id    : uint8
// motor_count : uint8
//
// State mapping:
// 0 = IDLE
// 1 = EXECUTING
// 2 = COMPLETED
// 3 = FAILED
// 4 = REJECTED
// ======================================================
// ======================================================
// Main -> UI
// Try parse intervention result packet
//
// TYPE = 0x54
//
// Packet format:
// AA 55 54 SEQ
// plan0 plan1 plan2 plan3
// state
// board_id
// motor_count
// 12 34
//
// Total length = 13 bytes
//
// Returns:
// true  = packet parsed and removed from buffer
// false = packet invalid or incomplete
// ======================================================
bool SerialReceiver::tryParseInterventionResult(InterventionResult &out)
{
    static constexpr int INTERVENTION_RESULT_LEN = 13;

    // Wait until full packet is available
    if (m_buf.size() < INTERVENTION_RESULT_LEN)
        return false;

    // Validate header
    if ((quint8)m_buf[0] != SOF0 || (quint8)m_buf[1] != SOF1)
        return false;

    // Validate packet type
    if ((quint8)m_buf[2] != TYPE_INTERVENTION_RESULT)
        return false;

    // Validate dummy/static CRC footer
    if ((quint8)m_buf[11] != 0x12 || (quint8)m_buf[12] != 0x34)
        return false;

    // Fill output struct
    out.planId =
        quint32((quint8)m_buf[4]) |
        (quint32((quint8)m_buf[5]) << 8) |
        (quint32((quint8)m_buf[6]) << 16) |
        (quint32((quint8)m_buf[7]) << 24);

    // Lifecycle state:
    // 0 = IDLE
    // 1 = EXECUTING
    // 2 = COMPLETED
    // 3 = FAILED
    // 4 = REJECTED
    out.state = (quint8)m_buf[8];

    // Board/node involved in execution
    out.boardId = (quint8)m_buf[9];

    // Number of motors involved
    out.motorCount = (quint8)m_buf[10];

    // ======================================================
    // Validate intervention lifecycle state.
    //
    // Valid states:
    // 0 = IDLE
    // 1 = EXECUTING
    // 2 = COMPLETED
    // 3 = FAILED
    // 4 = REJECTED
    //
    // If state is outside this range, this is not a valid
    // INTERVENTION_RESULT packet or the stream is misaligned.
    // ======================================================
    if (out.state > 4) {
        qWarning() << "[INTERVENTION_RESULT] Invalid state:"
                   << out.state
                   << "raw =" << m_buf.left(INTERVENTION_RESULT_LEN).toHex(' ');

        return false;
    }




    // Remove parsed packet from serial buffer
    m_buf.remove(0, INTERVENTION_RESULT_LEN);

    qDebug() << "[MAIN->UI] INTERVENTION_RESULT"
             << "plan_id =" << out.planId
             << "state =" << out.state
             << "board =" << out.boardId
             << "motors =" << out.motorCount;

    return true;
}

/*========================================================================================*/
// Main -> UI
// Try parse intervention plan packet
//
// TYPE = 0x51
//
// Current minimal packet format:
// AA 55 51 SEQ
// planL planH
// board_id
// target_zone
// motor_count
// risk_score
// risk_level
// recommendation_code
// reason_code
// 12 34
//
// Total length = 15 bytes
//
// Returns:
// true  = packet parsed and removed from buffer
// false = packet invalid or incomplete
//
// Note:
// Motor vector items are not decoded in this first version.
// This function only parses the plan header needed for UI.
// ======================================================
bool SerialReceiver::tryParseInterventionPlan(InterventionPlan &out)
{
    static constexpr int INTERVENTION_PLAN_LEN = 15;

    // Wait until full packet is available
    if (m_buf.size() < INTERVENTION_PLAN_LEN)
        return false;

    // Validate header
    if ((quint8)m_buf[0] != SOF0 || (quint8)m_buf[1] != SOF1)
        return false;

    // Validate packet type
    if ((quint8)m_buf[2] != TYPE_INTERVENTION_PLAN)
        return false;

    // Validate dummy/static CRC footer
    if ((quint8)m_buf[13] != 0x12 || (quint8)m_buf[14] != 0x34)
        return false;

    // plan_id is uint16 little-endian
    out.planId =
        quint16((quint8)m_buf[4]) |
        (quint16((quint8)m_buf[5]) << 8);

    // Main board / target board id
    out.boardId = (quint8)m_buf[6];

    // Target body/bed zone
    out.targetZone = (quint8)m_buf[7];

    // Number of motors in this intervention
    out.motorCount = (quint8)m_buf[8];

    // Risk information generated by Main Board
    out.riskScore = (quint8)m_buf[9];
    out.riskLevel = (quint8)m_buf[10];

    // Recommendation and reason codes for UI explanation
    out.recommendationCode = (quint8)m_buf[11];
    out.reasonCode = (quint8)m_buf[12];

    // Remove parsed packet from serial buffer
    m_buf.remove(0, INTERVENTION_PLAN_LEN);

    qDebug() << "[MAIN->UI] INTERVENTION_PLAN"
             << "plan_id =" << out.planId
             << "board =" << out.boardId
             << "zone =" << out.targetZone
             << "motors =" << out.motorCount
             << "risk =" << out.riskScore
             << "level =" << out.riskLevel
             << "recommendation =" << out.recommendationCode
             << "reason =" << out.reasonCode;

    return true;
}

/*========================================================================================*/

/*========================================================================================*/

/*========================================================================================*/

/*========================================================================================*/

/*========================================================================================*/
