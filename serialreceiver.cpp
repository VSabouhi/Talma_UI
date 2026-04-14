#include "serialreceiver.h"
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

        if (m_buf.size() < PKT_LEN)
            return;


        const quint8 type = quint8(m_buf[2]);

        // اگر packet از نوع NODE32 باشد، مثل قبل parse می‌شود
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
        // اگر packet از نوع SUMMARY باشد، داخل SummaryData parse می‌شود
        else if (type == TYPE_SUMMARY) {
            static constexpr int SUMMARY_LEN = 24;

            if (m_buf.size() < SUMMARY_LEN)
                return;

            SummaryData summary;
            if (!tryParseSummary(summary)) {
                m_buf.remove(0, 1);
                emit parseError("Invalid SUMMARY packet, resyncing...");
                continue;
            }

            m_buf.remove(0, SUMMARY_LEN);
            emit summaryReceived(summary);
        }
        // اگر type ناشناخته بود، یک بایت جلو می‌رویم تا resync کنیم
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

bool SerialReceiver::tryParseSummary(SummaryData &out)
{
    static constexpr int SUMMARY_LEN = 24;

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

    out.riskScore = (quint8)m_buf[6];
    out.riskLevel = (quint8)m_buf[7];

    out.movementDetected = (quint8)m_buf[8];
    out.timeSinceLastMovementS = quint16((quint8)m_buf[9])
                                 | (quint16((quint8)m_buf[10]) << 8);

    out.alertActive = (quint8)m_buf[11];
    out.alertType = (quint8)m_buf[12];
    out.alertSeverity = (quint8)m_buf[13];
    out.alertDurationS = quint16((quint8)m_buf[14])
                         | (quint16((quint8)m_buf[15]) << 8);

    out.recommendationCode = (quint8)m_buf[16];
    out.recommendationPriority = (quint8)m_buf[17];

    out.sacrumAvg = (quint8)m_buf[18];
    out.sacrumPeak = (quint8)m_buf[19];
    out.heelLeftAvg = (quint8)m_buf[20];
    out.heelRightAvg = (quint8)m_buf[21];

    // فعلاً CRC روی برد placeholder است، پس اینجا validate واقعی نمی‌کنیم
    return true;
}
/*========================================================================================*/


/*========================================================================================*/


/*========================================================================================*/
