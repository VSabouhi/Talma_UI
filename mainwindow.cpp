#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSerialPortInfo>
#include <QDebug>
#include <QSpacerItem>
#include <QLabel>
#include <QGridLayout>
#include <QFrame>
#include <QTableWidgetItem>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>



#include "sensordelegate.h"
#include "sensorstatus.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    /* =========================================================
     *  0) Initial UI State
     * ========================================================= */
    ui->tabs->setCurrentWidget(ui->tabHome);

    /* =========================================================
     *  1) Home Page (runtime-built, responsive)
     * ========================================================= */
    {
        QVBoxLayout *homeRootLayout = qobject_cast<QVBoxLayout*>(ui->tabHome->layout());
        if (!homeRootLayout) {
            homeRootLayout = new QVBoxLayout(ui->tabHome);
        }

        homeRootLayout->setContentsMargins(0, 0, 0, 0);
        homeRootLayout->setSpacing(0);

        // Vertical centering
        homeRootLayout->addStretch(1);

        QWidget *homeCenterRow = new QWidget(ui->tabHome);
        QHBoxLayout *centerRowLayout = new QHBoxLayout(homeCenterRow);
        centerRowLayout->setContentsMargins(0, 0, 0, 0);
        centerRowLayout->setSpacing(0);

        centerRowLayout->addStretch(1);

        // Main home content
        m_homeContentHost = new QWidget(homeCenterRow);
        m_homeContentHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        m_homeContentHost->setMinimumWidth(700);

        QVBoxLayout *homeContentLayout = new QVBoxLayout(m_homeContentHost);
        homeContentLayout->setContentsMargins(0, 0, 0, 0);
        homeContentLayout->setSpacing(16);

        // Title
        m_lblHomeTitle = new QLabel("Pressure Mapping & Monitoring", m_homeContentHost);
        m_lblHomeTitle->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

        QFont titleFont = m_lblHomeTitle->font();
        titleFont.setPointSize(20);
        titleFont.setBold(true);
        m_lblHomeTitle->setFont(titleFont);

        homeContentLayout->addWidget(m_lblHomeTitle, 0, Qt::AlignHCenter);

        // Cards row
        QWidget *homeCardsHost = new QWidget(m_homeContentHost);
        QHBoxLayout *cardsLayout = new QHBoxLayout(homeCardsHost);
        cardsLayout->setContentsMargins(0, 0, 0, 0);
        cardsLayout->setSpacing(16);

        auto makeCard = [](const QString &title, const QString &iconPath) -> QFrame*
        {
            QFrame *card = new QFrame();
            card->setMinimumSize(180, 150);
            card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            card->setFrameShape(QFrame::NoFrame);
            card->setCursor(Qt::PointingHandCursor);

            QVBoxLayout *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(18, 18, 18, 18);
            cardLayout->setSpacing(12);

            QLabel *iconLabel = new QLabel(card);
            iconLabel->setAlignment(Qt::AlignCenter);

            QPixmap pix(iconPath);
            iconLabel->setPixmap(pix.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));


            QLabel *titleLabel = new QLabel(title, card);
            titleLabel->setAlignment(Qt::AlignCenter);
            titleLabel->setWordWrap(true);

            QFont textFont = titleLabel->font();
            textFont.setPointSize(10);
            textFont.setBold(true);
            titleLabel->setStyleSheet("color: rgb(240,240,240);");

            cardLayout->addStretch(1);
            cardLayout->addWidget(iconLabel, 0, Qt::AlignCenter);
            cardLayout->addWidget(titleLabel, 0, Qt::AlignCenter);
            cardLayout->addStretch(1);

            card->setStyleSheet(
                "QFrame {"
                "  background-color: rgb(36, 36, 36);"
                "  border: 1px solid rgb(68, 68, 68);"
                "  border-radius: 12px;"
                "}"
                "QFrame:hover {"
                "  background-color: rgb(48, 48, 48);"
                "  border: 1px solid rgb(0, 170, 255);"
                "}"
                "QLabel {"
                "  color: rgb(235, 235, 235);"
                "  background: transparent;"
                "  border: none;"
                "}"
                );

            return card;
        };

        m_cardHeatmap  = makeCard("Heatmap Monitoring", "E:/Gallary/QtPractice/Qt Workspace/TalmaCanHeatMapProj9/icon/heatmap.png");
        m_cardData     = makeCard("Data Monitoring",    "E:/Gallary/QtPractice/Qt Workspace/TalmaCanHeatMapProj9/icon/data.png");
        m_cardSettings = makeCard("Settings",           "E:/Gallary/QtPractice/Qt Workspace/TalmaCanHeatMapProj9/icon/settings.png");

        m_cardHeatmap->installEventFilter(this);
        m_cardData->installEventFilter(this);
        m_cardSettings->installEventFilter(this);

        cardsLayout->addWidget(m_cardHeatmap);
        cardsLayout->addWidget(m_cardData);
        cardsLayout->addWidget(m_cardSettings);

        homeContentLayout->addWidget(homeCardsHost);

        centerRowLayout->addWidget(m_homeContentHost);
        centerRowLayout->addStretch(1);

        homeRootLayout->addWidget(homeCenterRow);
        homeRootLayout->addStretch(1);
    }

    /* =========================================================
 *  2) Settings Page (runtime-built, centered and stable)
 * ========================================================= */
    {
        QVBoxLayout *settingsRootLayout = qobject_cast<QVBoxLayout*>(ui->tabSettings->layout());
        if (!settingsRootLayout) {
            settingsRootLayout = new QVBoxLayout(ui->tabSettings);
        }

        settingsRootLayout->setContentsMargins(0, 0, 0, 0);
        settingsRootLayout->setSpacing(0);

        // بالا
        settingsRootLayout->addStretch(1);

        // پنل اصلی تنظیمات
        m_settingsContentHost = new QFrame(ui->tabSettings);
        m_settingsContentHost->setObjectName("settingsContentHost");
        m_settingsContentHost->setMinimumSize(700, 260);
        m_settingsContentHost->setMaximumWidth(900);
        m_settingsContentHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        m_settingsContentHost->setStyleSheet(
            "#settingsContentHost {"
            "  background-color: rgb(70, 70, 110);"
            "  border-radius: 8px;"
            "}"
            );

        QVBoxLayout *settingsContentLayout = new QVBoxLayout(m_settingsContentHost);
        settingsContentLayout->setContentsMargins(20, 20, 20, 20);
        settingsContentLayout->setSpacing(16);

        /* ---------- Serial Group ---------- */
        QGroupBox *grpSerial = new QGroupBox("Serial Connection", m_settingsContentHost);
        grpSerial->setMinimumHeight(90);
        grpSerial->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        QVBoxLayout *serialLayout = new QVBoxLayout(grpSerial);
        QHBoxLayout *serialRow = new QHBoxLayout();

        QLabel *lblPort = new QLabel("Port:", grpSerial);

        m_cmbPortSettings = new QComboBox(grpSerial);
        m_btnRefreshSettings = new QPushButton("Refresh", grpSerial);
        m_btnConnectSettings = new QPushButton("Connect", grpSerial);
        m_lblStatusSettings = new QLabel("Disconnected", grpSerial);

        serialRow->addWidget(lblPort);
        serialRow->addWidget(m_cmbPortSettings);
        serialRow->addWidget(m_btnRefreshSettings);
        serialRow->addWidget(m_btnConnectSettings);
        serialRow->addWidget(m_lblStatusSettings);

        serialLayout->addLayout(serialRow);

        /* ---------- Display Group ---------- */
        QGroupBox *grpDisplay = new QGroupBox("Display Settings", m_settingsContentHost);
        grpDisplay->setMinimumHeight(90);
        grpDisplay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        QVBoxLayout *displayLayout = new QVBoxLayout(grpDisplay);
        QHBoxLayout *rangeRow = new QHBoxLayout();

        QLabel *lblHigh = new QLabel("Red:", grpDisplay);
        m_spHighSettings = new QSpinBox(grpDisplay);
        m_spHighSettings->setRange(0, 63);
        m_spHighSettings->setValue(5);

        QLabel *lblNo = new QLabel("Blue:", grpDisplay);
        m_spNoSettings   = new QSpinBox(grpDisplay);
        m_spNoSettings->setRange(0, 63);
        m_spNoSettings->setValue(50);

        rangeRow->addWidget(lblHigh);
        rangeRow->addWidget(m_spHighSettings);
        rangeRow->addSpacing(10);
        rangeRow->addWidget(lblNo);
        rangeRow->addWidget(m_spNoSettings);
        rangeRow->addStretch(1);

        displayLayout->addLayout(rangeRow);

        settingsContentLayout->addWidget(grpSerial);
        settingsContentLayout->addWidget(grpDisplay);

        // وسط‌چین افقی
        settingsRootLayout->addWidget(m_settingsContentHost, 0, Qt::AlignHCenter);

        // پایین
        settingsRootLayout->addStretch(1);
    }

    /* =========================================================
     *  3) Node Summary Grid (Data tab - top section)
     * ========================================================= */
    m_nodeSummaryLayout = new QGridLayout(ui->nodeSummaryHost);
    m_nodeSummaryLayout->setContentsMargins(8, 8, 8, 8);
    m_nodeSummaryLayout->setHorizontalSpacing(8);
    m_nodeSummaryLayout->setVerticalSpacing(8);

    m_nodeCards.reserve(SensorStore::NODES);

    for (int n = 0; n < SensorStore::NODES; ++n)
    {
        QFrame *card = new QFrame(ui->nodeSummaryHost);
        card->setFrameShape(QFrame::StyledPanel);
        card->setMinimumHeight(56);
        card->setStyleSheet(
            "QFrame {"
            "  background-color: #1e1e1e;"
            "  border: 1px solid #3a3a3a;"
            "  border-radius: 6px;"
            "}"
            );

        card->setProperty("nodeId", n);
        card->setCursor(Qt::PointingHandCursor);
        card->installEventFilter(this);

        QVBoxLayout *cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(8, 6, 8, 6);
        cardLay->setSpacing(2);

        QLabel *title = new QLabel(QString("Node %1").arg(n), card);
        title->setStyleSheet("QLabel { color: #e8e8e8; font-weight: 600; }");

        QLabel *sub = new QLabel("OFFLINE  |  C0", card);
        sub->setStyleSheet("QLabel { color: #a8a8a8; font-size: 11px; }");

        m_nodeTitles.append(title);
        m_nodeSubs.append(sub);

        cardLay->addWidget(title);
        cardLay->addWidget(sub);

        const int row = n / 4;
        const int col = n % 4;
        m_nodeSummaryLayout->addWidget(card, row, col);

        m_nodeCards.append(card);
    }

    /* =========================================================
     *  4) Heatmap Host (runtime widget)
     * ========================================================= */
    /* =========================================================
 *  4) Heatmap Host (runtime widget)
 * ========================================================= */
    ui->tabHeatmapHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // layout موجود از .ui را بگیر
    QLayout *hostLayout = ui->tabHeatmapHost->layout();

    if (!hostLayout) {
        QVBoxLayout *fallbackLayout = new QVBoxLayout(ui->tabHeatmapHost);
        fallbackLayout->setContentsMargins(0, 0, 0, 0);
        fallbackLayout->setSpacing(0);
        hostLayout = fallbackLayout;
    } else {
        // اگر قبلاً چیزی داخلش هست، خالی‌اش کن
        while (QLayoutItem *item = hostLayout->takeAt(0)) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }

    // یک container جدید داخل tabHeatmapHost می‌گذاریم
    QWidget *heatmapPageContainer = new QWidget(ui->tabHeatmapHost);
    heatmapPageContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QHBoxLayout *mainLayout = new QHBoxLayout(heatmapPageContainer);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(12);

    // ================= LEFT PANEL =================
    QWidget *leftPanel = new QWidget(heatmapPageContainer);
    leftPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    // Title
    QLabel *title = new QLabel("Live Pressure Heatmap", leftPanel);
    title->setStyleSheet(
        "QLabel {"
        "  font-size: 22px;"
        "  font-weight: 700;"
        "  color: #F2F4F8;"
        "  padding-bottom: 4px;"
        "}"
        );
    leftLayout->addWidget(title, 0, Qt::AlignLeft);

    // Heatmap
    m_heatmap = new HeatmapWidget(leftPanel);
    m_heatmap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_heatmap->setMinimumSize(620, 620);
    m_heatmap->setStore(&m_store);
    leftLayout->addWidget(m_heatmap, 1);

    // Bottom info row
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(12);

    m_lblRiskLive = new QLabel("Risk: --", leftPanel);
    m_lblRiskLive->setMinimumHeight(44);
    m_lblRiskLive->setMinimumWidth(120);
    m_lblRiskLive->setStyleSheet(
        "QLabel {"
        "  background-color: #1B1F24;"
        "  border: 1px solid #2F3945;"
        "  border-radius: 10px;"
        "  padding: 10px 14px;"
        "  color: #F2F4F8;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "}"
        );


    m_lblMovementLive = new QLabel("Last Move: --", leftPanel);
    m_lblMovementLive->setMinimumHeight(44);
    m_lblMovementLive->setMinimumWidth(150);
    m_lblMovementLive->setStyleSheet(
        "QLabel {"
        "  background-color: #1B1F24;"
        "  border: 1px solid #2F3945;"
        "  border-radius: 10px;"
        "  padding: 10px 14px;"
        "  color: #F2F4F8;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "}"
        );

    bottomRow->addWidget(m_lblRiskLive);
    bottomRow->addWidget(m_lblMovementLive);
    bottomRow->addStretch(1);

    leftLayout->addLayout(bottomRow);

    // ================= RIGHT PANEL =================
    QWidget *rightPanel = new QWidget(heatmapPageContainer);
    rightPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    rightPanel->setMinimumWidth(260);
    rightPanel->setMaximumWidth(300);

    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    // Alerts
    QGroupBox *grpAlerts = new QGroupBox("Active Alerts", rightPanel);
    QVBoxLayout *alertsLay = new QVBoxLayout(grpAlerts);
    m_lblAlertsText = new QLabel("No alerts", grpAlerts);
    m_lblAlertsText->setStyleSheet("color:#D8DEE9; padding:4px 2px 6px 2px;");
    alertsLay->addWidget(m_lblAlertsText);
    rightLayout->addWidget(grpAlerts);

    // Zones
    QGroupBox *grpZones = new QGroupBox("Body Zones", rightPanel);
    QVBoxLayout *zonesLay = new QVBoxLayout(grpZones);
    m_lblSacrum = new QLabel("Sacrum: --", grpZones);
    m_lblHeels = new QLabel("Heels: --", grpZones);
    m_lblShoulders = new QLabel("Shoulders: --", grpZones);

    m_lblSacrum->setStyleSheet("color:#D8DEE9; padding:2px;");
    m_lblHeels->setStyleSheet("color:#D8DEE9; padding:2px;");
    m_lblShoulders->setStyleSheet("color:#D8DEE9; padding:2px;");

    zonesLay->addWidget(m_lblSacrum);
    zonesLay->addWidget(m_lblHeels);
    zonesLay->addWidget(m_lblShoulders);
    rightLayout->addWidget(grpZones);

    // Recommendation
    QGroupBox *grpRec = new QGroupBox("Recommendation", rightPanel);

    const QString sideGroupStyle =
        "QGroupBox {"
        "  border: 1px solid #2D3742;"
        "  border-radius: 10px;"
        "  margin-top: 10px;"
        "  padding-top: 12px;"
        "  background-color: #15191E;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 4px 0 4px;"
        "  color: #E7ECF3;"
        "}";

    grpAlerts->setStyleSheet(sideGroupStyle);
    grpZones->setStyleSheet(sideGroupStyle);
    grpRec->setStyleSheet(sideGroupStyle);

    QVBoxLayout *recLay = new QVBoxLayout(grpRec);
    m_lblRecommendation = new QLabel("No recommendation", grpRec);
    m_lblRecommendation->setWordWrap(true);
    m_lblRecommendation->setStyleSheet("color:#D8DEE9; padding:4px 2px 6px 2px;");
    recLay->addWidget(m_lblRecommendation);
    rightLayout->addWidget(grpRec);

    rightLayout->addStretch(1);

    // ===== Add to main layout =====
    mainLayout->addWidget(leftPanel, 4);
    mainLayout->addWidget(rightPanel, 1);

    // container را داخل layout موجود tabHeatmapHost قرار بده
    hostLayout->addWidget(heatmapPageContainer);



    connect(&m_store, &SensorStore::nodeUpdated,
            m_heatmap, &HeatmapWidget::onNodeUpdated);

    connect(&m_store, &SensorStore::nodeUpdated,
            this, &MainWindow::updateNodeSummary);

    /* =========================================================
     *  6) Data Table Setup
     * ========================================================= */
    ui->tblSensors->setRowCount(2);
    ui->tblSensors->setColumnCount(16);

    for (int c = 0; c < 16; ++c)
        ui->tblSensors->setHorizontalHeaderItem(c, new QTableWidgetItem(QString::number(c)));

    ui->tblSensors->setVerticalHeaderItem(0, new QTableWidgetItem("Top Row"));
    ui->tblSensors->setVerticalHeaderItem(1, new QTableWidgetItem("Bottom Row"));

    ui->tblSensors->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tblSensors->setSelectionMode(QAbstractItemView::NoSelection);

    ui->tblSensors->setItemDelegate(new SensorDelegate(ui->tblSensors));
    ui->tblSensors->setStyleSheet("QTableWidget::item { padding-top: 6px; }");

    connect(ui->tabs, &QTabWidget::currentChanged, this, [this](int){
        if (ui->tabs->currentWidget() != ui->Data)
            return;

        updateTableNode(m_selectedNode);
    });

    connect(&m_store, &SensorStore::nodeUpdated,
            this, &MainWindow::updateTableNode);

    /* =========================================================
     *  7) Serial / Port UI
     * ========================================================= */
    refreshPorts();
    setConnectedUi(false);

    connect(m_btnRefreshSettings, &QPushButton::clicked,
            this, &MainWindow::refreshPorts);

    connect(m_btnConnectSettings, &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);
    /* =========================================================
     *  8) Serial Receiver / Data Pipeline
     * ========================================================= */
    m_rx.attach(&m_port);

    connect(&m_port, &QSerialPort::errorOccurred,
            this, &MainWindow::onSerialError);

    connect(&m_rx, &SerialReceiver::packetReceived,
            this, &MainWindow::onPacket);

    connect(&m_rx, &SerialReceiver::packetReceived,
            &m_store, &SensorStore::applyPacket);

    /* =========================================================
     *  9) Initial Refresh
     * ========================================================= */
    updateNodeSummary(m_selectedNode);
    updateTableNode(m_selectedNode);
    refreshNodeCardStyles();


    connect(m_spHighSettings, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int){
                if (m_heatmap)
                    m_heatmap->setPressureRange(m_spHighSettings->value(),
                                                m_spNoSettings->value());
            });

    connect(m_spNoSettings, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int){
                if (m_heatmap)
                    m_heatmap->setPressureRange(m_spHighSettings->value(),
                                                m_spNoSettings->value());
            });


    connect(&m_store, &SensorStore::nodeUpdated,
            this, &MainWindow::updateLiveMonitoring);

}

