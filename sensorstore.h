#pragma once
#include <QObject>
#include <array>
#include "sensorstatus.h"

struct NodePacket;

class SensorStore : public QObject
{
    Q_OBJECT
public:
    static constexpr int NODES = 16;
    static constexpr int SENS  = 32;

    bool hasData(int nodeId) const;

    explicit SensorStore(QObject *parent = nullptr);

    quint8 raw(int node, int idx) const;
    quint8 value(int node, int idx) const;
    SensorStatus status(int node, int idx) const;
    bool valid(int node, int idx) const;
    quint8 confidence(int node, int idx) const;

    quint16 cycle(int nodeId) const;
    quint8 flags(int nodeId) const;
    NodeState nodeState(int nodeId) const;

public slots:
    void applyPacket(const NodePacket &pkt);
    void setNodeState(int nodeId, NodeState state);
    void setAllNodesState(NodeState state);

signals:
    void nodeUpdated(int nodeId);

private:
    quint8  m_raw[NODES][SENS] = {};
    quint16 m_cycle[NODES] = {};
    quint8  m_flags[NODES] = {};
    quint8  m_lastSeq[NODES] = {};
};
