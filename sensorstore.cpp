#include "sensorstore.h"
#include "serialreceiver.h"
#include <QDebug>

SensorStore::SensorStore(QObject *parent)
    : QObject(parent)
{
    for (int n = 0; n < NODES; ++n) {
        m_cycle[n] = 0;
        m_flags[n] = quint8(NodeState::Offline);
        m_lastSeq[n] = 0;

        for (int i = 0; i < SENS; ++i)
            m_raw[n][i] = makeRaw(0, SensorStatus::Disconnected);
    }
}

quint8 SensorStore::raw(int node, int idx) const
{
    return m_raw[node][idx];
}

quint8 SensorStore::value(int node, int idx) const
{
    return sensorValueFromRaw(m_raw[node][idx]);
}

SensorStatus SensorStore::status(int node, int idx) const
{
    return sensorStatusFromRaw(m_raw[node][idx]);
}

bool SensorStore::valid(int node, int idx) const
{
    return isSensorValid(m_raw[node][idx]);
}

quint8 SensorStore::confidence(int node, int idx) const
{
    return sensorConfidence(m_raw[node][idx]);
}

quint16 SensorStore::cycle(int nodeId) const
{
    return m_cycle[nodeId];
}

quint8 SensorStore::flags(int nodeId) const
{
    return m_flags[nodeId];
}

NodeState SensorStore::nodeState(int nodeId) const
{
    return nodeStateFromFlags(m_flags[nodeId]);
}

void SensorStore::applyPacket(const NodePacket &pkt)
{
    if (pkt.nodeId >= NODES)
        return;

    bool changed = false;

    if (m_cycle[pkt.nodeId] != pkt.cycle) {
        m_cycle[pkt.nodeId] = pkt.cycle;
        changed = true;
    }

    if (m_flags[pkt.nodeId] != pkt.flags) {
        m_flags[pkt.nodeId] = pkt.flags;
        changed = true;
    }

    if (m_lastSeq[pkt.nodeId] != pkt.seq) {
        m_lastSeq[pkt.nodeId] = pkt.seq;
        changed = true;
    }

    for (int i = 0; i < SENS; ++i) {


        if (m_raw[pkt.nodeId][i] != pkt.sensors[i]) {
            m_raw[pkt.nodeId][i] = pkt.sensors[i];
            changed = true;
        }
    }

    if (changed)
        emit nodeUpdated(pkt.nodeId);

}

void SensorStore::setNodeState(int nodeId, NodeState state)
{
    if (nodeId < 0 || nodeId >= NODES)
        return;

    const quint8 oldStateBits = (m_flags[nodeId] & 0x03);
    const quint8 newStateBits = quint8(state) & 0x03;

    if (oldStateBits == newStateBits)
        return;

    m_flags[nodeId] = (m_flags[nodeId] & ~quint8(0x03)) | newStateBits;
    emit nodeUpdated(nodeId);
}

void SensorStore::setAllNodesState(NodeState state)
{
    for (int n = 0; n < NODES; ++n)
        setNodeState(n, state);
}

bool SensorStore::hasData(int nodeId) const
{
    if (nodeId < 0 || nodeId >= NODES)
        return false;

    return m_cycle[nodeId] != 0;
}