/*========================================================= */
/*========================================================= */


MainWindow::~MainWindow()
{
    if (m_port.isOpen())
        m_port.close();
    delete ui;
}

void MainWindow::refreshPorts()
{
    if (!m_cmbPortSettings)
        return;

    m_cmbPortSettings->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports)
    {
        m_cmbPortSettings->addItem(port.portName());
    }
}

void MainWindow::setConnectedUi(bool connected)
{
    if (m_btnConnectSettings)
        m_btnConnectSettings->setText(connected ? "Disconnect" : "Connect");

    if (m_lblStatusSettings)
        m_lblStatusSettings->setText(connected ? "Connected" : "Disconnected");

    if (m_cmbPortSettings)
        m_cmbPortSettings->setEnabled(!connected);

    if (m_btnRefreshSettings)
        m_btnRefreshSettings->setEnabled(!connected);
}

void MainWindow::onConnectClicked()
{
    qDebug() << "onConnectClicked called";

    if (m_port.isOpen()) {
        m_port.close();
        setConnectedUi(false);
        return;
    }

    const QString portName = m_cmbPortSettings
                                 ? m_cmbPortSettings->currentText().trimmed()
                                 : QString();

    qDebug() << "Selected port =" << portName;

    if (portName.isEmpty()) {
        if (m_lblStatusSettings)
            m_lblStatusSettings->setText("No port selected");
        return;
    }

    m_port.setPortName(portName);
    m_port.setBaudRate(115200);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        if (m_lblStatusSettings)
            m_lblStatusSettings->setText("Open failed: " + m_port.errorString());
        setConnectedUi(false);
        return;
    }

    setConnectedUi(true);

    if (m_lblStatusSettings)
        m_lblStatusSettings->setText("Connected: " + portName);
}

