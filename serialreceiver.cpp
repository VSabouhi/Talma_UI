#include "serialreceiver.h"
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
    if (!m_port) return;
    m_buf.append(m_port->readAll());
    processBuffer();
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

            BedSnapshotPacket pkt;
            if (!tryParseBedSnapshot(pkt)) {
                m_buf.remove(0, 1);
                emit parseError("Invalid BED_SNAPSHOT packet, resyncing...");
                continue;
            }

            m_buf.remove(0, BED_PKT_LEN);
            emit bedSnapshotReceived(pkt);
        }
        // ---------------- BED STATUS (0x22) ----------------
        else if (type == TYPE_BED_STATUS) {
            static constexpr int BED_PKT_LEN = 522;

            if (m_buf.size() < BED_PKT_LEN)
                return;

            BedStatusPacket pkt;
            if (!tryParseBedStatus(pkt)) {
                m_buf.remove(0, 1);
                emit parseError("Invalid BED_STATUS packet, resyncing...");
                continue;
            }

            m_buf.remove(0, BED_PKT_LEN);
            emit bedStatusReceived(pkt);
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

            m_buf.remove(0, SUMMARY_LEN);
           /* qDebug() << "SUMMARY parsed:"
                     << "frameId =" << summary.frameId
                     << "risk =" << summary.riskScore
                     << "movement =" << summary.timeSinceLastMovementS;*/
            emit summaryReceived(summary);
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

    // CRC فعلاً placeholder است، validate واقعی نداریم
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

    // CRC فعلاً placeholder است، validate واقعی نداریم
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


/*========================================================================================*/


/*========================================================================================*/
