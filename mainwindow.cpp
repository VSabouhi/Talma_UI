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
#include <QDateTime>   // برای ثبت زمان آخرین دریافت Summary


#include "sensordelegate.h"
#include "sensorstatus.h"
#include "summarydata.h"
#include <QMetaType>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // NEW: ساخت تب مستقل Debug و اضافه کردن به TabWidget
    m_tabDebug = new QWidget();
    m_tabDebug->setObjectName("tabDebug");
    ui->tabs->addTab(m_tabDebug, "Debug");

    qRegisterMetaType<SummaryData>("SummaryData");  // ثبت type برای signal/slot


    // این تایمر باعث می‌شود داشبورد سمت راست با نرخ آرام‌تر refresh شود
    // تا متن‌ها پایدار و قابل خواندن باشند.
    m_summaryUiTimer = new QTimer(this);
    m_summaryUiTimer->setInterval(250);  // هر 250 میلی‌ثانیه یک بار UI را آپدیت می‌کنیم
    connect(m_summaryUiTimer, &QTimer::timeout,
            this, &MainWindow::renderSummaryToDashboard);
    m_summaryUiTimer->start();
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

        m_cardHeatmap  = makeCard("Heatmap Monitoring", "D:/gKteso/TALMA/Software/Qt/Talma_UI/icon/heatmap.png");
        m_cardData     = makeCard("Data Monitoring",    "D:/gKteso/TALMA/Software/Qt/Talma_UI/icon/data.png");
        m_cardSettings = makeCard("Settings",           "D:/gKteso/TALMA/Software/Qt/Talma_UI/icon/settings.png");

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
     *  2.5) Debug Page (runtime-built, separate from Settings)
     * ========================================================= */
    {
        QVBoxLayout *debugRootLayout = qobject_cast<QVBoxLayout*>(m_tabDebug->layout());
        if (!debugRootLayout) {
            debugRootLayout = new QVBoxLayout(m_tabDebug);
        }

        debugRootLayout->setContentsMargins(0, 0, 0, 0);
        debugRootLayout->setSpacing(0);

        debugRootLayout->addStretch(1);

        m_debugContentHost = new QFrame(m_tabDebug);
        m_debugContentHost->setObjectName("debugContentHost");
        m_debugContentHost->setMinimumSize(700, 260);
        m_debugContentHost->setMaximumWidth(900);
        m_debugContentHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        m_debugContentHost->setStyleSheet(
            "#debugContentHost {"
            "  background-color: rgb(60, 60, 95);"
            "  border-radius: 8px;"
            "}"
            );

        QVBoxLayout *debugContentLayout = new QVBoxLayout(m_debugContentHost);
        debugContentLayout->setContentsMargins(20, 20, 20, 20);
        debugContentLayout->setSpacing(16);

        // عنوان
        m_lblDebugTitle = new QLabel("Technical Debug Tools", m_debugContentHost);
        m_lblDebugTitle->setStyleSheet(
            "QLabel {"
            "  font-size: 18px;"
            "  font-weight: 700;"
            "  color: #F2F4F8;"
            "}"
            );
        debugContentLayout->addWidget(m_lblDebugTitle, 0, Qt::AlignLeft);

        // گروه Visual Debug
        QGroupBox *grpVisualDebug = new QGroupBox("Visual Debug", m_debugContentHost);
        grpVisualDebug->setStyleSheet(
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
            "}"
            );

        QVBoxLayout *visualDebugLayout = new QVBoxLayout(grpVisualDebug);
        visualDebugLayout->setContentsMargins(14, 12, 14, 12);
        visualDebugLayout->setSpacing(10);

        m_chkShowDebugText = new QCheckBox("Show cell debug text", grpVisualDebug);
        m_chkShowTooltip   = new QCheckBox("Show hover tooltip", grpVisualDebug);
        m_chkShowBodyBounds = new QCheckBox("Show body bounds (debug)", grpVisualDebug);
        m_chkShowBodyZones  = new QCheckBox("Show body zones", grpVisualDebug);
        m_chkShowZoneValues = new QCheckBox("Show zone values (debug)", grpVisualDebug);
        // ===== Dashboard Zone Stats Mode =====
        m_grpZoneStatsMode = new QGroupBox("Dashboard Zone Stats Source", m_debugContentHost);
        m_grpZoneStatsMode->setStyleSheet(
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
            "}"
            );

        QVBoxLayout *zoneModeLayout = new QVBoxLayout(m_grpZoneStatsMode);
        zoneModeLayout->setContentsMargins(14, 12, 14, 12);
        zoneModeLayout->setSpacing(8);

        m_radZoneStatsDevice = new QRadioButton("Device stats", m_grpZoneStatsMode);
        m_radZoneStatsAdaptive = new QRadioButton("Adaptive stats", m_grpZoneStatsMode);
        m_radZoneStatsCompare = new QRadioButton("Compare device vs adaptive", m_grpZoneStatsMode);

        m_radZoneStatsDevice->setStyleSheet("QRadioButton { color: #D8DEE9; }");
        m_radZoneStatsAdaptive->setStyleSheet("QRadioButton { color: #D8DEE9; }");
        m_radZoneStatsCompare->setStyleSheet("QRadioButton { color: #D8DEE9; }");

        // حالت پیش‌فرض
        m_radZoneStatsDevice->setChecked(false);
        m_radZoneStatsAdaptive->setChecked(true);
        m_radZoneStatsCompare->setChecked(false);

        zoneModeLayout->addWidget(m_radZoneStatsDevice);
        zoneModeLayout->addWidget(m_radZoneStatsAdaptive);
        zoneModeLayout->addWidget(m_radZoneStatsCompare);


        m_chkShowDebugText->setChecked(true);
        m_chkShowTooltip->setChecked(true);

        m_chkShowDebugText->setStyleSheet("QCheckBox { color: #D8DEE9; }");
        m_chkShowTooltip->setStyleSheet("QCheckBox { color: #D8DEE9; }");

        m_chkShowBodyBounds->setChecked(false);   // ← اینجا
        m_chkShowBodyZones->setChecked(false);     // ←
        m_chkShowZoneValues->setChecked(false);


        visualDebugLayout->addWidget(m_chkShowDebugText);
        visualDebugLayout->addWidget(m_chkShowTooltip);
        visualDebugLayout->addWidget(m_chkShowBodyBounds);
        visualDebugLayout->addWidget(m_chkShowBodyZones);
        visualDebugLayout->addWidget(m_chkShowZoneValues);


        debugContentLayout->addWidget(grpVisualDebug);
        debugContentLayout->addWidget(m_grpZoneStatsMode);
        // ===== Debug Data Panel =====
        m_grpDebugData = new QGroupBox("Data Debug", m_debugContentHost);
        m_grpDebugData->setStyleSheet(
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
            "}"
            );

        QVBoxLayout *debugDataLayout = new QVBoxLayout(m_grpDebugData);
        debugDataLayout->setContentsMargins(14, 12, 14, 12);
        debugDataLayout->setSpacing(8);

        auto makeDebugValueLabel = [this]() -> QLabel* {
            QLabel *lbl = new QLabel(m_grpDebugData);
            lbl->setStyleSheet(
                "QLabel {"
                "  color: #D8DEE9;"
                "  background-color: #10151B;"
                "  border: 1px solid #2D3742;"
                "  border-radius: 6px;"
                "  padding: 8px 10px;"
                "  font-size: 12px;"
                "}"
                );
            lbl->setWordWrap(true);
            return lbl;
        };

        m_lblDbgFrame = makeDebugValueLabel();
        m_lblDbgSync = makeDebugValueLabel();
        m_lblDbgSource = makeDebugValueLabel();
        m_lblDbgBody = makeDebugValueLabel();
        m_lblDbgSacrum = makeDebugValueLabel();
        m_lblDbgHeelLeft = makeDebugValueLabel();
        m_lblDbgHeelRight = makeDebugValueLabel();

        m_lblDbgBody->installEventFilter(this);
        m_lblDbgSacrum->installEventFilter(this);
        m_lblDbgHeelLeft->installEventFilter(this);
        m_lblDbgHeelRight->installEventFilter(this);

        m_lblDbgFrame->setText("Frame ID: --");
        m_lblDbgSync->setText("Sync: --");
        m_lblDbgSource->setText("Source: --");
        m_lblDbgBody->setText("Body: --");
        m_lblDbgSacrum->setText("Sacrum: --");
        m_lblDbgHeelLeft->setText("Left Heel: --");
        m_lblDbgHeelRight->setText("Right Heel: --");

        debugDataLayout->addWidget(m_lblDbgFrame);
        debugDataLayout->addWidget(m_lblDbgSync);
        debugDataLayout->addWidget(m_lblDbgSource);
        debugDataLayout->addWidget(m_lblDbgBody);
        debugDataLayout->addWidget(m_lblDbgSacrum);
        debugDataLayout->addWidget(m_lblDbgHeelLeft);
        debugDataLayout->addWidget(m_lblDbgHeelRight);

        debugContentLayout->addWidget(m_grpDebugData);
        debugContentLayout->addStretch(1);

        debugRootLayout->addWidget(m_debugContentHost, 0, Qt::AlignHCenter);
        debugRootLayout->addStretch(1);
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


    // Frame sync status
    m_lblFrameSync = new QLabel("Frame: waiting...", leftPanel);
    m_lblFrameSync->setMinimumHeight(22);
    m_lblFrameSync->setStyleSheet(
        "QLabel {"
        "  color: #AAB4BE;"
        "  background-color: #14181D;"
        "  border: 1px solid #2E3440;"
        "  border-radius: 6px;"
        "  padding: 3px 8px;"
        "  font-size: 11px;"
        "  font-weight: 500;"
        "}"
        );
    leftLayout->addWidget(m_lblFrameSync, 0, Qt::AlignLeft);


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
    m_heatmap->setBedStore(&m_bedStore);   // Heatmap را به storage جدید BED وصل می‌کنیم
    m_heatmap->setShowDebugText(true);   // NEW: sync اولیه با تب Debug
    m_heatmap->setShowTooltip(true);     // NEW: sync اولیه با تب Debug
    m_heatmap->setShowBodyZones(false);
    m_heatmap->setShowZoneValues(false);
    m_useAdaptiveZoneStats = true;   // NEW: حالت اولیه = adaptive
    m_compareZoneStats = false;
    leftLayout->addWidget(m_heatmap, 1);

    // Bottom info row
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(12);

    m_lblRiskLive = new QLabel("Risk: --", leftPanel);
    m_lblRiskLive->setMinimumHeight(64);   // بزرگ‌تر برای دیده شدن بهتر
    m_lblRiskLive->setMinimumWidth(220);   // عرض بیشتر تا شبیه کارت شود
    m_lblRiskLive->setAlignment(Qt::AlignCenter);
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
    m_lblMovementLive->setMinimumHeight(64);   // هماهنگ با کارت Risk
    m_lblMovementLive->setMinimumWidth(220);
    m_lblMovementLive->setAlignment(Qt::AlignCenter);
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

    // کارت Sacrum
    m_lblSacrum = new QLabel("SACRUM\n--", grpZones);

    // کارت Heel Left
    m_lblHeelLeft = new QLabel("LEFT HEEL\n--", grpZones);

    // کارت Heel Right
    m_lblHeelRight = new QLabel("RIGHT HEEL\n--", grpZones);

    // اگر فعلاً shoulders را نگه می‌داری، همین‌جا بماند
    //m_lblShoulders = new QLabel("Shoulders: --", grpZones);

    // استایل پایه برای کارت‌های zone
    const QString zoneCardStyle =
        "QLabel {"
        "color: #D8DEE9;"
        "background-color: #1B1F24;"
        "border: 1px solid #3B4252;"
        "padding: 10px;"
        "border-radius: 10px;"
        "font-weight: 600;"
        "}";

    m_lblSacrum->setStyleSheet(zoneCardStyle);
    m_lblHeelLeft->setStyleSheet(zoneCardStyle);
    m_lblHeelRight->setStyleSheet(zoneCardStyle);

    // اضافه شدن به layout زون‌ها
    zonesLay->addWidget(m_lblSacrum);
    zonesLay->addWidget(m_lblHeelLeft);
    zonesLay->addWidget(m_lblHeelRight);

    // اگر فعلاً shoulders لازم نیست، این خط را کامنت کن
    // zonesLay->addWidget(m_lblShoulders);

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

    connect(&m_rx, &SerialReceiver::summaryReceived,
            this, &MainWindow::onSummaryReceived);  // وصل کردن SUMMARY به UI

    connect(&m_rx, &SerialReceiver::bedSnapshotReceived,
            this, &MainWindow::onBedSnapshot);   // اتصال packet 0x20 به MainWindow

    connect(&m_rx, &SerialReceiver::bedStatusReceived,
            this, &MainWindow::onBedStatus);     // اتصال packet 0x22 به MainWindow

    connect(&m_rx, &SerialReceiver::nodeHealthReceived,
            this, &MainWindow::onNodeHealth);   // اتصال packet 0x30 به MainWindow


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

    connect(m_chkShowDebugText, &QCheckBox::toggled,
            this, [this](bool checked){
                if (m_heatmap)
                    m_heatmap->setShowDebugText(checked);
            });

    connect(m_chkShowTooltip, &QCheckBox::toggled,
            this, [this](bool checked){
                if (m_heatmap)
                    m_heatmap->setShowTooltip(checked);
            });


    connect(&m_store, &SensorStore::nodeUpdated,
            this, &MainWindow::updateLiveMonitoring);

    connect(&m_bedStore, &BedFrameStore::frameUpdated,
            m_heatmap, &HeatmapWidget::onBedFrameUpdated);   // با هر frame جدید، Heatmap دوباره رسم شود


    connect(m_chkShowBodyBounds, &QCheckBox::toggled,
            this, [this](bool checked){
                if (m_heatmap)
                    m_heatmap->setShowBodyBounds(checked);
            });

    connect(m_chkShowBodyZones, &QCheckBox::toggled,
            this, [this](bool checked){
                if (m_heatmap)
                    m_heatmap->setShowBodyZones(checked);
            });
    connect(m_chkShowZoneValues, &QCheckBox::toggled,
            this, [this](bool checked){
                if (m_heatmap)
                    m_heatmap->setShowZoneValues(checked);
            });
    connect(m_radZoneStatsDevice, &QRadioButton::toggled,
            this, [this](bool checked){
                if (!checked)
                    return;

                m_useAdaptiveZoneStats = false;
                m_compareZoneStats = false;
                renderSummaryToDashboard();
            });

    connect(m_radZoneStatsAdaptive, &QRadioButton::toggled,
            this, [this](bool checked){
                if (!checked)
                    return;

                m_useAdaptiveZoneStats = true;
                m_compareZoneStats = false;
                renderSummaryToDashboard();
            });

    connect(m_radZoneStatsCompare, &QRadioButton::toggled,
            this, [this](bool checked){
                if (!checked)
                    return;

                m_useAdaptiveZoneStats = false;
                m_compareZoneStats = true;
                renderSummaryToDashboard();
            });
}