void MainWindow::onSerialError(QSerialPort::SerialPortError e)
{
    if (e == QSerialPort::NoError)
        return;

    if (m_lblStatusSettings)
        m_lblStatusSettings->setText("Serial error: " + m_port.errorString());

    if (m_port.isOpen()) {
        m_port.close();
        setConnectedUi(false);
    }

}

void MainWindow::onPacket(const NodePacket &pkt)
{
    const NodeState ns = nodeStateFromFlags(pkt.flags);

    m_lblStatusSettings->setText(
        QString("RX node=%1 cycle=%2 seq=%3 state=%4")
            .arg(pkt.nodeId)
            .arg(pkt.cycle)
            .arg(pkt.seq)
            .arg(nodeStateText(ns))
        );
}

void MainWindow::updateTableNode(int nodeId)
{
    if (ui->tabs->currentWidget() != ui->Data)
        return;

    if (nodeId != m_selectedNode)
        return;

    ui->tblSensors->setUpdatesEnabled(false);
    ui->tblSensors->clearContents();
    ui->tblSensors->setRowCount(2);
    ui->tblSensors->setColumnCount(16);

    ui->tblSensors->setVerticalHeaderItem(0, new QTableWidgetItem("Top Row"));
    ui->tblSensors->setVerticalHeaderItem(1, new QTableWidgetItem("Bottom Row"));

    for (int c = 0; c < 16; ++c) {
        if (!ui->tblSensors->horizontalHeaderItem(c))
            ui->tblSensors->setHorizontalHeaderItem(c, new QTableWidgetItem(QString::number(c)));
    }

    const NodeState nState = m_store.nodeState(nodeId);
    const quint16 cyc = m_store.cycle(nodeId);

    for (int rr = 0; rr < 2; ++rr)
    {
        const int row = rr;

        for (int c = 0; c < 16; ++c)
        {
            const int idx = rr * 16 + c;
            const quint8 raw = m_store.raw(nodeId, idx);
            const quint8 value = m_store.value(nodeId, idx);
            const SensorStatus sst = m_store.status(nodeId, idx);
            const bool valid = m_store.valid(nodeId, idx);
            const quint8 conf = m_store.confidence(nodeId, idx);

            QTableWidgetItem *it = ui->tblSensors->item(row, c);
            if (!it) {
                it = new QTableWidgetItem();
                it->setTextAlignment(Qt::AlignCenter);
                ui->tblSensors->setItem(row, c, it);
            }

            it->setData(Qt::UserRole, int(raw));
            it->setData(Qt::UserRole + 1, int(nState));
            it->setData(Qt::UserRole + 2, uint(cyc));

            if (valid)
            {
                it->setData(Qt::DisplayRole, QString::number(int(value)));
            }
            else
            {
                if (sst == SensorStatus::Disconnected)
                    it->setData(Qt::DisplayRole, "--");
                else if (sst == SensorStatus::Error)
                    it->setData(Qt::DisplayRole, "ERR");
                else
                    it->setData(Qt::DisplayRole, "");
            }

            const int bedRow = 2 * nodeId + (idx / 16);
            const int bedCol = idx % 16;

            it->setToolTip(
                QString("Node: %1\n"
                        "Sensor: %2\n"
                        "Bed Row/Col: %3 / %4\n"
                        "Raw: 0x%5\n"
                        "Value: %6\n"
                        "Status: %7\n"
                        "Valid: %8\n"
                        "Confidence: %9\n"
                        "Node State: %10\n"
                        "Node Cycle: %11")
                    .arg(nodeId)
                    .arg(idx)
                    .arg(bedRow)
                    .arg(bedCol)
                    .arg(QString::number(raw, 16).rightJustified(2, '0').toUpper())
                    .arg(value)
                    .arg(sensorStatusText(sst))
                    .arg(valid ? "true" : "false")
                    .arg(conf)
                    .arg(nodeStateText(nState))
                    .arg(cyc)
                );
        }
    }

    ui->tblSensors->setUpdatesEnabled(true);
    ui->tblSensors->viewport()->update();
}

