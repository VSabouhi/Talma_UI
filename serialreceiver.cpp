#include "serialreceiver.h"

SerialReceiver::SerialReceiver(QObject *parent) : QObject(parent)
{
    m_buf.reserve(4096);
}

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

void SerialReceiver::detach()
{
    if (!m_port) return;

    disconnect(m_port, &QSerialPort::readyRead,
               this, &SerialReceiver::onReadyRead);

    m_port = nullptr;
    m_buf.clear();
}

void SerialReceiver::onReadyRead()
{
    if (!m_port) return;
    m_buf.append(m_port->readAll());
    processBuffer();
}

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

        NodePacket pkt;
        if (!tryParseOne(pkt)) {
            m_buf.remove(0, 1);
            emit parseError("Invalid packet, resyncing...");
            continue;
        }

        m_buf.remove(0, PKT_LEN);
        emit packetReceived(pkt);
    }
}

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