/*========================================================= */
/*========================================================= */


MainWindow::~MainWindow()
{
    if (m_port.isOpen())
        m_port.close();
    delete ui;
}
/*========================================================================================*/

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
/*========================================================================================*/

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
/*========================================================================================*/

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
/*========================================================================================*/

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
/*========================================================================================*/

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
/*========================================================================================*/

void MainWindow::onBedSnapshot(const BedSnapshotPacket &pkt)
{
    // داده‌ی خام 32x16 تخت را داخل BedFrameStore ذخیره می‌کنیم
    m_bedStore.setSnapshot(pkt.frameId, pkt.values.data(), int(pkt.values.size()));

    // Frame sync state
    m_frameSync.snapshotFrameId = pkt.frameId;
    m_frameSync.hasSnapshot = true;

    // qDebug() << "BED_SNAPSHOT received, frameId =" << pkt.frameId;
}
/*========================================================================================*/

void MainWindow::onBedStatus(const BedStatusPacket &pkt)
{
    // status 32x16 تخت را داخل BedFrameStore ذخیره می‌کنیم
    m_bedStore.setStatus(pkt.frameId, pkt.status.data(), int(pkt.status.size()));

    // Frame sync state
    m_frameSync.statusFrameId = pkt.frameId;
    m_frameSync.hasStatus = true;

    // qDebug() << "BED_STATUS received, frameId =" << pkt.frameId;
}
/*========================================================================================*/

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
/*========================================================================================*/