void MainWindow::markAllNodesState(NodeState state)
{
    m_store.setAllNodesState(state);
}


void MainWindow::updateNodeSummary(int nodeId)
{
    if (nodeId < 0 || nodeId >= SensorStore::NODES)
        return;

    NodeState st = m_store.nodeState(nodeId);
    quint16 cyc = m_store.cycle(nodeId);

    int validCount = 0;
    int invalidCount = 0;

    for (int i = 0; i < 32; ++i) {
        quint8 raw = m_store.raw(nodeId, i);
        if (isSensorValid(raw))
            validCount++;
        else
            invalidCount++;
    }

    // text
    QString stateText;
    QColor color;

    switch (st)
    {
    case NodeState::Online:
        stateText = "ONLINE";
        color = QColor(60, 180, 75);
        break;

    case NodeState::Stale:
        stateText = "STALE";
        color = QColor(255, 200, 0);
        break;

    case NodeState::Offline:
    default:
        stateText = "OFFLINE";
        color = QColor(120, 120, 120);
        break;
    }

    QString stateColor;

    switch (st)
    {
    case NodeState::Online:
        stateColor = "#3CB44B"; // سبز
        break;

    case NodeState::Stale:
        stateColor = "#FFC800"; // زرد
        break;

    case NodeState::Offline:
    default:
        stateColor = "#AAAAAA"; // خاکستری
        break;
    }


    QString summary = QString(
                          "<span style='color:%1; font-weight:600;'>%2</span><br>"
                          "Cycle: %3<br>"
                          "Valid: %4&nbsp;&nbsp;Invalid: %5"
                          )
                          .arg(stateColor)
                          .arg(stateText)
                          .arg(cyc)
                          .arg(validCount)
                          .arg(invalidCount);

    m_nodeSubs[nodeId]->setTextFormat(Qt::RichText);

    m_nodeSubs[nodeId]->setText(summary);

    QString tooltip = QString(
                          "<b>Node %1</b><br>"
                          "State: %2<br>"
                          "Cycle: %3<br>"
                          "Valid: %4<br>"
                          "Invalid: %5"
                          )
                          .arg(nodeId)
                          .arg(stateText)
                          .arg(cyc)
                          .arg(validCount)
                          .arg(invalidCount);

    m_nodeCards[nodeId]->setToolTip(tooltip);



    QFont f = m_nodeSubs[nodeId]->font();
    f.setPointSize(9);
    f.setBold(false);
    m_nodeSubs[nodeId]->setFont(f);

    refreshNodeCardStyles();

}


