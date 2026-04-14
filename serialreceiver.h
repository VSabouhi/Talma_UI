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

class SerialReceiver : public QObject {
    Q_OBJECT
public:
    explicit SerialReceiver(QObject *parent = nullptr);

    void attach(QSerialPort *port);
    void detach();

signals:
    void packetReceived(const NodePacket &pkt);
    void parseError(const QString &msg);
    void summaryReceived(const SummaryData &summary); // برای ارسال داده parse‌شده‌ی packet 0x40 به UI

private slots:
    void onReadyRead();

private:
    void processBuffer();
    bool tryParseOne(NodePacket &out);
    bool tryParseSummary(SummaryData &out);  // برای parse کردن packet نوع 0x40

    QSerialPort *m_port = nullptr;
    QByteArray m_buf;

    static constexpr int PKT_LEN = 42;
    static constexpr quint8 SOF0 = 0xAA;
    static constexpr quint8 SOF1 = 0x55;
    static constexpr quint8 TYPE_NODE32 = 0x10;
    static constexpr quint8 TYPE_SUMMARY = 0x40;  // نوع packet برای Summary high-level metrics
};