void MainWindow::markAllNodesState(NodeState state)
{
    m_store.setAllNodesState(state);
}

/*========================================================================================*/

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

/*========================================================================================*/

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
/*========================================================================================*/


bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{


    if (event->type() == QEvent::MouseButtonPress)
    {

        // ===== Click on Debug Data labels => highlight zone on heatmap =====
        if (m_heatmap) {
            if (obj == m_lblDbgSacrum) {
                m_heatmap->setHighlightedZoneRect(m_dbgSacrumRect);
                return true;
            }
            else if (obj == m_lblDbgHeelLeft) {
                m_heatmap->setHighlightedZoneRect(m_dbgLeftHeelRect);
                return true;
            }
            else if (obj == m_lblDbgHeelRight) {
                m_heatmap->setHighlightedZoneRect(m_dbgRightHeelRect);
                return true;
            }
            else if (obj == m_lblDbgBody) {
                m_heatmap->clearHighlightedZoneRect();
                return true;
            }
        }

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

/*========================================================================================*/

void MainWindow::updateLiveMonitoring()
{
    // فعلاً فقط تستی
    /*m_lblRiskLive->setText("Risk: 0");
    m_lblMovementLive->setText("Last Move: 0s");

    m_lblAlertsText->setText("No alerts");

    m_lblSacrum->setText("Sacrum: --");
    m_lblHeels->setText("Heels: --");
    m_lblShoulders->setText("Shoulders: --");

    m_lblRecommendation->setText("Monitoring...");*/
}
/*========================================================================================*/
/*void MainWindow::onSummaryReceived(const SummaryData &summary)
{
    // آخرین Summary معتبر را cache می‌کنیم
    m_lastSummary = summary;
    qDebug() << "SUMMARY movementDetected =" << summary.movementDetected
             << "timeSinceLastMovementS =" << summary.timeSinceLastMovementS;
    m_hasSummary = true;

    // زمان دریافت آخرین Summary را نگه می‌داریم
    m_lastSummaryRxMs = QDateTime::currentMSecsSinceEpoch();
}*/



void MainWindow::onSummaryReceived(const SummaryData &summary)
{
    m_lastSummary = summary;
    m_hasSummary = true;
    m_lastSummaryRxMs = QDateTime::currentMSecsSinceEpoch();

    // Frame sync state
    m_frameSync.summaryFrameId = summary.frameId;
    m_frameSync.hasSummary = true;
}
/*========================================================================================*/

void MainWindow::renderSummaryToDashboard()
{
    if (!m_hasSummary)
        return;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool summaryIsFresh =
        (m_lastSummaryRxMs > 0) && ((nowMs - m_lastSummaryRxMs) <= 1500);

    // فعلاً حتی اگر Summary کمی دیر برسد، آخرین داده را نگه می‌داریم
    // تا dashboard نپرد و خوانا بماند.
    Q_UNUSED(summaryIsFresh);

    const SummaryData &summary = m_lastSummary;

    // ---------- Frame Sync Status ----------
    // ---------- Frame Sync Status ----------
    if (m_lblFrameSync) {
        if (isCurrentFrameSynchronized()) {
            m_lblFrameSync->setText(
                QString("Frame %1  •  synced").arg(summary.frameId)
                );

            m_lblFrameSync->setStyleSheet(
                "QLabel {"
                "  color: #8BC48E;"
                "  background-color: #152019;"
                "  border: 1px solid #2F4F3A;"
                "  border-radius: 6px;"
                "  padding: 3px 8px;"
                "  font-size: 11px;"
                "  font-weight: 600;"
                "}"
                );
        } else {
            m_lblFrameSync->setText(
                QString("Frame %1  •  waiting").arg(summary.frameId)
                );

            m_lblFrameSync->setStyleSheet(
                "QLabel {"
                "  color: #D8C07A;"
                "  background-color: #221F18;"
                "  border: 1px solid #5B4B2A;"
                "  border-radius: 6px;"
                "  padding: 3px 8px;"
                "  font-size: 11px;"
                "  font-weight: 600;"
                "}"
                );
        }
    }



    // ---------- Risk ----------
    QString riskLevelText;
    switch (summary.riskLevel) {
    case 0: riskLevelText = "LOW"; break;
    case 1: riskLevelText = "MODERATE"; break;
    case 2: riskLevelText = "HIGH"; break;
    case 3: riskLevelText = "CRITICAL"; break;
    default: riskLevelText = "UNKNOWN"; break;
    }

    if (m_lblRiskLive) {
        QString textColor;
        QString bgColor;
        QString borderColor;

        switch (summary.riskLevel) {
        case 0: // LOW
            textColor = "#A3BE8C";
            bgColor = "#1A221C";
            borderColor = "#2F4F3A";
            break;
        case 1: // MODERATE
            textColor = "#EBCB8B";
            bgColor = "#221F18";
            borderColor = "#5B4B2A";
            break;
        case 2: // HIGH
            textColor = "#D9A066";
            bgColor = "#241D18";
            borderColor = "#6A4A2C";
            break;
        case 3: // CRITICAL
            textColor = "#E7C0C3";
            bgColor = "#241718";
            borderColor = "#6A3238";
            break;
        default:
            textColor = "#D8DEE9";
            bgColor = "#1B1F24";
            borderColor = "#3B4252";
            break;
        }

        m_lblRiskLive->setText(
            QString("RISK\n%1  |  %2")
                .arg(summary.riskScore)
                .arg(riskLevelText)
            );

        m_lblRiskLive->setStyleSheet(QString(
                                         "QLabel {"
                                         "color: %1;"
                                         "background-color: %2;"
                                         "border: 1px solid %3;"
                                         "font-weight: 700;"
                                         "font-size: 16px;"
                                         "padding: 8px 12px;"
                                         "border-radius: 10px;"
                                         "}"
                                         ).arg(textColor, bgColor, borderColor));
    }
    // ---------- Movement ----------
    QString movementText;
    if (summary.timeSinceLastMovementS == 0xFFFF) {
            movementText = "Last Move: No movement yet";
    } else if (summary.timeSinceLastMovementS < 60) {
        movementText = QString("Last Move: %1 s ago")
        .arg(summary.timeSinceLastMovementS);
    } else {
        const int minutes = int(summary.timeSinceLastMovementS) / 60;
        movementText = QString("Last Move: %1 min ago").arg(minutes);
    }

    if (m_lblMovementLive) {
        // متن movement که بالاتر ساخته شده را واقعاً روی label اعمال می‌کنیم
        m_lblMovementLive->setText(movementText);

        m_lblMovementLive->setStyleSheet(
            "QLabel {"
            "color: #E5E9F0;"
            "background-color: #1B1F24;"
            "border: 1px solid #3B4252;"
            "font-weight: 600;"
            "font-size: 15px;"
            "padding: 8px 12px;"
            "border-radius: 10px;"
            "}"
            );
    }

    // ---------- Alert ----------
    if (m_lblAlertsText) {

        if (!summary.alertActive) {
            m_lblAlertsText->setText("No active alerts");
            m_lblAlertsText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            m_lblAlertsText->setStyleSheet(
                "QLabel {"
                "color: #A3BE8C;"
                "background-color: #151C17;"
                "border: 1px solid #2C4433;"
                "padding: 8px 10px;"
                "border-radius: 8px;"
                "font-size: 13px;"
                "font-weight: 600;"
                "}"
                );
        }
        else {
            QString alertTypeText;
            switch (summary.alertType) {
            case 1: alertTypeText = "High pressure"; break;
            default: alertTypeText = "Alert"; break;
            }

            QString sevText;
            QString textColor;
            QString bgColor;
            QString borderColor;

            switch (summary.alertSeverity) {
            case 1:
                sevText = "LOW";
                textColor = "#EBCB8B";
                bgColor = "#221F18";
                borderColor = "#5B4B2A";
                break;
            case 2:
                sevText = "MEDIUM";
                textColor = "#D9A066";
                bgColor = "#241D18";
                borderColor = "#6A4A2C";
                break;
            case 3:
                sevText = "HIGH";
                textColor = "#E7C0C3";
                bgColor = "#241718";
                borderColor = "#6A3238";
                break;
            default:
                sevText = "UNKNOWN";
                textColor = "#D8DEE9";
                bgColor = "#1B1F24";
                borderColor = "#3B4252";
                break;
            }

            m_lblAlertsText->setText(
                QString("%1\nSeverity: %2  •  %3 s")
                    .arg(alertTypeText)
                    .arg(sevText)
                    .arg(summary.alertDurationS)
                );

            m_lblAlertsText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            m_lblAlertsText->setStyleSheet(QString(
                                               "QLabel {"
                                               "color: %1;"
                                               "background-color: %2;"
                                               "border: 1px solid %3;"
                                               "padding: 8px 10px;"
                                               "border-radius: 8px;"
                                               "font-size: 13px;"
                                               "font-weight: 600;"
                                               "}"
                                               ).arg(textColor, bgColor, borderColor));
        }
    }

    // ---------- Recommendation ----------
    // ---------- Recommendation ----------
    if (m_lblRecommendation) {
        QString recText;
        switch (summary.recommendationCode) {
        case 0: recText = "No recommendation"; break;
        case 1: recText = "Monitor"; break;
        case 2: recText = "Reposition patient"; break;
        case 3: recText = "Urgent reposition"; break;
        case 4: recText = "Offload sacrum"; break;
        default: recText = "Unknown recommendation"; break;
        }

        // اگر recommendation مهم باشد، کمی برجسته‌تر نمایش می‌دهیم
        const bool urgent =
            (summary.recommendationCode == 3) || (summary.recommendationPriority >= 3);

        QString textColor = urgent ? "#E7C0C3" : "#D8DEE9";
        QString bgColor = urgent ? "#241718" : "#1B1F24";
        QString borderColor = urgent ? "#6A3238" : "#3B4252";

        m_lblRecommendation->setText(
            QString("Action\n%1").arg(recText)
            );

        m_lblRecommendation->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        m_lblRecommendation->setStyleSheet(QString(
                                               "QLabel {"
                                               "color: %1;"
                                               "background-color: %2;"
                                               "border: 1px solid %3;"
                                               "padding: 8px 10px;"
                                               "border-radius: 8px;"
                                               "font-size: 13px;"
                                               "font-weight: 600;"
                                               "}"
                                               ).arg(textColor, bgColor, borderColor));
    }

    // ---------- Zones ----------
    const int sacrumPressureLike = 63 - int(summary.sacrumAvg);
    const int sacrumPeakPressureLike = 63 - int(summary.sacrumPeak);
    const int heelLeftPressureLike = 63 - int(summary.heelLeftAvg);
    const int heelRightPressureLike = 63 - int(summary.heelRightAvg);

    // ---------- ZONES (Card Style) ----------

    // Sacrum
    if (m_lblSacrum) {

        QString riskTag;
        QString color;

        if (sacrumPressureLike > 45) {
            riskTag = "HIGH";
            color = "#E07A7A";
        } else if (sacrumPressureLike > 30) {
            riskTag = "MED";
            color = "#EBCB8B";
        } else {
            riskTag = "OK";
            color = "#A3BE8C";
        }

        m_lblSacrum->setText(
            QString("SACRUM\nAvg: %1  Peak: %2\n%3")
                .arg(sacrumPressureLike)
                .arg(sacrumPeakPressureLike)
                .arg(riskTag)
            );

        m_lblSacrum->setStyleSheet(QString(
                                       "QLabel {"
                                       "color: %1;"
                                       "background-color: #1B1F24;"
                                       "border: 1px solid #3B4252;"
                                       "padding: 10px;"
                                       "border-radius: 10px;"
                                       "font-weight: 600;"
                                       "}"
                                       ).arg(color));
    }

    // Left Heel
    if (m_lblHeelLeft) {

        int val = heelLeftPressureLike;

        QString color = (val > 40) ? "#E07A7A" :
                            (val > 25) ? "#EBCB8B" :
                            "#A3BE8C";

        m_lblHeelLeft->setText(
            QString("LEFT HEEL\n%1").arg(val)
            );

        m_lblHeelLeft->setStyleSheet(QString(
                                         "QLabel {"
                                         "color: %1;"
                                         "background-color: #1B1F24;"
                                         "border: 1px solid #3B4252;"
                                         "padding: 10px;"
                                         "border-radius: 10px;"
                                         "font-weight: 600;"
                                         "}"
                                         ).arg(color));
    }

    // Right Heel
    if (m_lblHeelRight) {

        int val = heelRightPressureLike;

        QString color = (val > 40) ? "#E07A7A" :
                            (val > 25) ? "#EBCB8B" :
                            "#A3BE8C";

        m_lblHeelRight->setText(
            QString("RIGHT HEEL\n%1").arg(val)
            );

        m_lblHeelRight->setStyleSheet(QString(
                                          "QLabel {"
                                          "color: %1;"
                                          "background-color: #1B1F24;"
                                          "border: 1px solid #3B4252;"
                                          "padding: 10px;"
                                          "border-radius: 10px;"
                                          "font-weight: 600;"
                                          "}"
                                          ).arg(color));
    }

    if (m_compareZoneStats) {
        renderComparedZoneStats();   // NEW: بالاترین اولویت با compare mode
    }
    else if (m_useAdaptiveZoneStats) {
        renderAdaptiveZoneStats();   // NEW: adaptive-only mode
    }

    renderDebugDataPanel();   // NEW: همزمان پنل داده‌های Debug را هم تازه کن
}

/*========================================================================================*/

void MainWindow::renderAdaptiveZoneStats()
{
    if (!m_bedStore.hasFrame())
        return;

    const BodyDetector::Result body =
        BodyDetector::detect(&m_bedStore, 10, 4);

    if (!body.valid)
        return;

    const BodyZones::Zones zones = BodyZones::estimate(body);
    if (!zones.valid) {
        m_lblDbgSacrum->setText("Sacrum: invalid");
        m_lblDbgHeelLeft->setText("Left Heel: invalid");
        m_lblDbgHeelRight->setText("Right Heel: invalid");
        return;
    }
    m_dbgSacrumRect = zones.sacrumRect;
    m_dbgLeftHeelRect = zones.leftHeelRect;
    m_dbgRightHeelRect = zones.rightHeelRect;

    const BodyZoneAnalyzer::Result stats =
        BodyZoneAnalyzer::analyze(&m_bedStore, zones);

    if (!stats.valid)
        return;

    // NEW:
    // فعلاً این بخش فقط برای debug/reference است
    // تا adaptive zone stats را با summary packet مقایسه کنیم.

    if (m_lblSacrum) {
        m_lblSacrum->setText(
            QString("SACRUM\nAvg: %1  Peak: %2")
                .arg(stats.sacrum.avg)
                .arg(stats.sacrum.peak)
            );
    }

    if (m_lblHeelLeft) {
        m_lblHeelLeft->setText(
            QString("LEFT HEEL\nAvg: %1  Peak: %2")
                .arg(stats.leftHeel.avg)
                .arg(stats.leftHeel.peak)
            );
    }

    if (m_lblHeelRight) {
        m_lblHeelRight->setText(
            QString("RIGHT HEEL\nAvg: %1  Peak: %2")
                .arg(stats.rightHeel.avg)
                .arg(stats.rightHeel.peak)
            );
    }
}

/*========================================================================================*/

void MainWindow::renderComparedZoneStats()
{
    if (!m_hasSummary || !m_bedStore.hasFrame())
        return;

    const BodyDetector::Result body =
        BodyDetector::detect(&m_bedStore, 10, 4);

    if (!body.valid)
        return;

    const BodyZones::Zones zones = BodyZones::estimate(body);
    if (!zones.valid)
        return;

    const BodyZoneAnalyzer::Result stats =
        BodyZoneAnalyzer::analyze(&m_bedStore, zones);

    if (!stats.valid)
        return;

    const int devSacrumAvg = int(m_lastSummary.sacrumAvg);
    const int devSacrumPeak = int(m_lastSummary.sacrumPeak);
    const int devLeftHeelAvg = int(m_lastSummary.heelLeftAvg);
    const int devRightHeelAvg = int(m_lastSummary.heelRightAvg);

    const int adpSacrumAvg = stats.sacrum.avg;
    const int adpSacrumPeak = stats.sacrum.peak;
    const int adpLeftHeelAvg = stats.leftHeel.avg;
    const int adpRightHeelAvg = stats.rightHeel.avg;

    const int deltaSacrumAvg = adpSacrumAvg - devSacrumAvg;
    const int deltaSacrumPeak = adpSacrumPeak - devSacrumPeak;
    const int deltaLeftHeelAvg = adpLeftHeelAvg - devLeftHeelAvg;
    const int deltaRightHeelAvg = adpRightHeelAvg - devRightHeelAvg;

    auto deltaText = [](int d) -> QString {
        return (d >= 0)
        ? QString("+%1").arg(d)
        : QString::number(d);
    };

    if (m_lblSacrum) {
        m_lblSacrum->setText(
            QString("SACRUM\n"
                    "Dev  A:%1 P:%2\n"
                    "Adp  A:%3 P:%4\n"
                    "Δ    A:%5 P:%6")
                .arg(devSacrumAvg)
                .arg(devSacrumPeak)
                .arg(adpSacrumAvg)
                .arg(adpSacrumPeak)
                .arg(deltaText(deltaSacrumAvg))
                .arg(deltaText(deltaSacrumPeak))
            );
    }

    if (m_lblHeelLeft) {
        m_lblHeelLeft->setText(
            QString("LEFT HEEL\n"
                    "Dev  A:%1\n"
                    "Adp  A:%2\n"
                    "Δ    A:%3")
                .arg(devLeftHeelAvg)
                .arg(adpLeftHeelAvg)
                .arg(deltaText(deltaLeftHeelAvg))
            );
    }

    if (m_lblHeelRight) {
        m_lblHeelRight->setText(
            QString("RIGHT HEEL\n"
                    "Dev  A:%1\n"
                    "Adp  A:%2\n"
                    "Δ    A:%3")
                .arg(devRightHeelAvg)
                .arg(adpRightHeelAvg)
                .arg(deltaText(deltaRightHeelAvg))
            );
    }
}
/*========================================================================================*/

void MainWindow::renderDebugDataPanel()
{
    if (!m_lblDbgFrame || !m_lblDbgSync || !m_lblDbgSource ||
        !m_lblDbgBody || !m_lblDbgSacrum || !m_lblDbgHeelLeft || !m_lblDbgHeelRight) {
        return;
    }

    // ===== Frame / Sync =====
    QString frameText = "Frame ID: --";
    if (m_bedStore.hasFrame()) {
        frameText = QString("Frame ID: %1").arg(m_bedStore.frameId());
    }
    m_lblDbgFrame->setText(frameText);

    m_lblDbgSync->setText(
        QString("Sync: %1")
            .arg(isCurrentFrameSynchronized() ? "Synced" : "Waiting / Partial")
        );

    // ===== Source Mode =====
    QString sourceText = "Source: Device";
    if (m_compareZoneStats) {
        sourceText = "Source: Compare (Device vs Adaptive)";
    } else if (m_useAdaptiveZoneStats) {
        sourceText = "Source: Adaptive";
    }
    m_lblDbgSource->setText(sourceText);

    // ===== Body / Zones =====
    if (!m_bedStore.hasFrame()) {
        m_lblDbgBody->setText("Body: --");
        m_lblDbgSacrum->setText("Sacrum: --");
        m_lblDbgHeelLeft->setText("Left Heel: --");
        m_lblDbgHeelRight->setText("Right Heel: --");
        return;
    }

    const BodyDetector::Result body =
        BodyDetector::detect(&m_bedStore, 10, 4);

    if (!body.valid) {
        m_lblDbgBody->setText("Body: invalid");
        m_lblDbgSacrum->setText("Sacrum: --");
        m_lblDbgHeelLeft->setText("Left Heel: --");
        m_lblDbgHeelRight->setText("Right Heel: --");
        return;
    }

    m_lblDbgBody->setText(
        QString("Body: T:%1  B:%2  L:%3  R:%4  C:%5  A:%6")
            .arg(body.topRow)
            .arg(body.bottomRow)
            .arg(body.leftCol)
            .arg(body.rightCol)
            .arg(body.centerCol)
            .arg(body.activeCellCount)
        );

    const BodyZones::Zones zones = BodyZones::estimate(body);
    if (!zones.valid) {
        m_lblDbgSacrum->setText("Sacrum: invalid");
        m_lblDbgHeelLeft->setText("Left Heel: invalid");
        m_lblDbgHeelRight->setText("Right Heel: invalid");
        return;
    }

    const BodyZoneAnalyzer::Result stats =
        BodyZoneAnalyzer::analyze(&m_bedStore, zones);

    if (!stats.valid) {
        m_lblDbgSacrum->setText("Sacrum: --");
        m_lblDbgHeelLeft->setText("Left Heel: --");
        m_lblDbgHeelRight->setText("Right Heel: --");
        return;
    }

    m_lblDbgSacrum->setText(
        QString("Sacrum: Avg:%1  Peak:%2  Valid:%3  [R:%4-%5 C:%6-%7]")
            .arg(stats.sacrum.avg)
            .arg(stats.sacrum.peak)
            .arg(stats.sacrum.validCount)
            .arg(zones.sacrumRect.top())
            .arg(zones.sacrumRect.bottom())
            .arg(zones.sacrumRect.left())
            .arg(zones.sacrumRect.right())
        );

    m_lblDbgHeelLeft->setText(
        QString("Left Heel: Avg:%1  Peak:%2  Valid:%3")
            .arg(stats.leftHeel.avg)
            .arg(stats.leftHeel.peak)
            .arg(stats.leftHeel.validCount)
        );

    m_lblDbgHeelRight->setText(
        QString("Right Heel: Avg:%1  Peak:%2  Valid:%3")
            .arg(stats.rightHeel.avg)
            .arg(stats.rightHeel.peak)
            .arg(stats.rightHeel.validCount)
        );
}
/*========================================================================================*/
void MainWindow::onNodeHealth(const NodeHealthPacket &pkt)
{
    // packet 0x30 وضعیت 16 نود را می‌دهد.
    // برای استفاده از UI فعلی، state هر نود را داخل SensorStore می‌نویسیم.
    // در نتیجه updateNodeSummary / refreshNodeCardStyles با همان مسیر فعلی کار می‌کنند.

    const int count = qMin(int(pkt.nodeCount), SensorStore::NODES);

    for (int i = 0; i < count; ++i) {
        NodeState state = NodeState::Offline;

        switch (pkt.nodeState[i]) {
        case 1:
            state = NodeState::Online;
            break;
        case 2:
            state = NodeState::Stale;
            break;
        case 0:
        default:
            state = NodeState::Offline;
            break;
        }

        m_store.setNodeState(i, state);
    }

    // اگر packet کمتر از 16 نود داشت، بقیه را آفلاین در نظر می‌گیریم
    for (int i = count; i < SensorStore::NODES; ++i) {
        m_store.setNodeState(i, NodeState::Offline);
    }

    m_frameSync.nodeHealthFrameId = pkt.frameId;
    m_frameSync.hasNodeHealth = true;

    // این debug فعلاً مفیده؛ بعداً اگر خواستی پاکش می‌کنیم
    qDebug() << "NODE_HEALTH applied, frameId =" << pkt.frameId
             << "nodeCount =" << pkt.nodeCount;
}
/*========================================================================================*/
bool MainWindow::isCurrentFrameSynchronized() const
{
    // فقط وقتی sync معتبر است که هر 4 packet را گرفته باشیم
    if (!m_frameSync.hasSnapshot ||
        !m_frameSync.hasStatus ||
        !m_frameSync.hasNodeHealth ||
        !m_frameSync.hasSummary) {
        return false;
    }

    const quint16 f = m_frameSync.snapshotFrameId;

    return (m_frameSync.statusFrameId == f) &&
           (m_frameSync.nodeHealthFrameId == f) &&
           (m_frameSync.summaryFrameId == f);
}
/*========================================================================================*/