void MainWindow::refreshNodeCardStyles()
{
    for (int nodeId = 0; nodeId < SensorStore::NODES; ++nodeId)
    {
        NodeState st = m_store.nodeState(nodeId);

        QColor borderColor;
        QColor bgColor = QColor(30, 30, 30);

        if (st == NodeState::Online)
            bgColor = QColor(30, 45, 30);   // سبز خیلی ملایم
        else if (st == NodeState::Stale)
            bgColor = QColor(45, 40, 25);   // زرد خیلی ملایم

        switch (st)
        {
        case NodeState::Online:
            borderColor = QColor(60, 180, 75);
            break;

        case NodeState::Stale:
            borderColor = QColor(255, 200, 0);
            break;

        case NodeState::Offline:
        default:
            borderColor = QColor(80, 80, 80);
            break;
        }

        int borderWidth = 1;

        // اگر این نود انتخاب شده باشد، ظاهرش برجسته‌تر شود
        if (nodeId == m_selectedNode) {
            borderWidth = 2;
            bgColor = QColor(38, 38, 38);
        }

        QString style = QString(
                            "QFrame {"
                            " background-color: %1;"
                            " border: %2px solid #444444;"
                            " border-radius: 8px;"
                            "}"
                            )
                            .arg(bgColor.name())
                            .arg(borderWidth);

        m_nodeCards[nodeId]->setStyleSheet(style);

        m_nodeCards[nodeId]->setStyleSheet(style);

        // title هم اگر انتخاب شده بود کمی پررنگ‌تر
        if (nodeId < m_nodeTitles.size()) {
            QString titleColor = "#E8E8E8";

            switch (st)
            {
            case NodeState::Online:
                titleColor = "#3CB44B";
                break;

            case NodeState::Stale:
                titleColor = "#FFC800";
                break;

            case NodeState::Offline:
            default:
                titleColor = "#E8E8E8";
                break;
            }

           /* if (nodeId == m_selectedNode) {
                borderWidth = 2;
                bgColor = QColor(38, 38, 38);
            }*/

            int titleWeight = (nodeId == m_selectedNode) ? 700 : 600;

            m_nodeTitles[nodeId]->setStyleSheet(
                QString("QLabel { color: %1; font-weight: %2; }")
                    .arg(titleColor)
                    .arg(titleWeight)
                );
        }
    }
}


bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        if (obj == m_cardHeatmap)
        {
            ui->tabs->setCurrentWidget(ui->Heatmap);
            return true;
        }
        else if (obj == m_cardData)
        {
            ui->tabs->setCurrentWidget(ui->Data);
            return true;
        }
        else if (obj == m_cardSettings)
        {
            ui->tabs->setCurrentWidget(ui->tabSettings);
            return true;
        }

        // Node cards
        for (int i = 0; i < m_nodeCards.size(); ++i)
        {
            if (obj == m_nodeCards[i])
            {
                m_selectedNode = i;
                updateNodeSummary(i);
                updateTableNode(i);
                refreshNodeCardStyles();
                return true;
            }
        }
    }
        return QMainWindow::eventFilter(obj, event);
}


void MainWindow::updateLiveMonitoring()
{
    // فعلاً فقط تستی
    m_lblRiskLive->setText("Risk: 0");
    m_lblMovementLive->setText("Last Move: 0s");

    m_lblAlertsText->setText("No alerts");

    m_lblSacrum->setText("Sacrum: --");
    m_lblHeels->setText("Heels: --");
    m_lblShoulders->setText("Shoulders: --");

    m_lblRecommendation->setText("Monitoring...");
}
