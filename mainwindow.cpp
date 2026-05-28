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
#include <QDateTime>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QMetaType>

#include "sensordelegate.h"
#include "sensorstatus.h"
#include "summarydata.h"
#include "talma_debug.h"
#include <QGraphicsDropShadowEffect>
/*========================================================================================*/

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_trendHistory.setMaxSamples(600);   // حدود 10 دقیقه history در 1Hz

    // ثبت type برای signal/slot
    qRegisterMetaType<SummaryData>("SummaryData");

    // ساخت تب مستقل Debug
    m_tabDebug = new QWidget();
    m_tabDebug->setObjectName("tabDebug");
    ui->tabs->addTab(m_tabDebug, "Debug");

    // NEW: ساخت تب مستقل Analytics و اضافه کردن به TabWidget
    m_tabAnalytics = new QWidget();
    m_tabAnalytics->setObjectName("tabAnalytics");
    ui->tabs->addTab(m_tabAnalytics, "Analytics");

    // تایمر refresh آرام برای داشبورد
    m_summaryUiTimer = new QTimer(this);
    // ====================== ALERT BLINK TIMER ======================
    m_alertBlinkTimer = new QTimer(this);
    m_alertBlinkTimer->setInterval(700);   // سرعت blink (می‌تونی 300–700 تغییر بدی)

    connect(m_alertBlinkTimer, &QTimer::timeout, this, [this]() {

        if (!m_lblAlerts)
            return;

        if (!m_alertBlinkActive)
            return;

        // toggle
        m_alertBlinkOn = !m_alertBlinkOn;

        if (m_alertBlinkOn)
        {
            m_lblAlerts->setStyleSheet(m_currentAlertStyle);
        }
        else
        {
            // 🔥 به جای خاموش شدن → فقط کم‌رنگ شود
            m_lblAlerts->setStyleSheet(
                "QLabel {"
                "  background-color: #7A2E2E;"   // قرمز تیره
                "  color: rgba(255,255,255,0.75);"
                "  border-radius: 12px;"
                "  padding: 12px;"
                "  border: 2px solid rgba(255,255,255,0.25);"
                "}"
                );
        }
    });
    m_summaryUiTimer->setInterval(250);

    connect(m_summaryUiTimer, &QTimer::timeout, this, [this]() {
        if (!m_hasValidSummary)
            return;

        renderSummaryToDashboard(m_lastSummary);
    });
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

        homeRootLayout->addStretch(1);

        QWidget *homeCenterRow = new QWidget(ui->tabHome);
        QHBoxLayout *centerRowLayout = new QHBoxLayout(homeCenterRow);
        centerRowLayout->setContentsMargins(0, 0, 0, 0);
        centerRowLayout->setSpacing(0);

        centerRowLayout->addStretch(1);

        m_homeContentHost = new QWidget(homeCenterRow);
        m_homeContentHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        m_homeContentHost->setMinimumWidth(700);

        QVBoxLayout *homeContentLayout = new QVBoxLayout(m_homeContentHost);
        homeContentLayout->setContentsMargins(0, 0, 0, 0);
        homeContentLayout->setSpacing(16);

        m_lblHomeTitle = new QLabel("Pressure Mapping & Monitoring", m_homeContentHost);
        m_lblHomeTitle->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

        QFont titleFont = m_lblHomeTitle->font();
        titleFont.setPointSize(20);
        titleFont.setBold(true);
        m_lblHomeTitle->setFont(titleFont);

        homeContentLayout->addWidget(m_lblHomeTitle, 0, Qt::AlignHCenter);

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
 *  2) Settings Page (FIXED + POLISHED)
 * ========================================================= */
    {
        QVBoxLayout *settingsRootLayout = qobject_cast<QVBoxLayout*>(ui->tabSettings->layout());
        if (!settingsRootLayout) {
            settingsRootLayout = new QVBoxLayout(ui->tabSettings);
        }

        settingsRootLayout->setContentsMargins(0, 0, 0, 0);
        settingsRootLayout->setSpacing(0);
        settingsRootLayout->addStretch(1);

        // ================= MAIN PANEL =================
        m_settingsContentHost = new QFrame(ui->tabSettings);
        m_settingsContentHost->setObjectName("settingsContentHost");
        m_settingsContentHost->setMinimumSize(700, 260);
        m_settingsContentHost->setMaximumWidth(900);
        m_settingsContentHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

        m_settingsContentHost->setStyleSheet(
            "#settingsContentHost {"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "              stop:0 #1E293B, stop:1 #0F172A);"
            "  border-radius: 10px;"
            "}"
            );

        QVBoxLayout *settingsContentLayout = new QVBoxLayout(m_settingsContentHost);
        settingsContentLayout->setContentsMargins(20, 20, 20, 20);
        settingsContentLayout->setSpacing(16);

        // ================= COMMON STYLES =================
        const QString groupStyle =
            "QGroupBox {"
            "  border: 1px solid rgba(255,255,255,0.08);"
            "  border-radius: 12px;"
            "  margin-top: 14px;"
            "  padding-top: 12px;"
            "  background-color: rgba(255,255,255,0.03);"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 14px;"
            "  padding: 0 6px;"
            "  color: #E2E8F0;"
            "  font-size: 13px;"
            "  font-weight: 700;"
            "}";

        const QString labelStyle =
            "QLabel {"
            "  color: #CBD5E1;"
            "  font-size: 13px;"
            "  font-weight: 500;"
            "  background: transparent;"
            "  padding: 0px;"
            "}";

        const QString inputStyle =
            "QComboBox {"
            "  background-color: #020617;"
            "  border: 1px solid #334155;"
            "  border-radius: 6px;"
            "  padding: 4px 8px;"
            "  color: #E2E8F0;"
            "  min-height: 22px;"
            "}"
            "QComboBox:hover {"
            "  border: 1px solid #475569;"
            "}"
            "QComboBox:focus {"
            "  border: 1px solid #3B82F6;"
            "}"
            "QComboBox::drop-down {"
            "  border: none;"
            "  width: 20px;"
            "}"
            "QComboBox QAbstractItemView {"
            "  background-color: #020617;"
            "  border: 1px solid #334155;"
            "  selection-background-color: #1E293B;"
            "  color: #E2E8F0;"
            "}"

            "QSpinBox {"
            "  background-color: #020617;"
            "  border: 1px solid #334155;"
            "  border-radius: 6px;"
            "  padding: 2px 6px;"
            "  color: #E2E8F0;"
            "  min-height: 22px;"
            "}"
            "QSpinBox:hover {"
            "  border: 1px solid #475569;"
            "}"
            "QSpinBox:focus {"
            "  border: 1px solid #3B82F6;"
            "}";

        const QString buttonStyle =
            "QPushButton {"
            "  background-color: #1E293B;"
            "  border: 1px solid #334155;"
            "  border-radius: 6px;"
            "  padding: 6px 12px;"
            "  color: #E2E8F0;"
            "  font-weight: 600;"
            "}"
            "QPushButton:hover {"
            "  background-color: #334155;"
            "  border: 1px solid #475569;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #0F172A;"
            "}"
            "QPushButton:disabled {"
            "  background-color: #0B1220;"
            "  color: #64748B;"
            "  border: 1px solid #1F2937;"
            "}";

        const QString statusDisconnectedStyle =
            "QLabel {"
            "  background-color: rgba(239,68,68,0.15);"
            "  color: #EF4444;"
            "  border-radius: 10px;"
            "  padding: 4px 10px;"
            "  font-weight: 600;"
            "}";

        // ================= SERIAL GROUP =================
        QGroupBox *grpSerial = new QGroupBox("Serial Connection", m_settingsContentHost);
        grpSerial->setStyleSheet(groupStyle);

        QVBoxLayout *serialLayout = new QVBoxLayout(grpSerial);
        serialLayout->setContentsMargins(12, 10, 12, 12);
        serialLayout->setSpacing(10);

        QHBoxLayout *serialRow = new QHBoxLayout();
        serialRow->setSpacing(10);

        QLabel *lblPort = new QLabel("Port:", grpSerial);
        lblPort->setStyleSheet(labelStyle);

        m_cmbPortSettings = new QComboBox(grpSerial);
        m_cmbPortSettings->setMinimumWidth(220);
        m_cmbPortSettings->setStyleSheet(inputStyle);

        m_btnRefreshSettings = new QPushButton("Refresh", grpSerial);
        m_btnRefreshSettings->setMinimumHeight(30);
        m_btnRefreshSettings->setStyleSheet(buttonStyle);

        m_btnConnectSettings = new QPushButton("Connect", grpSerial);
        m_btnConnectSettings->setMinimumHeight(30);
        m_btnConnectSettings->setStyleSheet(buttonStyle);

        m_lblStatusSettings = new QLabel("● Disconnected", grpSerial);
        m_lblStatusSettings->setStyleSheet(statusDisconnectedStyle);
        m_lblStatusSettings->setAlignment(Qt::AlignCenter);
        m_lblStatusSettings->setMinimumHeight(30);

        serialRow->addWidget(lblPort);
        serialRow->addWidget(m_cmbPortSettings, 1);
        serialRow->addWidget(m_btnRefreshSettings);
        serialRow->addWidget(m_btnConnectSettings);
        serialRow->addSpacing(8);
        serialRow->addWidget(m_lblStatusSettings);

        serialLayout->addLayout(serialRow);

        // ================= DISPLAY GROUP =================
        QGroupBox *grpDisplay = new QGroupBox("Display Settings", m_settingsContentHost);
        grpDisplay->setStyleSheet(groupStyle);

        QVBoxLayout *displayLayout = new QVBoxLayout(grpDisplay);
        displayLayout->setContentsMargins(12, 10, 12, 12);
        displayLayout->setSpacing(10);

        QHBoxLayout *rangeRow = new QHBoxLayout();
        rangeRow->setSpacing(10);

        QLabel *lblHigh = new QLabel("Red:", grpDisplay);
        lblHigh->setStyleSheet(labelStyle);

        m_spHighSettings = new QSpinBox(grpDisplay);
        m_spHighSettings->setRange(0, 63);
        m_spHighSettings->setValue(5);
        m_spHighSettings->setMinimumWidth(64);
        m_spHighSettings->setStyleSheet(inputStyle);

        QLabel *lblNo = new QLabel("Blue:", grpDisplay);
        lblNo->setStyleSheet(labelStyle);

        m_spNoSettings = new QSpinBox(grpDisplay);
        m_spNoSettings->setRange(0, 63);
        m_spNoSettings->setValue(50);
        m_spNoSettings->setMinimumWidth(64);
        m_spNoSettings->setStyleSheet(inputStyle);

        rangeRow->addWidget(lblHigh);
        rangeRow->addWidget(m_spHighSettings);
        rangeRow->addSpacing(12);
        rangeRow->addWidget(lblNo);
        rangeRow->addWidget(m_spNoSettings);
        rangeRow->addStretch(1);

        displayLayout->addLayout(rangeRow);

        // ================= ADD TO ROOT =================
        settingsContentLayout->addWidget(grpSerial);
        settingsContentLayout->addWidget(grpDisplay);

        settingsRootLayout->addWidget(m_settingsContentHost, 0, Qt::AlignHCenter);
        settingsRootLayout->addStretch(1);
    }

    /* =========================================================
 *  2.5) Debug Page (CLEAN REBUILD)
 * ========================================================= */
    {
        QVBoxLayout *debugRootLayout = qobject_cast<QVBoxLayout*>(m_tabDebug->layout());
        if (!debugRootLayout) {
            debugRootLayout = new QVBoxLayout(m_tabDebug);
        }

        debugRootLayout->setContentsMargins(0, 0, 0, 0);
        debugRootLayout->setSpacing(0);
        debugRootLayout->addStretch(1);

        // ================= MAIN PANEL =================
        m_debugContentHost = new QFrame(m_tabDebug);
        m_debugContentHost->setObjectName("debugContentHost");
        m_debugContentHost->setMinimumSize(760, 420);
        m_debugContentHost->setMaximumWidth(980);
        m_debugContentHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

        m_debugContentHost->setStyleSheet(
            "#debugContentHost {"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "              stop:0 #1E293B, stop:1 #0F172A);"
            "  border-radius: 10px;"
            "}"
            );

        QVBoxLayout *debugContentLayout = new QVBoxLayout(m_debugContentHost);
        debugContentLayout->setContentsMargins(20, 20, 20, 20);
        debugContentLayout->setSpacing(16);

        // ================= COMMON STYLE =================
        const QString debugGroupStyle =
            "QGroupBox {"
            "  border: 1px solid rgba(255,255,255,0.08);"
            "  border-radius: 12px;"
            "  margin-top: 14px;"
            "  padding-top: 12px;"
            "  background-color: rgba(255,255,255,0.03);"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 14px;"
            "  padding: 0 6px;"
            "  color: #E2E8F0;"
            "  font-size: 13px;"
            "  font-weight: 700;"
            "}";

        const QString checkStyle =
            "QCheckBox {"
            "  color: #CBD5E1;"
            "  font-size: 14px;"
            "  spacing: 8px;"
            "}"
            "QCheckBox::indicator {"
            "  width: 16px;"
            "  height: 16px;"
            "}"
            "QCheckBox::indicator:unchecked {"
            "  border: 1px solid #475569;"
            "  border-radius: 4px;"
            "  background: transparent;"
            "}"
            "QCheckBox::indicator:checked {"
            "  border: 1px solid #22C55E;"
            "  border-radius: 4px;"
            "  background-color: #22C55E;"
            "}";

        const QString radioStyle =
            "QRadioButton {"
            "  color: #CBD5E1;"
            "  font-size: 13px;"
            "  spacing: 8px;"
            "}"
            "QRadioButton::indicator {"
            "  width: 16px;"
            "  height: 16px;"
            "}"
            "QRadioButton::indicator:unchecked {"
            "  border: 1px solid #475569;"
            "  border-radius: 8px;"
            "   background: transparent;"
            "}"
            "QRadioButton::indicator:checked {"
            "  border: 1px solid #38BDF8;"
            "  border-radius: 8px;"
            "  background-color: #38BDF8;"
            "}";

        const QString debugValueStyle =
            "QLabel {"
            "  color: #F8FAFC;"
            "  background-color: rgba(2,6,23,0.88);"
            "  border: 1px solid #334155;"
            "  border-radius: 10px;"
            "  padding: 12px 14px;"
            "  font-size: 13px;"
            "  font-weight: 600;"
            "}";

        // ================= TITLE =================
        m_lblDebugTitle = new QLabel("Debug & Diagnostics", m_debugContentHost);
        m_lblDebugTitle->setStyleSheet(
            "QLabel {"
            "  font-size: 18px;"
            "  font-weight: 700;"
            "  color: #F2F4F8;"
            "}"
            );
        debugContentLayout->addWidget(m_lblDebugTitle, 0, Qt::AlignLeft);

        // ================= VISUAL DEBUG =================
        QGroupBox *grpVisualDebug = new QGroupBox("Visual Debug", m_debugContentHost);
        grpVisualDebug->setStyleSheet(debugGroupStyle);

        QVBoxLayout *visualDebugLayout = new QVBoxLayout(grpVisualDebug);
        visualDebugLayout->setContentsMargins(14, 10, 14, 12);
        visualDebugLayout->setSpacing(6);

        m_chkShowDebugText = new QCheckBox("Show cell debug text", grpVisualDebug);
        m_chkShowTooltip = new QCheckBox("Show hover tooltip", grpVisualDebug);
        m_chkShowBodyBounds = new QCheckBox("Show body bounds", grpVisualDebug);
        m_chkShowBodyZones = new QCheckBox("Show body zones", grpVisualDebug);
        m_chkShowZoneValues = new QCheckBox("Show zone values", grpVisualDebug);

        m_chkShowDebugText->setChecked(false);
        m_chkShowTooltip->setChecked(true);
        m_chkShowBodyBounds->setChecked(false);
        m_chkShowBodyZones->setChecked(false);
        m_chkShowZoneValues->setChecked(false);

        m_chkShowDebugText->setStyleSheet(checkStyle);
        m_chkShowTooltip->setStyleSheet(checkStyle);
        m_chkShowBodyBounds->setStyleSheet(checkStyle);
        m_chkShowBodyZones->setStyleSheet(checkStyle);
        m_chkShowZoneValues->setStyleSheet(checkStyle);

        visualDebugLayout->addWidget(m_chkShowDebugText);
        visualDebugLayout->addWidget(m_chkShowTooltip);
        visualDebugLayout->addWidget(m_chkShowBodyBounds);
        visualDebugLayout->addWidget(m_chkShowBodyZones);
        visualDebugLayout->addWidget(m_chkShowZoneValues);

        // ================= ZONE SOURCE MODE =================
        m_grpZoneStatsMode = new QGroupBox("Dashboard Zone Stats Source", m_debugContentHost);
        m_grpZoneStatsMode->setStyleSheet(debugGroupStyle);

        QVBoxLayout *zoneModeLayout = new QVBoxLayout(m_grpZoneStatsMode);
        zoneModeLayout->setContentsMargins(14, 10, 14, 12);
        zoneModeLayout->setSpacing(6);

        m_radZoneStatsDevice = new QRadioButton("Device stats", m_grpZoneStatsMode);
        m_radZoneStatsAdaptive = new QRadioButton("Adaptive stats", m_grpZoneStatsMode);
        m_radZoneStatsCompare = new QRadioButton("Compare device vs adaptive", m_grpZoneStatsMode);

        m_radZoneStatsDevice->setStyleSheet(radioStyle);
        m_radZoneStatsAdaptive->setStyleSheet(radioStyle);
        m_radZoneStatsCompare->setStyleSheet(radioStyle);

        m_radZoneStatsDevice->setChecked(false);
        m_radZoneStatsAdaptive->setChecked(true);
        m_radZoneStatsCompare->setChecked(false);

        zoneModeLayout->addWidget(m_radZoneStatsDevice);
        zoneModeLayout->addWidget(m_radZoneStatsAdaptive);
        zoneModeLayout->addWidget(m_radZoneStatsCompare);

        // ================= DEBUG DATA PANEL =================
        m_grpDebugData = new QGroupBox("Data Debug", m_debugContentHost);
        m_grpDebugData->setStyleSheet(debugGroupStyle);

        QVBoxLayout *debugDataLayout = new QVBoxLayout(m_grpDebugData);
        debugDataLayout->setContentsMargins(14, 10, 14, 12);
        debugDataLayout->setSpacing(8);

        auto makeDebugValueLabel = [this, &debugValueStyle]() -> QLabel* {
            QLabel *lbl = new QLabel(m_grpDebugData);
            lbl->setStyleSheet(debugValueStyle);
            lbl->setWordWrap(true);
            lbl->setMinimumHeight(40);
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

        //debugDataLayout->addWidget(m_lblDbgFrame);
       // debugDataLayout->addWidget(m_lblDbgSync);
        QGridLayout *grid = new QGridLayout();
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(12);

        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);

        grid->addWidget(m_lblDbgFrame,     0, 0);
        grid->addWidget(m_lblDbgSync,      0, 1);

        grid->addWidget(m_lblDbgSource,    1, 0);
        grid->addWidget(m_lblDbgBody,      1, 1);

        grid->addWidget(m_lblDbgSacrum,    2, 0);
        grid->addWidget(m_lblDbgHeelLeft,  2, 1);

        grid->addWidget(m_lblDbgHeelRight, 3, 0);

        QLabel *m_lblDbgPlaceholder = new QLabel("", m_grpDebugData);
        m_lblDbgPlaceholder->setStyleSheet(
            "QLabel {"
            "  background: transparent;"
            "  border: none;"
            "}"
            );
        grid->addWidget(m_lblDbgPlaceholder, 3, 1);

       /* debugDataLayout->addLayout(grid);
        debugDataLayout->addWidget(m_lblDbgSource);
        debugDataLayout->addWidget(m_lblDbgBody);
        debugDataLayout->addWidget(m_lblDbgSacrum);
        debugDataLayout->addWidget(m_lblDbgHeelLeft);
        debugDataLayout->addWidget(m_lblDbgHeelRight);*/

        // ================= LAYOUT =================
        QHBoxLayout *topDebugRow = new QHBoxLayout();
        topDebugRow->setSpacing(12);
        topDebugRow->addWidget(grpVisualDebug, 1);
        topDebugRow->addWidget(m_grpZoneStatsMode, 1);

        debugContentLayout->addLayout(topDebugRow);
        debugContentLayout->addWidget(m_grpDebugData);


        /* =========================================================
 *  Main Board Debug Console
 *
 *  Purpose:
 *  - UI-side control panel for Main Board debug commands
 *  - protocol simulation
 *  - intervention workflow testing
 *  - future motor/CAN diagnostics
 *
 *  IMPORTANT:
 *  This section is for development/service use only.
 *  In production, it can be hidden or permission-locked.
 * ========================================================= */
        m_grpMainBoardDebug = new QGroupBox("Main Board Debug Console", m_debugContentHost);
        m_grpMainBoardDebug->setStyleSheet(debugGroupStyle);

        QVBoxLayout *mainBoardDbgLayout = new QVBoxLayout(m_grpMainBoardDebug);
        mainBoardDbgLayout->setContentsMargins(14, 10, 14, 12);
        mainBoardDbgLayout->setSpacing(8);

        // Master safety gate for debug commands
        m_chkEnableMainBoardDebug = new QCheckBox("Enable Main Board debug commands", m_grpMainBoardDebug);
        m_chkEnableMainBoardDebug->setChecked(false);
        m_chkEnableMainBoardDebug->setStyleSheet(checkStyle);
        mainBoardDbgLayout->addWidget(m_chkEnableMainBoardDebug);

        // Debug command buttons
        QGridLayout *mainBoardDbgButtonGrid = new QGridLayout();
        mainBoardDbgButtonGrid->setHorizontalSpacing(8);
        mainBoardDbgButtonGrid->setVerticalSpacing(8);

        m_btnDbgTestPlan = new QPushButton("Test Intervention Plan", m_grpMainBoardDebug);
        m_btnDbgResultExecuting = new QPushButton("Result: Executing", m_grpMainBoardDebug);
        m_btnDbgResultCompleted = new QPushButton("Result: Completed", m_grpMainBoardDebug);
        m_btnDbgResultFailed = new QPushButton("Result: Failed", m_grpMainBoardDebug);

        m_btnDbgTestPlan->setEnabled(false);
        m_btnDbgResultExecuting->setEnabled(false);
        m_btnDbgResultCompleted->setEnabled(false);
        m_btnDbgResultFailed->setEnabled(false);


        mainBoardDbgButtonGrid->addWidget(m_btnDbgTestPlan, 0, 0);
        mainBoardDbgButtonGrid->addWidget(m_btnDbgResultExecuting, 0, 1);
        mainBoardDbgButtonGrid->addWidget(m_btnDbgResultCompleted, 1, 0);
        mainBoardDbgButtonGrid->addWidget(m_btnDbgResultFailed, 1, 1);

        mainBoardDbgLayout->addLayout(mainBoardDbgButtonGrid);


        // ======================================================
        // Runtime command controls
        //
        // These controls send runtime configuration commands to
        // the Main Board command channel.
        //
        // TYPE 0x60:
        // - Therapy/risk preset selection
        //
        // TYPE 0x61:
        // - Synthetic bed test pattern selection
        //
        // NOTE:
        // Controls are placed inside the Main Board Debug Console
        // so they stay grouped with other UI -> Main commands.
        // ======================================================
        QGroupBox *grpRuntimeCommands =
            new QGroupBox("Runtime Commands", m_grpMainBoardDebug);

        grpRuntimeCommands->setStyleSheet(debugGroupStyle);

        QVBoxLayout *runtimeCommandLayout =
            new QVBoxLayout(grpRuntimeCommands);

        runtimeCommandLayout->setContentsMargins(14, 10, 14, 12);
        runtimeCommandLayout->setSpacing(8);

        // ======================================================
        // Therapy/risk preset selector.
        //
        // 0 = DEMO
        // 1 = CLINICAL_TEST
        // ======================================================
        m_comboTherapyPreset = new QComboBox(grpRuntimeCommands);

        m_comboTherapyPreset->addItem("DEMO", 0);
        m_comboTherapyPreset->addItem("CLINICAL_TEST", 1);

        m_comboTherapyPreset->setToolTip(
            "Select Main Board therapy/risk behavior preset"
            );

        // ======================================================
        // Synthetic bed pattern selector.
        //
        // Pattern IDs are defined by Main Board firmware.
        // ======================================================
        m_comboTestPattern = new QComboBox(grpRuntimeCommands);

        m_comboTestPattern->addItem("REAL_DATA",                  0);
        m_comboTestPattern->addItem("GRADIENT_DEBUG",             1);
        m_comboTestPattern->addItem("CHECKERBOARD_DEBUG",         2);
        m_comboTestPattern->addItem("BODY_REALISTIC_SUPINE",      3);
        m_comboTestPattern->addItem("BODY_ADULT_NORMAL",          4);
        m_comboTestPattern->addItem("BODY_SHORT_LIGHT",           5);
        m_comboTestPattern->addItem("BODY_SHIFT_LEFT",            6);
        m_comboTestPattern->addItem("BODY_SHIFT_RIGHT",           7);
        m_comboTestPattern->addItem("BODY_SACRUM_DOMINANT",       8);
        m_comboTestPattern->addItem("BODY_ONE_HEEL_DOMINANT",     9);
        m_comboTestPattern->addItem("BODY_PARTIAL_FAULT",         10);
        m_comboTestPattern->addItem("BODY_RESTLESS",              11);
        m_comboTestPattern->addItem("BODY_SIDE_LEFT",             12);
        m_comboTestPattern->addItem("BODY_SIDE_RIGHT",            13);
        m_comboTestPattern->addItem("BODY_TURNING_CYCLE",         14);
        m_comboTestPattern->addItem("BODY_TURNING_CYCLE_SMOOTH",  15);

        m_comboTestPattern->setToolTip(
            "Select Main Board synthetic heatmap pattern"
            );

        // ======================================================
        // Add runtime command controls to debug layout.
        // ======================================================
        runtimeCommandLayout->addWidget(new QLabel("Therapy Preset:", grpRuntimeCommands));
        runtimeCommandLayout->addWidget(m_comboTherapyPreset);

        runtimeCommandLayout->addSpacing(8);

        runtimeCommandLayout->addWidget(new QLabel("Test Pattern:", grpRuntimeCommands));
        runtimeCommandLayout->addWidget(m_comboTestPattern);

        mainBoardDbgLayout->addWidget(grpRuntimeCommands);

        // Status labels
        m_lblDbgMainBoardStatus = new QLabel("Status: Debug disabled", m_grpMainBoardDebug);
        m_lblDbgLastTx = new QLabel("Last TX: --", m_grpMainBoardDebug);
        m_lblDbgLastRx = new QLabel("Last RX: --", m_grpMainBoardDebug);

        m_lblDbgMainBoardStatus->setStyleSheet(debugValueStyle);
        m_lblDbgLastTx->setStyleSheet(debugValueStyle);
        m_lblDbgLastRx->setStyleSheet(debugValueStyle);

        mainBoardDbgLayout->addWidget(m_lblDbgMainBoardStatus);
        mainBoardDbgLayout->addWidget(m_lblDbgLastTx);
        mainBoardDbgLayout->addWidget(m_lblDbgLastRx);

        debugContentLayout->addWidget(m_grpMainBoardDebug);

        // ======================================================
        // DEBUG:
        // Enable/disable Main Board debug mode.
        //
        // This controls whether simulation/debug commands
        // are allowed to be sent to Main Board.
        // ======================================================
        connect(m_chkEnableMainBoardDebug,
                &QCheckBox::toggled,
                this,
                [this](bool enabled)
                {
                    #if TALMA_DEBUG_MAINBOARD
                        qDebug() << "[DEBUG UI] Main Board debug toggled =" << enabled;
                    #endif

                    m_btnDbgTestPlan->setEnabled(enabled);

                    m_btnDbgResultExecuting->setEnabled(enabled);

                    m_btnDbgResultCompleted->setEnabled(enabled);

                    m_btnDbgResultFailed->setEnabled(enabled);

                    m_lblDbgMainBoardStatus->setText(
                        enabled
                            ? "Status: Debug commands ENABLED"
                            : "Status: Debug commands DISABLED");
                });


        // ======================================================
        // Runtime command:
        // Send therapy/risk preset selection to Main Board.
        //
        // TYPE = 0x60
        //
        // Safety:
        // - Command is only sent when Main Board debug commands
        //   are enabled.
        // - This prevents accidental runtime mode changes.
        // ======================================================
        connect(m_comboTherapyPreset,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int index)
                {
                    if (!m_chkEnableMainBoardDebug ||
                        !m_chkEnableMainBoardDebug->isChecked()) {

                        qWarning() << "[RUNTIME CMD] Therapy preset ignored:"
                                   << "debug commands disabled";
                        return;
                    }

                    if (!m_port.isOpen()) {
                        qWarning() << "[RUNTIME CMD] Therapy preset ignored:"
                                   << "serial port is closed";

                        if (m_lblDbgMainBoardStatus)
                            m_lblDbgMainBoardStatus->setText("Status: Serial CLOSED");

                        return;
                    }

                    const quint8 preset =
                        quint8(m_comboTherapyPreset->itemData(index).toUInt());

                    m_rx.sendTherapyPreset(preset);

                    if (m_lblDbgLastTx) {
                        m_lblDbgLastTx->setText(
                            QString("Last TX: THERAPY_PRESET preset=%1")
                                .arg(preset));
                    }
                });


        // ======================================================
        // Runtime command:
        // Send synthetic test pattern selection to Main Board.
        //
        // TYPE = 0x61
        //
        // Safety:
        // - Command is only sent when Main Board debug commands
        //   are enabled.
        // - This prevents accidental runtime pattern changes.
        // ======================================================
        connect(m_comboTestPattern,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int index)
                {
                    if (!m_chkEnableMainBoardDebug ||
                        !m_chkEnableMainBoardDebug->isChecked()) {

                        qWarning() << "[RUNTIME CMD] Test pattern ignored:"
                                   << "debug commands disabled";
                        return;
                    }

                    if (!m_port.isOpen()) {
                        qWarning() << "[RUNTIME CMD] Test pattern ignored:"
                                   << "serial port is closed";

                        if (m_lblDbgMainBoardStatus)
                            m_lblDbgMainBoardStatus->setText("Status: Serial CLOSED");

                        return;
                    }

                    const quint8 patternId =
                        quint8(m_comboTestPattern->itemData(index).toUInt());

                    m_rx.sendTestPattern(patternId);

                    if (m_lblDbgLastTx) {
                        m_lblDbgLastTx->setText(
                            QString("Last TX: TEST_PATTERN pattern=%1")
                                .arg(patternId));
                    }
                });


        // ======================================================
        // DEBUG:
        // Request Main Board to send fake INTERVENTION_RESULT
        // with state = EXECUTING.
        // ======================================================
        connect(m_btnDbgResultExecuting,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    #if TALMA_DEBUG_MAINBOARD
                        qDebug() << "[DEBUG UI] Fake EXECUTING result requested";
                    #endif
                    m_rx.sendDebugCommand(2, 1);
                });

        // ======================================================
        // DEBUG:
        // Request Main Board to send fake INTERVENTION_RESULT
        // with state = COMPLETED.
        // ======================================================
        connect(m_btnDbgResultCompleted,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    #if TALMA_DEBUG_MAINBOARD
                        qDebug() << "[DEBUG UI] Fake COMPLETED result requested";
                    #endif
                    m_rx.sendDebugCommand(3, 1);
                });

        // ======================================================
        // DEBUG:
        // Request Main Board to send fake INTERVENTION_RESULT
        // with state = FAILED.
        // ======================================================
        connect(m_btnDbgResultFailed,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    #if TALMA_DEBUG_MAINBOARD
                        qDebug() << "[DEBUG UI] Fake FAILED result requested";
                    #endif
                    m_rx.sendDebugCommand(4, 1);

                });

        debugRootLayout->addWidget(m_debugContentHost, 0, Qt::AlignHCenter);
        debugRootLayout->addStretch(1);
    }


    // ======================================================
    // DEBUG:
    // Test Intervention Plan button.
    //
    // When clicked:
    // UI sends DEBUG_COMMAND to Main Board.
    //
    // Expected:
    // Main Board responds with TYPE 0x51 INTERVENTION_PLAN.
    // ======================================================
    connect(m_btnDbgTestPlan,
            &QPushButton::clicked,
            this,
            [this]()
            {
                #if TALMA_DEBUG_MAINBOARD
                 qDebug() << "[DEBUG UI] Test Intervention Plan button clicked";
                #endif

                if (!m_chkEnableMainBoardDebug->isChecked()) {
                    qWarning() << "[DEBUG UI] Debug mode is disabled";
                    return;
                }

                if (!m_port.isOpen()) {
                    qWarning() << "[DEBUG UI] Serial port is closed";
                    m_lblDbgMainBoardStatus->setText("Status: Serial CLOSED");
                    return;
                }

                m_rx.sendDebugCommand(1, 1);

                m_lblDbgMainBoardStatus->setText("Status: Test plan requested");
                m_lblDbgLastTx->setText("Last TX: DEBUG_COMMAND cmd=1 param=1");
            });






    /* =========================================================
     *  2.6) Analytics Page (runtime-built, separate tab)
     * ========================================================= */
    {
        QVBoxLayout *analyticsRootLayout = qobject_cast<QVBoxLayout*>(m_tabAnalytics->layout());
        if (!analyticsRootLayout) {
            analyticsRootLayout = new QVBoxLayout(m_tabAnalytics);
        }

        analyticsRootLayout->setContentsMargins(0, 0, 0, 0);
        analyticsRootLayout->setSpacing(0);
        analyticsRootLayout->addStretch(1);

        m_analyticsContentHost = new QFrame(m_tabAnalytics);
        m_analyticsContentHost->setObjectName("analyticsContentHost");
        m_analyticsContentHost->setMinimumSize(980, 760);
        m_analyticsContentHost->setMaximumWidth(1280);
        m_analyticsContentHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        m_analyticsContentHost->setStyleSheet(
            "#analyticsContentHost {"
            "  background-color: rgb(18, 22, 30);"
            "  border-radius: 10px;"
            "}"
            );

        QVBoxLayout *analyticsContentLayout = new QVBoxLayout(m_analyticsContentHost);
        analyticsContentLayout->setContentsMargins(20, 20, 20, 20);
        analyticsContentLayout->setSpacing(18);

        // ====================== Title ======================
        m_lblAnalyticsTitle = new QLabel("Analytics & Trend Overview", m_analyticsContentHost);
        m_lblAnalyticsTitle->setStyleSheet(
            "QLabel {"
            "  font-size: 20px;"
            "  font-weight: 700;"
            "  color: #F2F4F8;"
            "}"
            );
        analyticsContentLayout->addWidget(m_lblAnalyticsTitle, 0, Qt::AlignLeft);

        // ====================== Clinical Summary Row ======================
        m_analyticsSummaryRow = new QWidget(m_analyticsContentHost);
        QHBoxLayout *summaryRowLayout = new QHBoxLayout(m_analyticsSummaryRow);
        summaryRowLayout->setContentsMargins(0, 0, 0, 2);
        summaryRowLayout->setSpacing(6);

        auto makeClinicalCard = [](const QString &title) -> QLabel*
        {
            QLabel *lbl = new QLabel();
            lbl->setMinimumHeight(58);
            lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            lbl->setWordWrap(true);
            lbl->setStyleSheet(
                "QLabel {"
                "  background-color: #111827;"
                "  border: 1px solid #334155;"
                "  border-radius: 10px;"
                "  padding: 8px 12px;"
                "  color: #F8FAFC;"
                "  font-size: 13px;"
                "  font-weight: 700;"
                "}"
                );
            lbl->setText(title);
            return lbl;
        };

        m_lblClinicalLastMove = makeClinicalCard("Move: --");
        m_lblClinicalTopZone = makeClinicalCard("Top: --");
        m_lblClinicalMaxExposure = makeClinicalCard("Exp: --");
        m_lblClinicalAction = makeClinicalCard("Action: --");

        summaryRowLayout->addWidget(m_lblClinicalLastMove, 1);
        summaryRowLayout->addWidget(m_lblClinicalTopZone, 1);
        summaryRowLayout->addWidget(m_lblClinicalMaxExposure, 1);
        summaryRowLayout->addWidget(m_lblClinicalAction, 1);

        analyticsContentLayout->addWidget(m_analyticsSummaryRow);

        const QString analyticsGroupStyle =
            "QGroupBox {"
            "  border: 1px solid #2D3742;"
            "  border-radius: 10px;"
            "  margin-top: 8px;"
            "  padding-top: 10px;"
            "  background-color: #15191E;"
            "  font-size: 14px;"
            "  font-weight: 600;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 10px;"
            "  top: 0px;"
            "  padding: 0px 6px 0px 6px;"
            "  color: #E7ECF3;"
            "}";

        const QString legendLabelStyleOrange =
            "QLabel { color: #FF9800; font-size: 10px; font-weight: 700; background: transparent; padding: 0px; margin: 0px; }";
        const QString legendLabelStyleRed =
            "QLabel { color: #EF4444; font-size: 10px; font-weight: 700; background: transparent; padding: 0px; margin: 0px; }";
        const QString legendLabelStyleGreen =
            "QLabel { color: #22C55E; font-size: 10px; font-weight: 700; background: transparent; padding: 0px; margin: 0px; }";
        const QString legendLabelStyleBlue =
            "QLabel { color: #38BDF8; font-size: 10px; font-weight: 700; background: transparent; padding: 0px; margin: 0px; }";

        // ====================== Risk Trend ======================
        QGroupBox *grpRiskTrend = new QGroupBox("Risk Trend", m_analyticsContentHost);
        grpRiskTrend->setStyleSheet(analyticsGroupStyle +
                                    "QGroupBox::title {"
                                    "  font-size: 16px;"
                                    "  font-weight: 700;"
                                    "  color: #F1F5F9;"
                                    "}");

        QVBoxLayout *riskLayout = new QVBoxLayout(grpRiskTrend);
        riskLayout->setContentsMargins(12, 2, 12, 8);
        riskLayout->setSpacing(1);

        QWidget *riskLegendRow = new QWidget(grpRiskTrend);
        riskLegendRow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        riskLegendRow->setFixedHeight(12);
        riskLegendRow->setStyleSheet("background: transparent;");

        QHBoxLayout *riskLegendLayout = new QHBoxLayout(riskLegendRow);
        riskLegendLayout->setContentsMargins(0, 0, 0, 0);
        riskLegendLayout->setSpacing(10);

        QLabel *riskLegendRisk = new QLabel("■ Risk", riskLegendRow);
        riskLegendRisk->setStyleSheet(legendLabelStyleOrange);

        QLabel *riskLegendThreshold = new QLabel("■ Threshold", riskLegendRow);
        riskLegendThreshold->setStyleSheet(legendLabelStyleRed);

        riskLegendLayout->addStretch(1);
        riskLegendLayout->addWidget(riskLegendRisk);
        riskLegendLayout->addWidget(riskLegendThreshold);
        riskLegendLayout->addStretch(1);

        riskLayout->addWidget(riskLegendRow);

        m_seriesRisk = new QLineSeries();
        m_seriesRisk->setName("Risk");

        QPen riskPen(QColor("#FF9800"));
        riskPen.setWidth(3);
        m_seriesRisk->setPen(riskPen);

        m_seriesRiskThreshold = new QLineSeries();
        m_seriesRiskThreshold->setName("Threshold");
        m_seriesRiskThreshold->append(0, 70);
        m_seriesRiskThreshold->append(60, 70);

        QPen thresholdPen(QColor("#EF4444"));
        thresholdPen.setWidth(2);
        thresholdPen.setStyle(Qt::DashLine);
        m_seriesRiskThreshold->setPen(thresholdPen);

        QChart *riskChart = new QChart();
        riskChart->setMargins(QMargins(0, 0, 0, 12));
        riskChart->layout()->setContentsMargins(0, 0, 0, 12);
        riskChart->addSeries(m_seriesRisk);
        riskChart->addSeries(m_seriesRiskThreshold);
        riskChart->setTitle("");
        riskChart->setBackgroundVisible(false);
        riskChart->setPlotAreaBackgroundVisible(true);
        riskChart->setPlotAreaBackgroundBrush(QColor("#0B1220"));
        riskChart->legend()->hide();

        QValueAxis *riskAxisX = new QValueAxis();
        riskAxisX->setRange(0, 60);
        riskAxisX->setTickCount(5);
        riskAxisX->setLabelFormat("%d");
        riskAxisX->setTitleText("Time");
        riskAxisX->setTitleBrush(QBrush(QColor("#E2E8F0")));
        riskAxisX->setGridLineColor(QColor("#2D3742"));
        riskAxisX->setLabelsColor(QColor("#E2E8F0"));
        QFont riskAxisXLabelsFont;
        riskAxisXLabelsFont.setPointSize(8);
        riskAxisX->setLabelsFont(riskAxisXLabelsFont);
        QFont riskAxisXTitleFont;
        riskAxisXTitleFont.setPointSize(10);
        riskAxisXTitleFont.setBold(true);
        riskAxisX->setTitleFont(riskAxisXTitleFont);

        QValueAxis *riskAxisY = new QValueAxis();
        riskAxisY->setRange(0, 100);
        riskAxisY->setTickCount(5);
        riskAxisY->setLabelFormat("%d");
        riskAxisY->setTitleText("Risk Score (%)");
        riskAxisY->setTitleBrush(QBrush(QColor("#E2E8F0")));
        riskAxisY->setGridLineColor(QColor("#2D3742"));
        riskAxisY->setLabelsColor(QColor("#E2E8F0"));
        QFont riskAxisYLabelsFont;
        riskAxisYLabelsFont.setPointSize(10);
        riskAxisY->setLabelsFont(riskAxisYLabelsFont);
        QFont riskAxisYTitleFont;
        riskAxisYTitleFont.setPointSize(11);
        riskAxisYTitleFont.setBold(true);
        riskAxisY->setTitleFont(riskAxisYTitleFont);

        riskChart->addAxis(riskAxisX, Qt::AlignBottom);
        riskChart->addAxis(riskAxisY, Qt::AlignLeft);

        m_seriesRisk->attachAxis(riskAxisX);
        m_seriesRisk->attachAxis(riskAxisY);
        m_seriesRiskThreshold->attachAxis(riskAxisX);
        m_seriesRiskThreshold->attachAxis(riskAxisY);

        m_chartRiskView = new QChartView(riskChart, grpRiskTrend);
        m_chartRiskView->setMinimumHeight(240);
        m_chartRiskView->setRenderHint(QPainter::Antialiasing);
        m_chartRiskView->setStyleSheet("background: transparent;");
        m_chartRiskView->setContentsMargins(0, 0, 0, 0);

        riskLayout->addWidget(m_chartRiskView);

        // ====================== Zones Trend ======================
        QGroupBox *grpZonesTrend = new QGroupBox("Pressure Zones Trend", m_analyticsContentHost);
        grpZonesTrend->setStyleSheet(analyticsGroupStyle +
                                     "QGroupBox::title {"
                                     "  font-size: 16px;"
                                     "  font-weight: 700;"
                                     "  color: #F1F5F9;"
                                     "}");

        QVBoxLayout *zonesTrendLayout = new QVBoxLayout(grpZonesTrend);
        zonesTrendLayout->setContentsMargins(12, 2, 12, 8);
        zonesTrendLayout->setSpacing(1);

        QWidget *zonesLegendRow = new QWidget(grpZonesTrend);
        zonesLegendRow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        zonesLegendRow->setFixedHeight(12);
        zonesLegendRow->setStyleSheet("background: transparent;");

        QHBoxLayout *zonesLegendLayout = new QHBoxLayout(zonesLegendRow);
        zonesLegendLayout->setContentsMargins(0, 0, 0, 0);
        zonesLegendLayout->setSpacing(10);

        QLabel *zonesLegendSacrum = new QLabel("■ Sacrum", zonesLegendRow);
        zonesLegendSacrum->setStyleSheet(legendLabelStyleRed);

        QLabel *zonesLegendHeels = new QLabel("■ Heels", zonesLegendRow);
        zonesLegendHeels->setStyleSheet(legendLabelStyleGreen);

        QLabel *zonesLegendShoulders = new QLabel("■ Shoulders", zonesLegendRow);
        zonesLegendShoulders->setStyleSheet(legendLabelStyleBlue);

        zonesLegendLayout->addStretch(1);
        zonesLegendLayout->addWidget(zonesLegendSacrum);
        zonesLegendLayout->addWidget(zonesLegendHeels);
        zonesLegendLayout->addWidget(zonesLegendShoulders);
        zonesLegendLayout->addStretch(1);

        zonesTrendLayout->addWidget(zonesLegendRow);

        m_seriesSacrum = new QLineSeries();
        m_seriesHeel = new QLineSeries();
        m_seriesShoulders = new QLineSeries();

        m_seriesSacrum->setName("Sacrum");
        m_seriesHeel->setName("Heels");
        m_seriesShoulders->setName("Shoulders");

        QPen sacrumPen(QColor("#EF4444"));
        sacrumPen.setWidth(4);
        m_seriesSacrum->setPen(sacrumPen);

        QPen heelPen(QColor("#22C55E"));
        heelPen.setWidth(4);
        m_seriesHeel->setPen(heelPen);

        QPen shouldersPen(QColor("#38BDF8"));
        shouldersPen.setWidth(4);
        m_seriesShoulders->setPen(shouldersPen);

        QChart *zonesChart = new QChart();
        zonesChart->setMargins(QMargins(0, 0, 0, 12));
        zonesChart->layout()->setContentsMargins(0, 0, 0, 12);
        zonesChart->addSeries(m_seriesSacrum);
        zonesChart->addSeries(m_seriesHeel);
        zonesChart->addSeries(m_seriesShoulders);
        zonesChart->legend()->hide();
        zonesChart->setBackgroundVisible(false);
        zonesChart->setPlotAreaBackgroundVisible(true);
        zonesChart->setPlotAreaBackgroundBrush(QColor("#0B1220"));
        zonesChart->setTitle("");

        QValueAxis *zonesAxisX = new QValueAxis();
        zonesAxisX->setRange(0, 60);
        zonesAxisX->setTickCount(5);
        zonesAxisX->setLabelFormat("%d");
        zonesAxisX->setTitleText("Time");
        zonesAxisX->setTitleBrush(QBrush(QColor("#E2E8F0")));
        zonesAxisX->setGridLineColor(QColor("#2D3742"));
        zonesAxisX->setLabelsColor(QColor("#E2E8F0"));
        QFont zonesAxisXLabelsFont;
        zonesAxisXLabelsFont.setPointSize(8);
        zonesAxisX->setLabelsFont(zonesAxisXLabelsFont);
        QFont zonesAxisXTitleFont;
        zonesAxisXTitleFont.setPointSize(10);
        zonesAxisXTitleFont.setBold(true);
        zonesAxisX->setTitleFont(zonesAxisXTitleFont);

        QValueAxis *zonesAxisY = new QValueAxis();
        zonesAxisY->setRange(0, 63);
        zonesAxisY->setTickCount(5);
        zonesAxisY->setLabelFormat("%d");
        zonesAxisY->setTitleText("Pressure Level (0-63)");
        zonesAxisY->setTitleBrush(QBrush(QColor("#E2E8F0")));
        zonesAxisY->setGridLineColor(QColor("#2D3742"));
        zonesAxisY->setLabelsColor(QColor("#E2E8F0"));
        QFont zonesAxisYLabelsFont;
        zonesAxisYLabelsFont.setPointSize(10);
        zonesAxisY->setLabelsFont(zonesAxisYLabelsFont);
        QFont zonesAxisYTitleFont;
        zonesAxisYTitleFont.setPointSize(11);
        zonesAxisYTitleFont.setBold(true);
        zonesAxisY->setTitleFont(zonesAxisYTitleFont);

        zonesChart->addAxis(zonesAxisX, Qt::AlignBottom);
        zonesChart->addAxis(zonesAxisY, Qt::AlignLeft);

        m_seriesSacrum->attachAxis(zonesAxisX);
        m_seriesSacrum->attachAxis(zonesAxisY);
        m_seriesHeel->attachAxis(zonesAxisX);
        m_seriesHeel->attachAxis(zonesAxisY);
        m_seriesShoulders->attachAxis(zonesAxisX);
        m_seriesShoulders->attachAxis(zonesAxisY);

        m_chartZonesView = new QChartView(zonesChart, grpZonesTrend);
        m_chartZonesView->setMinimumHeight(240);
        m_chartZonesView->setRenderHint(QPainter::Antialiasing);
        m_chartZonesView->setStyleSheet("background: transparent;");
        m_chartZonesView->setContentsMargins(0, 0, 0, 0);

        zonesTrendLayout->addWidget(m_chartZonesView);

        // ====================== Exposure Chart ======================
        QGroupBox *grpExposure = new QGroupBox("Exposure Duration", m_analyticsContentHost);
        grpExposure->setStyleSheet(analyticsGroupStyle +
                                   "QGroupBox::title {"
                                   "  font-size: 16px;"
                                   "  font-weight: 700;"
                                   "  color: #F1F5F9;"
                                   "}");

        QVBoxLayout *exposureLayout = new QVBoxLayout(grpExposure);
        exposureLayout->setContentsMargins(12, 2, 12, 8);
        exposureLayout->setSpacing(1);

        QWidget *exposureLegendRow = new QWidget(grpExposure);
        exposureLegendRow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        exposureLegendRow->setFixedHeight(12);
        exposureLegendRow->setStyleSheet("background: transparent;");

        QHBoxLayout *exposureLegendLayout = new QHBoxLayout(exposureLegendRow);
        exposureLegendLayout->setContentsMargins(0, 0, 0, 0);
        exposureLegendLayout->setSpacing(10);

        QLabel *exposureLegendSacrum = new QLabel("■ Sacrum", exposureLegendRow);
        exposureLegendSacrum->setStyleSheet(legendLabelStyleRed);

        QLabel *exposureLegendHeels = new QLabel("■ Heels", exposureLegendRow);
        exposureLegendHeels->setStyleSheet(legendLabelStyleGreen);

        QLabel *exposureLegendShoulders = new QLabel("■ Shoulders", exposureLegendRow);
        exposureLegendShoulders->setStyleSheet(legendLabelStyleBlue);

        exposureLegendLayout->addStretch(1);
        exposureLegendLayout->addWidget(exposureLegendSacrum);
        exposureLegendLayout->addWidget(exposureLegendHeels);
        exposureLegendLayout->addWidget(exposureLegendShoulders);
        exposureLegendLayout->addStretch(1);

        exposureLayout->addWidget(exposureLegendRow);

        QBarSet *setSacrum = new QBarSet("Sacrum");
        QBarSet *setHeels = new QBarSet("Heels");
        QBarSet *setShoulders = new QBarSet("Shoulders");

        setSacrum->setColor(QColor("#EF4444"));
        setHeels->setColor(QColor("#22C55E"));
        setShoulders->setColor(QColor("#38BDF8"));

        *setSacrum << 0;
        *setHeels << 0;
        *setShoulders << 0;

        m_seriesExposure = new QBarSeries();
        m_seriesExposure->append(setSacrum);
        m_seriesExposure->append(setHeels);
        m_seriesExposure->append(setShoulders);

        QChart *exposureChart = new QChart();
        exposureChart->setMargins(QMargins(0, 0, 0, 12));
        exposureChart->layout()->setContentsMargins(0, 0, 0, 12);
        exposureChart->addSeries(m_seriesExposure);
        exposureChart->legend()->hide();
        exposureChart->setBackgroundVisible(false);
        exposureChart->setPlotAreaBackgroundVisible(true);
        exposureChart->setPlotAreaBackgroundBrush(QColor("#0B1220"));
        exposureChart->setTitle("");

        QBarCategoryAxis *exposureAxisX = new QBarCategoryAxis();
        exposureAxisX->append(QStringList() << "Exposure");
        exposureAxisX->setLabelsColor(QColor("#E2E8F0"));
        QFont exposureAxisXLabelsFont;
        exposureAxisXLabelsFont.setPointSize(8);
        exposureAxisX->setLabelsFont(exposureAxisXLabelsFont);

        QValueAxis *exposureAxisY = new QValueAxis();
        exposureAxisY->setRange(0, 300);
        exposureAxisY->setTickCount(5);
        exposureAxisY->setLabelFormat("%d");
        exposureAxisY->setTitleText("Exposure Time (min)");
        exposureAxisY->setTitleBrush(QBrush(QColor("#E2E8F0")));
        exposureAxisY->setGridLineColor(QColor("#2D3742"));
        exposureAxisY->setLabelsColor(QColor("#E2E8F0"));
        QFont exposureAxisYLabelsFont;
        exposureAxisYLabelsFont.setPointSize(10);
        exposureAxisY->setLabelsFont(exposureAxisYLabelsFont);
        QFont exposureAxisYTitleFont;
        exposureAxisYTitleFont.setPointSize(11);
        exposureAxisYTitleFont.setBold(true);
        exposureAxisY->setTitleFont(exposureAxisYTitleFont);

        exposureChart->addAxis(exposureAxisX, Qt::AlignBottom);
        exposureChart->addAxis(exposureAxisY, Qt::AlignLeft);
        m_seriesExposure->attachAxis(exposureAxisX);
        m_seriesExposure->attachAxis(exposureAxisY);

        m_chartExposureView = new QChartView(exposureChart, grpExposure);
        m_chartExposureView->setMinimumHeight(230);
        m_chartExposureView->setRenderHint(QPainter::Antialiasing);
        m_chartExposureView->setStyleSheet("background: transparent;");
        m_chartExposureView->setContentsMargins(0, 0, 0, 0);

        exposureLayout->addWidget(m_chartExposureView);

        // ====================== Add Widgets ======================
        analyticsContentLayout->addWidget(grpRiskTrend);
        analyticsContentLayout->addWidget(grpZonesTrend);
        analyticsContentLayout->addWidget(grpExposure);

        analyticsRootLayout->addWidget(m_analyticsContentHost, 0, Qt::AlignHCenter);
        analyticsRootLayout->addStretch(1);
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
    ui->tabHeatmapHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QLayout *hostLayout = ui->tabHeatmapHost->layout();

    if (!hostLayout) {
        QVBoxLayout *fallbackLayout = new QVBoxLayout(ui->tabHeatmapHost);
        fallbackLayout->setContentsMargins(0, 0, 0, 0);
        fallbackLayout->setSpacing(0);
        hostLayout = fallbackLayout;
    } else {
        while (QLayoutItem *item = hostLayout->takeAt(0)) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }

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

    m_heatmap = new HeatmapWidget(leftPanel);
    m_heatmap->setMinimumSize(720, 720);
    m_heatmap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_heatmap->setStore(&m_store);
    m_heatmap->setBedStore(&m_bedStore);
    m_heatmap->setShowDebugText(false);
    m_heatmap->setShowTooltip(true);
    m_heatmap->setShowBodyZones(false);
    m_heatmap->setShowZoneValues(false);

    m_useAdaptiveZoneStats = true;
    m_compareZoneStats = false;

    QHBoxLayout *topInfoRow = new QHBoxLayout();
    topInfoRow->setSpacing(12);

    m_lblRiskLive = new QLabel("RISK --", leftPanel);
    m_lblRiskLive->setMinimumHeight(84);
    m_lblRiskLive->setMinimumWidth(280);
    m_lblRiskLive->setAlignment(Qt::AlignCenter);
    // ====================== Risk Card Style ======================
    // رنگ نهایی این کارت بعداً داخل renderSummaryToDashboard به‌صورت dynamic override می‌شود.
    m_lblRiskLive->setStyleSheet(
        "QLabel {"
        "  background-color: #1F2933;"
        "  border: 1px solid #334155;"
        "  border-radius: 14px;"
        "  padding: 14px 18px;"
        "  color: #F8FAFC;"
        "  font-size: 18px;"
        "  font-weight: 800;"
        "}"
        );

    m_lblMovementLive = new QLabel("LAST MOVE --", leftPanel);
    m_lblMovementLive->setMinimumHeight(84);
    m_lblMovementLive->setMinimumWidth(240);
    m_lblMovementLive->setAlignment(Qt::AlignCenter);
    // ====================== Movement Card Style ======================
    m_lblMovementLive->setStyleSheet(
        "QLabel {"
        "  background-color: #17202A;"
        "  border: 1px solid #334155;"
        "  border-radius: 14px;"
        "  padding: 14px 18px;"
        "  color: #F8FAFC;"
        "  font-size: 15px;"
        "  font-weight: 650;"
        "}"
        );

    topInfoRow->addWidget(m_lblRiskLive, 0);
    topInfoRow->addWidget(m_lblMovementLive, 0);
    topInfoRow->addStretch(1);

    leftLayout->addLayout(topInfoRow);
    leftLayout->addWidget(m_heatmap, 1);

    // ================= RIGHT PANEL =================
    QWidget *rightPanel = new QWidget(heatmapPageContainer);
    rightPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    rightPanel->setMinimumWidth(240);
    rightPanel->setMaximumWidth(260);

    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

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

    // Alerts
    QGroupBox *grpAlerts = new QGroupBox("Active Alerts", rightPanel);
    grpAlerts->setStyleSheet(sideGroupStyle);
    QVBoxLayout *alertsLay = new QVBoxLayout(grpAlerts);

    m_lblAlerts = new QLabel("No active alerts", grpAlerts);
    m_lblAlerts->setAlignment(Qt::AlignCenter);
    m_lblAlerts->setMinimumHeight(60);
    m_lblAlerts->setStyleSheet(
        "QLabel {"
        "  background-color: #2C3E50;"
        "  color: #ECF0F1;"
        "  border-radius: 10px;"
        "  padding: 10px;"
        "  font-weight: 600;"
        "}"
        );
    alertsLay->addWidget(m_lblAlerts);
    rightLayout->addWidget(grpAlerts);

    // Zones
    QGroupBox *grpZones = new QGroupBox("Body Zones", rightPanel);
    grpZones->setStyleSheet(sideGroupStyle);
    QVBoxLayout *zonesLay = new QVBoxLayout(grpZones);

    m_lblSacrum = new QLabel("SACRUM\n--", grpZones);
    m_lblHeelLeft = new QLabel("LEFT HEEL\n--", grpZones);
    m_lblHeelRight = new QLabel("RIGHT HEEL\n--", grpZones);

    // ====================== Zone Card Base Style ======================
    // رنگ هر zone بعداً داخل renderSummaryToDashboard بر اساس شدت فشار override می‌شود.
    const QString zoneCardStyle =
        "QLabel {"
        "  color: #E5E7EB;"
        "  background-color: #18212B;"
        "  border: 1px solid #334155;"
        "  padding: 12px;"
        "  border-radius: 12px;"
        "  font-weight: 600;"
        "  line-height: 1.35;"
        "}";

    m_lblSacrum->setStyleSheet(zoneCardStyle);
    m_lblHeelLeft->setStyleSheet(zoneCardStyle);
    m_lblHeelRight->setStyleSheet(zoneCardStyle);

    zonesLay->addWidget(m_lblSacrum);
    zonesLay->addWidget(m_lblHeelLeft);
    zonesLay->addWidget(m_lblHeelRight);

    rightLayout->addWidget(grpZones);

    // Recommendation
    QGroupBox *grpRec = new QGroupBox("Recommendation", rightPanel);
    grpRec->setStyleSheet(sideGroupStyle);
    QVBoxLayout *recLay = new QVBoxLayout(grpRec);

    m_lblRecommendation = new QLabel("No recommendation", grpRec);
    m_lblRecommendation->setWordWrap(true);
    // ====================== Recommendation Card Body Style ======================
    m_lblRecommendation->setStyleSheet(
        "QLabel {"
        "  color: #E5E7EB;"
        "  background-color: #0F1720;"
        "  border: 1px solid #334155;"
        "  border-radius: 10px;"
        "  padding: 10px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        );
    recLay->addWidget(m_lblRecommendation);

    rightLayout->addWidget(grpRec);
    /* =========================================================
 *  Intervention Card
 *
 *  Purpose:
 *  - Display intervention plans received from Main Board
 *  - Show target zone, risk, motor count and current state
 *  - Provide Approve / Reject actions for nurse/operator
 *
 *  Protocol flow:
 *  Main -> UI : TYPE 0x51 INTERVENTION_PLAN
 *  UI   -> Main : TYPE 0x52 APPROVE or 0x53 REJECT
 *
 *  Current phase:
 *  - UI card only
 *  - Buttons exist but command wiring comes in next phase
 * ========================================================= */
    m_grpInterventionCard = new QGroupBox("Suggested Intervention", rightPanel);
    m_grpInterventionCard->setStyleSheet(sideGroupStyle);

    QVBoxLayout *interventionLay = new QVBoxLayout(m_grpInterventionCard);
    interventionLay->setContentsMargins(10, 10, 10, 10);
    interventionLay->setSpacing(8);

    // Main info labels
    m_lblInterventionPlanId = new QLabel("Plan: --", m_grpInterventionCard);
    m_lblInterventionTargetZone = new QLabel("Target: --", m_grpInterventionCard);
    m_lblInterventionRisk = new QLabel("Risk: --", m_grpInterventionCard);
    m_lblInterventionMotorCount = new QLabel("Motors: --", m_grpInterventionCard);
    m_lblInterventionStatus = new QLabel("Status: No pending plan", m_grpInterventionCard);

    const QString interventionLabelStyle =
        "QLabel {"
        "  color: #E5E7EB;"
        "  background-color: #0F1720;"
        "  border: 1px solid #334155;"
        "  border-radius: 8px;"
        "  padding: 7px 9px;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "}";

    m_lblInterventionPlanId->setStyleSheet(interventionLabelStyle);
    m_lblInterventionTargetZone->setStyleSheet(interventionLabelStyle);
    m_lblInterventionRisk->setStyleSheet(interventionLabelStyle);
    m_lblInterventionMotorCount->setStyleSheet(interventionLabelStyle);
    m_lblInterventionStatus->setStyleSheet(interventionLabelStyle);

    m_lblInterventionStatus->setMinimumHeight(36);
    m_lblInterventionStatus->setWordWrap(true);

    interventionLay->addWidget(m_lblInterventionPlanId);
    interventionLay->addWidget(m_lblInterventionTargetZone);
    interventionLay->addWidget(m_lblInterventionRisk);
    interventionLay->addWidget(m_lblInterventionMotorCount);
    interventionLay->addWidget(m_lblInterventionStatus);

    // Action buttons row
    QHBoxLayout *interventionButtonRow = new QHBoxLayout();
    interventionButtonRow->setSpacing(8);

    m_btnApproveIntervention = new QPushButton("Approve", m_grpInterventionCard);
    m_btnRejectIntervention = new QPushButton("Reject", m_grpInterventionCard);

    m_btnApproveIntervention->setEnabled(false);
    m_btnRejectIntervention->setEnabled(false);

    m_btnApproveIntervention->setStyleSheet(
        "QPushButton {"
        "  background-color: #14532D;"
        "  border: 1px solid #22C55E;"
        "  border-radius: 8px;"
        "  padding: 7px 10px;"
        "  color: #DCFCE7;"
        "  font-weight: 700;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #111827;"
        "  border: 1px solid #334155;"
        "  color: #64748B;"
        "}"
        );

    m_btnRejectIntervention->setStyleSheet(
        "QPushButton {"
        "  background-color: #7F1D1D;"
        "  border: 1px solid #EF4444;"
        "  border-radius: 8px;"
        "  padding: 7px 10px;"
        "  color: #FEE2E2;"
        "  font-weight: 700;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #111827;"
        "  border: 1px solid #334155;"
        "  color: #64748B;"
        "}"
        );

    interventionButtonRow->addWidget(m_btnApproveIntervention);
    interventionButtonRow->addWidget(m_btnRejectIntervention);

    interventionLay->addLayout(interventionButtonRow);

    // Start hidden until a valid 0x51 plan arrives
    m_grpInterventionCard->setVisible(false);

    // ======================================================
    // Intervention timeout watchdog.
    //
    // If Main Board does not answer with TYPE 0x54,
    // UI enters Timeout state.
    // ======================================================
    m_interventionTimeoutTimer = new QTimer(this);

    m_interventionTimeoutTimer->setSingleShot(true);

    connect(m_interventionTimeoutTimer,
            &QTimer::timeout,
            this,
            [this]()
            {
                qWarning() << "[INTERVENTION] Timeout waiting for Main Board result";

                setInterventionUiState(InterventionUiState::Timeout);

                m_hasPendingIntervention = false;
            });

    rightLayout->addWidget(m_grpInterventionCard);
    rightLayout->addStretch(1);

    mainLayout->addWidget(leftPanel, 4);
    mainLayout->addWidget(rightPanel, 1);

    hostLayout->addWidget(heatmapPageContainer);


    // ======================================================
    // Intervention Approve button
    //
    // User approves the currently pending intervention plan.
    //
    // UI -> Main Board:
    // TYPE = 0x52 APPROVE_INTERVENTION
    //
    // Packet:
    // AA 55 52 SEQ planL planH 12 34
    //
    // Safety:
    // - Only send if a valid pending plan exists.
    // - Disable both buttons immediately after sending.
    // - UI waits for Main Board lifecycle result 0x54.
    // ======================================================
    connect(m_btnApproveIntervention,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (!m_hasPendingIntervention) {
                    qWarning() << "[INTERVENTION] Approve ignored: no pending plan";
                    return;
                }

                qDebug() << "[INTERVENTION] Approve clicked"
                         << "plan_id =" << m_pendingInterventionPlanId;

                m_rx.sendInterventionApprove(m_pendingInterventionPlanId);

                setInterventionUiState(InterventionUiState::WaitingResult);

                // Start timeout watchdog after sending approve.
                // If Main Board does not send 0x54 result, UI enters Timeout.
                if (m_interventionTimeoutTimer)
                    m_interventionTimeoutTimer->start(8000);

                if (m_lblDbgLastTx)
                    m_lblDbgLastTx->setText(
                        QString("Last TX: APPROVE plan_id=%1")
                            .arg(m_pendingInterventionPlanId));
            });


    // ======================================================
    // Intervention Reject button
    //
    // User rejects the currently pending intervention plan.
    //
    // UI -> Main Board:
    // TYPE = 0x53 REJECT_INTERVENTION
    //
    // Packet:
    // AA 55 53 SEQ planL planH 12 34
    //
    // Safety:
    // - Only send if a valid pending plan exists.
    // - Disable both buttons immediately after sending.
    // - UI waits for Main Board result 0x54 REJECTED.
    // ======================================================
    connect(m_btnRejectIntervention,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (!m_hasPendingIntervention) {
                    qWarning() << "[INTERVENTION] Reject ignored: no pending plan";
                    return;
                }

                qDebug() << "[INTERVENTION] Reject clicked"
                         << "plan_id =" << m_pendingInterventionPlanId;

                m_rx.sendInterventionReject(m_pendingInterventionPlanId);

                setInterventionUiState(InterventionUiState::WaitingResult);
                // Start timeout watchdog after sending reject.
                // If Main Board does not send 0x54 result, UI enters Timeout.
                if (m_interventionTimeoutTimer)
                    m_interventionTimeoutTimer->start(8000);

                if (m_lblDbgLastTx)
                    m_lblDbgLastTx->setText(
                        QString("Last TX: REJECT plan_id=%1")
                            .arg(m_pendingInterventionPlanId));
            });


    // ====================== Alert Pulse Effect ======================
    // این افکت باعث می‌شود alert critical به‌صورت نرم کم‌نور/پرنور شود.
    m_alertOpacityEffect = new QGraphicsOpacityEffect(this);
    m_alertOpacityEffect->setOpacity(1.0);

    if (m_lblAlerts)
        m_lblAlerts->setGraphicsEffect(m_alertOpacityEffect);

    m_alertPulseAnim = new QPropertyAnimation(m_alertOpacityEffect, "opacity", this);
    m_alertPulseAnim->setDuration(1800);   // فرکانس کمتر و نرم‌تر
    m_alertPulseAnim->setStartValue(1.0);
    m_alertPulseAnim->setEndValue(0.45);
    m_alertPulseAnim->setEasingCurve(QEasingCurve::InOutSine);
    m_alertPulseAnim->setLoopCount(-1);    // بی‌نهایت
    // ====================== UI Shadows ======================

    // 🔹 Risk Card Shadow
    {
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(25);
        shadow->setOffset(0, 6);
        shadow->setColor(QColor(0, 0, 0, 120));
        m_lblRiskLive->setGraphicsEffect(shadow);
    }

    // 🔹 Movement Card Shadow
    {
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(20);
        shadow->setOffset(0, 5);
        shadow->setColor(QColor(0, 0, 0, 110));
        m_lblMovementLive->setGraphicsEffect(shadow);
    }

    // 🔹 Zone Cards Shadow
    auto applyShadow = [this](QWidget *w, int blur, int offsetY)
    {
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(blur);
        shadow->setOffset(0, offsetY);
        shadow->setColor(QColor(0, 0, 0, 100));
        w->setGraphicsEffect(shadow);
    };

    applyShadow(m_lblSacrum, 15, 4);
    applyShadow(m_lblHeelLeft, 15, 4);
    applyShadow(m_lblHeelRight, 15, 4);

    // 🔹 Recommendation Shadow
    applyShadow(m_lblRecommendation, 18, 5);

    /* =========================================================
     *  5) Core Connections
     * ========================================================= */
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

    // ======================================================
    // Analytics tab activation.
    //
    // Data is collected continuously in m_trendHistory.
    // Charts are rebuilt only when Analytics tab becomes visible.
    // ======================================================
    connect(ui->tabs, &QTabWidget::currentChanged,
            this,
            [this](int)
            {
                if (ui->tabs->currentWidget() != m_tabAnalytics)
                    return;

                rebuildAnalyticsChartsFromHistory();
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
            this, &MainWindow::onSummaryReceived);

    connect(&m_rx, &SerialReceiver::bedSnapshotReceived,
            this, &MainWindow::onBedSnapshot);

    connect(&m_rx, &SerialReceiver::bedStatusReceived,
            this, &MainWindow::onBedStatus);

    connect(&m_rx, &SerialReceiver::nodeHealthReceived,
            this, &MainWindow::onNodeHealth);
    // ======================================================
    // Main Board -> UI
    // Intervention plan received.
    //
    // TYPE = 0x51
    //
    // This signal is emitted by SerialReceiver after
    // a valid INTERVENTION_PLAN packet is parsed.
    // ======================================================
    connect(&m_rx,
            &SerialReceiver::interventionPlanReceived,
            this,
            &MainWindow::onInterventionPlanReceived);
    // ======================================================
    // Main Board -> UI
    // Intervention lifecycle/result received.
    //
    // TYPE = 0x54
    //
    // Updates card status:
    // EXECUTING / COMPLETED / FAILED / REJECTED
    // ======================================================
    connect(&m_rx,
            &SerialReceiver::interventionResultReceived,
            this,
            &MainWindow::onInterventionResultReceived);
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
            m_heatmap, &HeatmapWidget::onBedFrameUpdated);

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

                if (m_hasValidSummary)
                    renderSummaryToDashboard(m_lastSummary);
            });

    connect(m_radZoneStatsAdaptive, &QRadioButton::toggled,
            this, [this](bool checked){
                if (!checked)
                    return;

                m_useAdaptiveZoneStats = true;
                m_compareZoneStats = false;

                if (m_hasValidSummary)
                    renderSummaryToDashboard(m_lastSummary);
            });

    connect(m_radZoneStatsCompare, &QRadioButton::toggled,
            this, [this](bool checked){
                if (!checked)
                    return;

                m_useAdaptiveZoneStats = false;
                m_compareZoneStats = true;

                if (m_hasValidSummary)
                    renderSummaryToDashboard(m_lastSummary);
            });

}

/*========================================================================================*/

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
        m_cmbPortSettings->addItem(port.portName());
}

/*========================================================================================*/

void MainWindow::setConnectedUi(bool connected)
{
    if (m_btnConnectSettings)
        m_btnConnectSettings->setText(connected ? "Disconnect" : "Connect");

    if (m_lblStatusSettings) {
        if (connected) {
            m_lblStatusSettings->setText("● Connected");
            m_lblStatusSettings->setStyleSheet(
                "QLabel {"
                "  background-color: rgba(34,197,94,0.15);"
                "  color: #22C55E;"
                "  border-radius: 10px;"
                "  padding: 4px 10px;"
                "  font-weight: 600;"
                "}"
                );
        } else {
            m_lblStatusSettings->setText("● Disconnected");
            m_lblStatusSettings->setStyleSheet(
                "QLabel {"
                "  background-color: rgba(239,68,68,0.15);"
                "  color: #EF4444;"
                "  border-radius: 10px;"
                "  padding: 4px 10px;"
                "  font-weight: 600;"
                "}"
                );
        }
    }

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

    // ======================================================
    // Reset live UI state after serial connection.
    //
    // Reason:
    // At startup/reconnect the first UART packets may be partial.
    // Clearing UI state avoids showing stale or half-valid heatmap
    // until fresh BED_SNAPSHOT / STATUS / SUMMARY arrive.
    // ======================================================
    m_hasValidSummary = false;
    m_frameSync = FrameSyncState();

    if (m_lblFrameSync)
        m_lblFrameSync->setText("Frame: waiting...");

    if (m_lblRiskLive)
        m_lblRiskLive->setText("RISK --");

    if (m_lblMovementLive)
        m_lblMovementLive->setText("LAST MOVE --");

    if (m_lblSacrum)
        m_lblSacrum->setText("SACRUM\n--");

    if (m_lblHeelLeft)
        m_lblHeelLeft->setText("LEFT HEEL\n--");

    if (m_lblHeelRight)
        m_lblHeelRight->setText("RIGHT HEEL\n--");

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

    if (m_lblStatusSettings) {
        m_lblStatusSettings->setText(
            QString("RX node=%1 cycle=%2 seq=%3 state=%4")
                .arg(pkt.nodeId)
                .arg(pkt.cycle)
                .arg(pkt.seq)
                .arg(nodeStateText(ns))
            );
    }
}

/*========================================================================================*/

void MainWindow::onBedSnapshot(const BedSnapshotPacket &pkt)
{
    m_bedStore.setSnapshot(pkt.frameId, pkt.values.data(), int(pkt.values.size()));

    m_frameSync.snapshotFrameId = pkt.frameId;
    m_frameSync.hasSnapshot = true;
}

/*========================================================================================*/

void MainWindow::onBedStatus(const BedStatusPacket &pkt)
{
    m_bedStore.setStatus(pkt.frameId, pkt.status.data(), int(pkt.status.size()));

    m_frameSync.statusFrameId = pkt.frameId;
    m_frameSync.hasStatus = true;
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

            if (valid) {
                it->setData(Qt::DisplayRole, QString::number(int(value)));
            } else {
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

    Q_UNUSED(color);

    QString stateColor;
    switch (st)
    {
    case NodeState::Online:
        stateColor = "#3CB44B";
        break;
    case NodeState::Stale:
        stateColor = "#FFC800";
        break;
    case NodeState::Offline:
    default:
        stateColor = "#AAAAAA";
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
            bgColor = QColor(30, 45, 30);
        else if (st == NodeState::Stale)
            bgColor = QColor(45, 40, 25);

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

        Q_UNUSED(borderColor);

        int borderWidth = 1;

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

        if (obj == m_cardHeatmap) {
            ui->tabs->setCurrentWidget(ui->Heatmap);
            return true;
        }
        else if (obj == m_cardData) {
            ui->tabs->setCurrentWidget(ui->Data);
            return true;
        }
        else if (obj == m_cardSettings) {
            ui->tabs->setCurrentWidget(ui->tabSettings);
            return true;
        }

        for (int i = 0; i < m_nodeCards.size(); ++i)
        {
            if (obj == m_nodeCards[i]) {
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
    renderDebugDataPanel();

    if (!m_hasValidSummary)
        return;

    if (m_riskTrendSeries && m_riskTrendChartView)
        updateMiniRiskTrendChart();
}

/*========================================================================================*/

void MainWindow::onSummaryReceived(const SummaryData &summary)
{

    m_lastSummary = summary;
    m_hasValidSummary = true;

    m_trendHistory.appendSummary(summary);
    m_lastSummaryRxMs = QDateTime::currentMSecsSinceEpoch();

    m_frameSync.summaryFrameId = summary.frameId;
    m_frameSync.hasSummary = true;

    renderDebugDataPanel();

    if (m_riskTrendSeries && m_riskTrendChartView)
        updateMiniRiskTrendChart();

    // ====================== Analytics Charts Update ======================
    // Performance:
    // Keep collecting history always,
    // but repaint/update charts only when Analytics tab is visible.
  /*  if (ui->tabs->currentWidget() == m_tabAnalytics)
    {
    static int x = 0;

    // --------------------------------------------------------
    // Risk Trend
    // --------------------------------------------------------
    if (m_seriesRisk) {
        m_seriesRisk->append(x, summary.riskScore);

        if (m_seriesRisk->count() > 120)
            m_seriesRisk->removePoints(0, 1);
    }

    // --------------------------------------------------------
    // Pressure Zones Trend
    // --------------------------------------------------------
    auto toPressure = [](int raw) -> int {
        return 63 - raw;
    };

    const int sacrumP = toPressure(summary.sacrumAvg);
    const int heelP = toPressure(summary.heelLeftAvg);
    const int shouldersP = toPressure(summary.shouldersAvg);

    if (m_seriesSacrum && m_seriesHeel && m_seriesShoulders) {
        m_seriesSacrum->append(x, sacrumP);
        m_seriesHeel->append(x, heelP);
        m_seriesShoulders->append(x, shouldersP);

        if (m_seriesSacrum->count() > 120) {
            m_seriesSacrum->removePoints(0, 1);
            m_seriesHeel->removePoints(0, 1);
            m_seriesShoulders->removePoints(0, 1);
        }
    }

    // --------------------------------------------------------
    // Exposure Duration
    // IMPORTANT: 3 separate QBarSet objects, each one has only 1 value
    // sets[0] = Sacrum
    // sets[1] = Heels
    // sets[2] = Shoulders
    // --------------------------------------------------------
    if (m_seriesExposure) {
        const auto sets = m_seriesExposure->barSets();

        if (sets.size() >= 3) {
            sets[0]->replace(0, summary.sacrumExposureS / 60.0);
            sets[1]->replace(0, summary.heelsExposureS / 60.0);
            sets[2]->replace(0, summary.shouldersExposureS / 60.0);
        }
    }

    // --------------------------------------------------------
    // Keep visible x-range sliding
    // --------------------------------------------------------
    if (m_chartRiskView && m_chartRiskView->chart()) {
        const auto axes = m_chartRiskView->chart()->axes(Qt::Horizontal);
        if (!axes.isEmpty()) {
            if (auto *axis = qobject_cast<QValueAxis*>(axes.first())) {
                const int minX = (x > 60) ? (x - 60) : 0;
                const int maxX = (x > 60) ? x : 60;
                axis->setRange(minX, maxX);

                if (m_seriesRiskThreshold) {
                    m_seriesRiskThreshold->clear();
                    m_seriesRiskThreshold->append(minX, 70);
                    m_seriesRiskThreshold->append(maxX, 70);
                }
            }
        }
    }

    if (m_chartZonesView && m_chartZonesView->chart()) {
        const auto axes = m_chartZonesView->chart()->axes(Qt::Horizontal);
        if (!axes.isEmpty()) {
            if (auto *axis = qobject_cast<QValueAxis*>(axes.first())) {
                const int minX = (x > 60) ? (x - 60) : 0;
                const int maxX = (x > 60) ? x : 60;
                axis->setRange(minX, maxX);
            }
        }
    }

    x++;
}*/

    /*qDebug() << "[SUMMARY]"
             << "uptime=" << summary.uptimeS
             << "risk=" << summary.riskScore
             << "sacrumExp=" << summary.sacrumExposureS
             << "heelsExp=" << summary.heelsExposureS
             << "threshold=" << summary.pressureExposureThreshold;

    qDebug() << "[TrendHistory] size =" << m_trendHistory.size();*/

}


/*========================================================================================*/

// ======================================================
// Rebuild Analytics charts from stored TrendHistory.
//
// Data collection is continuous even when Analytics tab
// is not visible. This function redraws charts when user
// opens Analytics tab.
// ======================================================
void MainWindow::rebuildAnalyticsChartsFromHistory()
{
    if (!m_seriesRisk || !m_seriesSacrum || !m_seriesHeel ||
        !m_seriesShoulders || !m_seriesExposure) {
        return;
    }

    m_seriesRisk->clear();
    m_seriesSacrum->clear();
    m_seriesHeel->clear();
    m_seriesShoulders->clear();

    const auto &samples = m_trendHistory.samples();

    static int analyticsX = 0;
    analyticsX = 0;

    int x = 0;
    for (const auto &s : samples) {
        m_seriesRisk->append(x, s.riskScore);

        m_seriesSacrum->append(x, 63 - s.sacrumAvg);
        m_seriesHeel->append(x, 63 - s.heelLeftAvg);
        m_seriesShoulders->append(x, 63 - s.shouldersAvg);

        ++x;
    }

    if (!samples.isEmpty()) {
        const auto &last = samples.last();

        const auto sets = m_seriesExposure->barSets();
        if (sets.size() >= 3) {
            sets[0]->replace(0, last.sacrumExposureS / 60.0);
            sets[1]->replace(0, last.heelsExposureS / 60.0);
            sets[2]->replace(0, last.shouldersExposureS / 60.0);
        }
    }

    const int maxX = qMax(60, x);
    const int minX = (x > 60) ? (x - 60) : 0;

    if (m_chartRiskView && m_chartRiskView->chart()) {
        const auto axes = m_chartRiskView->chart()->axes(Qt::Horizontal);
        if (!axes.isEmpty()) {
            if (auto axis = qobject_cast<QValueAxis*>(axes.first()))
                axis->setRange(minX, maxX);
        }

        if (m_seriesRiskThreshold) {
            m_seriesRiskThreshold->clear();
            m_seriesRiskThreshold->append(minX, 70);
            m_seriesRiskThreshold->append(maxX, 70);
        }
    }

    if (m_chartZonesView && m_chartZonesView->chart()) {
        const auto axes = m_chartZonesView->chart()->axes(Qt::Horizontal);
        if (!axes.isEmpty()) {
            if (auto axis = qobject_cast<QValueAxis*>(axes.first()))
                axis->setRange(minX, maxX);
        }
    }

    #if TALMA_DEBUG_ANALYTICS
        qDebug() << "[ANALYTICS] charts rebuilt from history, samples =" << samples.size();
    #endif
}
/*========================================================================================*/

// ====================== Dashboard Rendering (Polished) ======================
void MainWindow::renderSummaryToDashboard(const SummaryData &summary)
{
    // --------------------------------------------------------
    // 1) Risk Level Text
    // --------------------------------------------------------
    QString riskLevelText;
    switch (summary.riskLevel)
    {
    case 0: riskLevelText = "LOW"; break;
    case 1: riskLevelText = "MODERATE"; break;
    case 2: riskLevelText = "HIGH"; break;
    case 3: riskLevelText = "CRITICAL"; break;
    default: riskLevelText = "UNKNOWN"; break;
    }

    // --------------------------------------------------------
    // 2) Risk Trend Estimation
    // --------------------------------------------------------
    QString trendText = "STABLE";
    if (summary.riskScore >= 80)
        trendText = "CRITICAL";
    else if (summary.riskScore >= 60)
        trendText = "RISING";
    else if (summary.riskScore >= 30)
        trendText = "WATCH";

    // --------------------------------------------------------
    // 3) Movement Text
    // --------------------------------------------------------
    QString movementText;
    if (summary.timeSinceLastMovementS == 0xFFFF) {
        movementText = "LAST MOVE\nNo movement yet";
    }
    else if (summary.timeSinceLastMovementS < 60) {
        movementText = QString("LAST MOVE\n%1 sec ago")
        .arg(summary.timeSinceLastMovementS);
    }
    else {
        const int minutes = int(summary.timeSinceLastMovementS) / 60;
        movementText = QString("LAST MOVE\n%1 min ago")
                           .arg(minutes);
    }

    // --------------------------------------------------------
    // 4) Frame Sync Badge
    // --------------------------------------------------------
    if (m_lblFrameSync) {
        const bool synced = isCurrentFrameSynchronized();
        const quint16 frameId = m_frameSync.hasSummary ? m_frameSync.summaryFrameId : 0;

        m_lblFrameSync->setText(
            QString("Frame %1  |  %2")
                .arg(frameId)
                .arg(synced ? "SYNCED" : "WAITING / PARTIAL")
            );

        m_lblFrameSync->setStyleSheet(QString(
                                          "QLabel {"
                                          "  color: %1;"
                                          "  background-color: %2;"
                                          "  border: 1px solid %3;"
                                          "  border-radius: 6px;"
                                          "  padding: 3px 8px;"
                                          "  font-size: 13px;"
                                          "  font-weight: 600;"
                                          "}"
                                          )
                                          .arg(synced ? "#C8FACC" : "#FDE68A")
                                          .arg(synced ? "#102418" : "#2B2112")
                                          .arg(synced ? "#1F7A3D" : "#8A6A17"));
    }

    // --------------------------------------------------------
    // 5) Convert Raw -> Pressure-like
    // --------------------------------------------------------
    auto toPressure = [](int raw) -> int {
        return 63 - raw;
    };

    const int sacrumAvg = toPressure(summary.sacrumAvg);
    const int sacrumPeak = toPressure(summary.sacrumPeak);
    const int heelLeftAvg = toPressure(summary.heelLeftAvg);
    const int heelRightAvg = toPressure(summary.heelRightAvg);
    const int heelLeftPeak = heelLeftAvg;
    const int heelRightPeak = heelRightAvg;

    const int sacrumExpMin = int(summary.sacrumExposureS) / 60;
    const int heelExpMin = int(summary.heelsExposureS) / 60;
    const int noMoveMin = (summary.timeSinceLastMovementS == 0xFFFF)
                              ? -1
                              : int(summary.timeSinceLastMovementS) / 60;


    // --------------------------------------------------------
    // 5.1) Clinical Summary Panel (Analytics) - Compact Version
    // --------------------------------------------------------

    // ---------- Last Movement ----------
    QString lastMoveText;
    if (summary.timeSinceLastMovementS == 0xFFFF) {
        lastMoveText = "Move: none";
    }
    else if (summary.timeSinceLastMovementS < 60) {
        lastMoveText = QString("Move: %1s").arg(summary.timeSinceLastMovementS);
    }
    else {
        lastMoveText = QString("Move: %1m").arg(int(summary.timeSinceLastMovementS) / 60);
    }

    // ---------- Highest-Risk Zone ----------
    QString topZoneName = "Sacrum";
    int topZonePressure = sacrumAvg;

    if (heelLeftAvg > topZonePressure) {
        topZoneName = "Left Heel";
        topZonePressure = heelLeftAvg;
    }
    if (heelRightAvg > topZonePressure) {
        topZoneName = "Right Heel";
        topZonePressure = heelRightAvg;
    }

    const int shouldersAvgP = toPressure(summary.shouldersAvg);
    if (shouldersAvgP > topZonePressure) {
        topZoneName = "Shoulders";
        topZonePressure = shouldersAvgP;
    }

    QString zoneSeverity = "LOW";
    if (topZonePressure >= 50)
        zoneSeverity = "HIGH";
    else if (topZonePressure >= 30)
        zoneSeverity = "MOD";

    const QString topZoneText =
        QString("Top: %1 (%2)")
            .arg(topZoneName)
            .arg(zoneSeverity);

    // ---------- Max Exposure ----------
    QString maxExposureZone = "Sacrum";
    int maxExposureMin = sacrumExpMin;

    if (heelExpMin > maxExposureMin) {
        maxExposureZone = "Heels";
        maxExposureMin = heelExpMin;
    }

    const int shouldersExpMin = int(summary.shouldersExposureS) / 60;
    if (shouldersExpMin > maxExposureMin) {
        maxExposureZone = "Shoulders";
        maxExposureMin = shouldersExpMin;
    }

    const QString maxExposureText =
        QString("Exp: %1m (%2)")
            .arg(maxExposureMin)
            .arg(maxExposureZone);

    // ---------- Recommended Action ----------
    QString actionShort = "Monitor";

    if (summary.recommendationCode == 1 || sacrumExpMin >= 10) {
        actionShort = "Reposition";
    }
    else if (summary.recommendationCode == 2) {
        actionShort = "Check support";
    }
    else if (summary.recommendationCode == 3 || noMoveMin >= 8) {
        actionShort = "Encourage move";
    }

    const QString actionText = QString("Action: %1").arg(actionShort);

    // ---------- Push text to UI ----------
    if (m_lblClinicalLastMove)
        m_lblClinicalLastMove->setText(lastMoveText);

    if (m_lblClinicalTopZone)
        m_lblClinicalTopZone->setText(topZoneText);

    if (m_lblClinicalMaxExposure)
        m_lblClinicalMaxExposure->setText(maxExposureText);

    if (m_lblClinicalAction)
        m_lblClinicalAction->setText(actionText);

    // ---------- Compact styling ----------
    if (m_lblClinicalLastMove) {
        QString style =
            "QLabel {"
            "  background-color: #111827;"
            "  border: 1px solid #334155;"
            "  border-radius: 10px;"
            "  padding: 6px 10px;"
            "  color: #F8FAFC;"
            "  font-size: 13px;"
            "  font-weight: 600;"
            "}";

        if (noMoveMin >= 10) {
            style =
                "QLabel {"
                "  background-color: #321717;"
                "  border: 1px solid #C0392B;"
                "  border-radius: 10px;"
                "  padding: 6px 10px;"
                "  color: #FECACA;"
                "  font-size: 13px;"
                "  font-weight: 700;"
                "}";
        }
        else if (noMoveMin >= 5) {
            style =
                "QLabel {"
                "  background-color: #2C2413;"
                "  border: 1px solid #B98900;"
                "  border-radius: 10px;"
                "  padding: 6px 10px;"
                "  color: #FDE68A;"
                "  font-size: 13px;"
                "  font-weight: 700;"
                "}";
        }

        m_lblClinicalLastMove->setStyleSheet(style);
    }

    if (m_lblClinicalTopZone) {
        QString style =
            "QLabel {"
            "  background-color: #10261A;"
            "  border: 1px solid #1F7A3D;"
            "  border-radius: 10px;"
            "  padding: 6px 10px;"
            "  color: #D1FADF;"
            "  font-size: 13px;"
            "  font-weight: 700;"
            "}";

        if (zoneSeverity == "HIGH") {
            style =
                "QLabel {"
                "  background-color: #321717;"
                "  border: 1px solid #C0392B;"
                "  border-radius: 10px;"
                "  padding: 6px 10px;"
                "  color: #FECACA;"
                "  font-size: 13px;"
                "  font-weight: 700;"
                "}";
        }
        else if (zoneSeverity == "MOD") {
            style =
                "QLabel {"
                "  background-color: #2C2413;"
                "  border: 1px solid #B98900;"
                "  border-radius: 10px;"
                "  padding: 6px 10px;"
                "  color: #FDE68A;"
                "  font-size: 13px;"
                "  font-weight: 700;"
                "}";
        }

        m_lblClinicalTopZone->setStyleSheet(style);
    }

    if (m_lblClinicalMaxExposure) {
        QString style =
            "QLabel {"
            "  background-color: #111827;"
            "  border: 1px solid #334155;"
            "  border-radius: 10px;"
            "  padding: 6px 10px;"
            "  color: #F8FAFC;"
            "  font-size: 13px;"
            "  font-weight: 600;"
            "}";

        if (maxExposureMin >= 180) {
            style =
                "QLabel {"
                "  background-color: #321717;"
                "  border: 1px solid #C0392B;"
                "  border-radius: 10px;"
                "  padding: 6px 10px;"
                "  color: #FECACA;"
                "  font-size: 13px;"
                "  font-weight: 700;"
                "}";
        }
        else if (maxExposureMin >= 60) {
            style =
                "QLabel {"
                "  background-color: #2C2413;"
                "  border: 1px solid #B98900;"
                "  border-radius: 10px;"
                "  padding: 6px 10px;"
                "  color: #FDE68A;"
                "  font-size: 13px;"
                "  font-weight: 700;"
                "}";
        }

        m_lblClinicalMaxExposure->setStyleSheet(style);
    }

    if (m_lblClinicalAction) {
        QString style =
            "QLabel {"
            "  background-color: #0F1720;"
            "  border: 1px solid #3B82F6;"
            "  border-radius: 10px;"
            "  padding: 6px 10px;"
            "  color: #DBEAFE;"
            "  font-size: 13px;"
            "  font-weight: 700;"
            "}";

        if (actionShort == "Reposition") {
            style =
                "QLabel {"
                "  background-color: #1E293B;"
                "  border: 1px solid #60A5FA;"
                "  border-radius: 10px;"
                "  padding: 6px 10px;"
                "  color: #E0F2FE;"
                "  font-size: 13px;"
                "  font-weight: 700;"
                "}";
        }

        m_lblClinicalAction->setStyleSheet(style);
    }

    // --------------------------------------------------------
    // 6) Risk Card Dynamic Style
    // --------------------------------------------------------
    QString riskBg = "#1F2933";
    QString riskBorder = "#334155";
    QString riskTextColor = "#F8FAFC";

    if (summary.riskLevel == 0) {
        riskBg = "#10261A";
        riskBorder = "#1F7A3D";
        riskTextColor = "#D1FADF";
    }
    else if (summary.riskLevel == 1) {
        riskBg = "#2C2413";
        riskBorder = "#B98900";
        riskTextColor = "#FDE68A";
    }
    else if (summary.riskLevel == 2) {
        riskBg = "#321717";
        riskBorder = "#C0392B";
        riskTextColor = "#FECACA";
    }

    m_lblRiskLive->setText(
        QString("RISK  %1\n%2  •  %3")
            .arg(summary.riskScore)
            .arg(riskLevelText)
            .arg(trendText)
        );

    m_lblRiskLive->setStyleSheet(QString(
                                     "QLabel {"
                                     "  background-color: %1;"
                                     "  border: 1px solid %2;"
                                     "  border-radius: 14px;"
                                     "  padding: 14px 18px;"
                                     "  color: %3;"
                                     "  font-size: 18px;"
                                     "  font-weight: 800;"
                                     "}"
                                     ).arg(riskBg, riskBorder, riskTextColor));

    // --------------------------------------------------------
    // 7) Movement Card Update
    // --------------------------------------------------------
    m_lblMovementLive->setText(movementText);

    QString moveBg = "#17202A";
    QString moveBorder = "#334155";
    QString moveTextColor = "#F8FAFC";

    if (noMoveMin >= 10) {
        moveBg = "#321717";
        moveBorder = "#C0392B";
        moveTextColor = "#FECACA";
    }
    else if (noMoveMin >= 5) {
        moveBg = "#2C2413";
        moveBorder = "#B98900";
        moveTextColor = "#FDE68A";
    }

    m_lblMovementLive->setStyleSheet(QString(
                                         "QLabel {"
                                         "  background-color: %1;"
                                         "  border: 1px solid %2;"
                                         "  border-radius: 14px;"
                                         "  padding: 14px 18px;"
                                         "  color: %3;"
                                         "  font-size: 15px;"
                                         "  font-weight: 650;"
                                         "}"
                                         ).arg(moveBg, moveBorder, moveTextColor));

    // --------------------------------------------------------
    // 8) Zone Helpers
    // --------------------------------------------------------
    auto zoneRiskTag = [](int pressure) -> QString {
        if (pressure >= 50) return "HIGH";
        if (pressure >= 30) return "MED";
        return "LOW";
    };

    auto zoneColors = [](int pressure) -> QPair<QString, QString> {
        if (pressure >= 50)
            return qMakePair(QString("#341717"), QString("#C0392B"));
        if (pressure >= 30)
            return qMakePair(QString("#2C2413"), QString("#B98900"));
        return qMakePair(QString("#10261A"), QString("#1F7A3D"));
    };

    // --------------------------------------------------------
    // 9) Zone Cards Source Mode
    // --------------------------------------------------------
    if (m_compareZoneStats) {
        renderComparedZoneStats();
    }
    else if (m_useAdaptiveZoneStats) {
        renderAdaptiveZoneStats();
    }
    else {
        const QString sacrumTag = zoneRiskTag(sacrumAvg);
        const QString heelLeftTag = zoneRiskTag(heelLeftAvg);
        const QString heelRightTag = zoneRiskTag(heelRightAvg);

        m_lblSacrum->setText(
            QString("SACRUM   %1\nAvg %2 | Peak %3\nExposure %4 min")
                .arg(sacrumTag)
                .arg(sacrumAvg)
                .arg(sacrumPeak)
                .arg(sacrumExpMin)
            );

        m_lblHeelLeft->setText(
            QString("LEFT HEEL   %1\nAvg %2 | Peak %3\nExposure %4 min")
                .arg(heelLeftTag)
                .arg(heelLeftAvg)
                .arg(heelLeftPeak)
                .arg(heelExpMin)
            );

        m_lblHeelRight->setText(
            QString("RIGHT HEEL   %1\nAvg %2 | Peak %3\nExposure %4 min")
                .arg(heelRightTag)
                .arg(heelRightAvg)
                .arg(heelRightPeak)
                .arg(heelExpMin)
            );

        const auto sacrumStyle = zoneColors(sacrumAvg);
        const auto heelLeftStyle = zoneColors(heelLeftAvg);
        const auto heelRightStyle = zoneColors(heelRightAvg);

        m_lblSacrum->setStyleSheet(QString(
                                       "QLabel {"
                                       "  color: #F8FAFC;"
                                       "  background-color: %1;"
                                       "  border: 1px solid %2;"
                                       "  padding: 12px;"
                                       "  border-radius: 12px;"
                                       "  font-weight: 700;"
                                       "  line-height: 1.35;"
                                       "}"
                                       ).arg(sacrumStyle.first, sacrumStyle.second));

        m_lblHeelLeft->setStyleSheet(QString(
                                         "QLabel {"
                                         "  color: #F8FAFC;"
                                         "  background-color: %1;"
                                         "  border: 1px solid %2;"
                                         "  padding: 12px;"
                                         "  border-radius: 12px;"
                                         "  font-weight: 700;"
                                         "  line-height: 1.35;"
                                         "}"
                                         ).arg(heelLeftStyle.first, heelLeftStyle.second));

        m_lblHeelRight->setStyleSheet(QString(
                                          "QLabel {"
                                          "  color: #F8FAFC;"
                                          "  background-color: %1;"
                                          "  border: 1px solid %2;"
                                          "  padding: 12px;"
                                          "  border-radius: 12px;"
                                          "  font-weight: 700;"
                                          "  line-height: 1.35;"
                                          "}"
                                          ).arg(heelRightStyle.first, heelRightStyle.second));
    }

    // --------------------------------------------------------
    // 10) Alert System
    // --------------------------------------------------------
    QString alertText = "No active alerts";
    QString alertColor = "#7F8C8D";


    // 🔴 Rule 1: High sacrum exposure
    if (sacrumExpMin >= 10)
    {
        alertText = QString("[CRITICAL] SACRUM OVERLOAD\n%1 min exposure")
        .arg(sacrumExpMin);
        alertColor = "#E74C3C";
    }
    else if (noMoveMin >= 8) {
        alertText = QString("No movement\n%1 min").arg(noMoveMin);
        alertColor = "#F39C12";
    }
    else if (summary.riskScore >= 50) {
        alertText = "Elevated risk";
        alertColor = "#D4AC0D";
    }

    if (summary.alertActive) {
        if (summary.alertSeverity >= 2) {
            alertText = "HIGH RISK ALERT (device)";
            alertColor = "#E74C3C";
        }
        else if (summary.alertSeverity == 1) {
            alertText = "Warning (device)";
            alertColor = "#F39C12";
        }
    }


    // --------------------------------------------------------
    // Apply Alert Text + Visual Style + Blink
    // --------------------------------------------------------

    // ======================================================
    // TEMP ALERT LOGIC
    //
    // Current Main Board test stream does not yet provide
    // realistic alertActive/riskLevel escalation.
    // Use riskScore directly for UI alert testing.
    // ======================================================
    if (summary.riskScore >= 40) {
        alertText = QString("HIGH PRESSURE RISK (%1)")
        .arg(summary.riskScore);

        alertColor = "#F39C12";   // Orange warning
    }

    m_lblAlerts->setText(alertText);

    /*qDebug() << "[ALERT CHECK]"
             << "alertText =" << alertText
             << "risk =" << summary.riskScore
             << "riskLevel =" << summary.riskLevel
             << "sacrumExpMin =" << sacrumExpMin
             << "noMoveMin =" << noMoveMin
             << "deviceAlert =" << summary.alertActive
             << "severity =" << summary.alertSeverity;*/

    QString alertStyle;

    const bool isCriticalAlert =
        (summary.alertSeverity >= 2) ||
        (summary.riskLevel == 2) ||
        (sacrumExpMin >= 10);

    if (isCriticalAlert)
    {
        // 🔴 HIGH ALERT
        alertStyle = QString(
                         "QLabel {"
                         "  background-color: %1;"
                         "  color: white;"
                         "  border-radius: 12px;"
                         "  padding: 12px;"
                         "  font-weight: 800;"
                         "  border: 2px solid rgba(255,255,255,0.9);"
                         "}"
                         ).arg(alertColor);
    }
    else
    {
        // 🟠/🟡 NORMAL ALERT
        alertStyle = QString(
                         "QLabel {"
                         "  background-color: %1;"
                         "  color: white;"
                         "  border-radius: 12px;"
                         "  padding: 10px;"
                         "  font-weight: 600;"
                         "  border: 1px solid rgba(255,255,255,0.15);"
                         "}"
                         ).arg(alertColor);
    }

    // استایل اصلی را ذخیره کن تا blink از آن استفاده کند
    // استایل اصلی alert را اعمال کن
    m_currentAlertStyle = alertStyle;
    m_lblAlerts->setStyleSheet(m_currentAlertStyle);

    // ====================== Alert Pulse Control ======================
    // به‌جای blink خام، برای alertهای critical یک pulse نرم اعمال می‌کنیم.
    if (isCriticalAlert)
    {
        if (m_alertPulseAnim && m_alertPulseAnim->state() != QAbstractAnimation::Running)
            m_alertPulseAnim->start();
    }
    else
    {
        if (m_alertPulseAnim && m_alertPulseAnim->state() == QAbstractAnimation::Running)
            m_alertPulseAnim->stop();

        if (m_alertOpacityEffect)
            m_alertOpacityEffect->setOpacity(1.0);   // حالت عادی: کاملاً روشن
    }


    // --------------------------------------------------------
    // 11) Recommendation
    // --------------------------------------------------------
    QString recText = "No recommendation";

    switch (summary.recommendationCode)
    {
    case 1:
        recText = "Reposition / pressure relief needed";
        break;
    case 2:
        recText = "Urgent reposition / critical pressure exposure";
        break;
    default:
        break;
    }

    if (summary.recommendationCode == 0) {
        if (sacrumExpMin >= 10)
            recText = "Reposition patient";
        else if (noMoveMin >= 8)
            recText = "Encourage movement / reposition";
        else if (summary.riskScore >= 50)
            recText = "Monitor closely";
    }

    QString recStyle;

    if (summary.recommendationPriority >= 2)
    {
        // 🔴 HIGH PRIORITY
        recStyle =
            "QLabel {"
            "  color: white;"
            "  background-color: #7F1D1D;"
            "  border: 2px solid #EF4444;"
            "  border-radius: 12px;"
            "  padding: 14px;"
            "  font-size: 14px;"
            "  font-weight: 700;"
            "}";
    }
    else
    {
        // 🔵 NORMAL
        recStyle =
            "QLabel {"
            "  color: #F8FAFC;"
            "  background-color: #1E293B;"
            "  border: 1px solid #3B82F6;"
            "  border-radius: 12px;"
            "  padding: 14px;"
            "  font-size: 14px;"
            "  font-weight: 700;"
            "}";
    }

    m_lblRecommendation->setStyleSheet(recStyle);
    m_lblRecommendation->setText(recText);
}
/*========================================================================================*/

void MainWindow::renderAdaptiveZoneStats()
{
    if (!m_bedStore.hasFrame())
        return;

    const BodyDetector::Result body = BodyDetector::detect(&m_bedStore, 10, 4);
    if (!body.valid)
        return;

    const BodyZones::Zones zones = BodyZones::estimate(body);
    if (!zones.valid) {
        if (m_lblSacrum)   m_lblSacrum->setText("SACRUM\ninvalid");
        if (m_lblHeelLeft) m_lblHeelLeft->setText("LEFT HEEL\ninvalid");
        if (m_lblHeelRight) m_lblHeelRight->setText("RIGHT HEEL\ninvalid");
        return;
    }

    m_dbgSacrumRect = zones.sacrumRect;
    m_dbgLeftHeelRect = zones.leftHeelRect;
    m_dbgRightHeelRect = zones.rightHeelRect;

    const BodyZoneAnalyzer::Result stats = BodyZoneAnalyzer::analyze(&m_bedStore, zones);
    if (!stats.valid)
        return;

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
    if (!m_hasValidSummary || !m_bedStore.hasFrame())
        return;

    const BodyDetector::Result body = BodyDetector::detect(&m_bedStore, 10, 4);
    if (!body.valid)
        return;

    const BodyZones::Zones zones = BodyZones::estimate(body);
    if (!zones.valid)
        return;

    const BodyZoneAnalyzer::Result stats = BodyZoneAnalyzer::analyze(&m_bedStore, zones);
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
        return (d >= 0) ? QString("+%1").arg(d) : QString::number(d);
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

    QString frameText = "Frame ID: --";
    if (m_bedStore.hasFrame()) {
        frameText = QString("Frame ID: %1").arg(m_bedStore.frameId());
    }
    m_lblDbgFrame->setText(frameText);

    m_lblDbgSync->setText(
        QString("Sync: %1")
            .arg(isCurrentFrameSynchronized() ? "Synced" : "Waiting / Partial")
        );

    QString sourceText = "Source: Device";
    if (m_compareZoneStats) {
        sourceText = "Source: Compare (Device vs Adaptive)";
    } else if (m_useAdaptiveZoneStats) {
        sourceText = "Source: Adaptive";
    }
    m_lblDbgSource->setText(sourceText);

    if (!m_bedStore.hasFrame()) {
        m_lblDbgBody->setText("Body: --");
        m_lblDbgSacrum->setText("Sacrum: --");
        m_lblDbgHeelLeft->setText("Left Heel: --");
        m_lblDbgHeelRight->setText("Right Heel: --");
        return;
    }

    const BodyDetector::Result body = BodyDetector::detect(&m_bedStore, 10, 4);
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

    m_dbgSacrumRect = zones.sacrumRect;
    m_dbgLeftHeelRect = zones.leftHeelRect;
    m_dbgRightHeelRect = zones.rightHeelRect;

    const BodyZoneAnalyzer::Result stats = BodyZoneAnalyzer::analyze(&m_bedStore, zones);
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

void MainWindow::updateMiniRiskTrendChart()
{
    if (!m_riskTrendSeries || !m_riskTrendChartView)
        return;

    m_riskTrendSeries->clear();

    const QVector<TrendHistoryStore::Sample> &samples = m_trendHistory.samples();
    if (samples.isEmpty())
        return;

    const int lastRisk = samples.last().riskScore;

    QColor trendColor = QColor("#4CAF50");
    if (lastRisk >= 60)
        trendColor = QColor("#FF9800");
    if (lastRisk >= 80)
        trendColor = QColor("#F44336");

    QPen trendPen(trendColor);
    trendPen.setWidth(3);
    m_riskTrendSeries->setPen(trendPen);

    const int count = samples.size();
    const int startIndex = qMax(0, count - 60);

    int x = 0;
    for (int i = startIndex; i < count; ++i, ++x) {
        m_riskTrendSeries->append(x, samples[i].riskScore);
    }

    QChart *chart = m_riskTrendChartView->chart();
    if (!chart)
        return;

    const auto axesX = chart->axes(Qt::Horizontal);
    const auto axesY = chart->axes(Qt::Vertical);

    if (!axesX.isEmpty()) {
        if (auto axisX = qobject_cast<QValueAxis*>(axesX.first())) {
            axisX->setRange(0, qMax(20, x));
            axisX->setLabelsFont(QFont("Segoe UI", 8));
            axisX->setTitleFont(QFont("Segoe UI", 8));
            axisX->setTitleText("Time");
        }
    }

    if (!axesY.isEmpty()) {
        if (auto axisY = qobject_cast<QValueAxis*>(axesY.first())) {
            axisY->setRange(0, 100);
        }
    }
}

/*========================================================================================*/

void MainWindow::onNodeHealth(const NodeHealthPacket &pkt)
{
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

    for (int i = count; i < SensorStore::NODES; ++i) {
        m_store.setNodeState(i, NodeState::Offline);
    }

    m_frameSync.nodeHealthFrameId = pkt.frameId;
    m_frameSync.hasNodeHealth = true;

   // qDebug() << "NODE_HEALTH applied, frameId =" << pkt.frameId
    //         << "nodeCount =" << pkt.nodeCount;
}

/*========================================================================================*/

bool MainWindow::isCurrentFrameSynchronized() const
{
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

// ======================================================
// Main Board -> UI
// Handle received intervention plan.
//
// TYPE = 0x51
//
// Current behavior:
// - log plan data
// - update Debug Console labels
//
// Next phase:
// - store current plan_id
// - show approve/reject intervention card
// ======================================================
void MainWindow::onInterventionPlanReceived(const InterventionPlan &plan)
{
    #if TALMA_DEBUG_INTERVENTION
        qDebug() << "[UI] Intervention plan received:"
                 << "plan_id =" << plan.planId
                 << "board =" << plan.boardId
                 << "zone =" << plan.targetZone
                 << "motors =" << plan.motorCount
                 << "risk =" << plan.riskScore
                 << "level =" << plan.riskLevel
                 << "rec =" << plan.recommendationCode
                 << "reason =" << plan.reasonCode;
    #endif

    if (m_lblDbgMainBoardStatus) {
        m_lblDbgMainBoardStatus->setText(
            QString("Status: PLAN received id=%1").arg(plan.planId));
    }

    if (m_lblDbgLastRx) {
        m_lblDbgLastRx->setText(
            QString("Last RX: PLAN id=%1 zone=%2 motors=%3 risk=%4")
                .arg(plan.planId)
                .arg(plan.targetZone)
                .arg(plan.motorCount)
                .arg(plan.riskScore));
    }

    // ======================================================
    // Show/update intervention workflow card.
    //
    // A valid TYPE 0x51 plan has been received from Main Board.
    // ======================================================

    // Cache current pending plan
    m_pendingInterventionPlanId = plan.planId;
    m_hasPendingIntervention = true;

    // Update runtime labels
    if (m_lblInterventionPlanId) {
        m_lblInterventionPlanId->setText(
            QString("Plan ID: %1").arg(plan.planId));
    }

    if (m_lblInterventionTargetZone) {
        m_lblInterventionTargetZone->setText(
            QString("Target Zone: %1").arg(plan.targetZone));
    }

    if (m_lblInterventionRisk) {
        m_lblInterventionRisk->setText(
            QString("Risk Score: %1").arg(plan.riskScore));
    }

    if (m_lblInterventionMotorCount) {
        m_lblInterventionMotorCount->setText(
            QString("Motor Count: %1").arg(plan.motorCount));
    }
    // Reveal intervention card when a valid plan arrives.
    // The card is hidden by default at startup.
    if (m_grpInterventionCard)
        m_grpInterventionCard->setVisible(true);

    // A new valid plan is now pending for user approval.
    setInterventionUiState(InterventionUiState::PendingApproval);

}
/*========================================================================================*/
/*========================================================================================*/

// ======================================================
// Main Board -> UI
// Handle intervention lifecycle/result packet.
//
// TYPE = 0x54
//
// State:
// 0 = IDLE
// 1 = EXECUTING
// 2 = COMPLETED
// 3 = FAILED
// 4 = REJECTED
// ======================================================
void MainWindow::onInterventionResultReceived(const InterventionResult &result)
{
    #if TALMA_DEBUG_INTERVENTION
        qDebug() << "[UI] Intervention result received:"
                 << "plan_id =" << result.planId
                 << "state =" << result.state
                 << "board =" << result.boardId
                 << "motors =" << result.motorCount;
    #endif

    // Main Board answered with 0x54, so timeout watchdog is no longer needed.
    if (m_interventionTimeoutTimer)
        m_interventionTimeoutTimer->stop();

    if (!m_hasPendingIntervention ||
        result.planId != m_pendingInterventionPlanId) {

        qWarning() << "[UI] Ignoring intervention result for stale/unknown plan"
                   << "result_plan =" << result.planId
                   << "pending_plan =" << m_pendingInterventionPlanId;

        return;
    }

    QString statusText;
        switch (result.state)
        {
        case 0:
            setInterventionUiState(InterventionUiState::Idle);
            break;

        case 1:
            setInterventionUiState(InterventionUiState::Executing);
            break;

        case 2:
            m_hasPendingIntervention = false;
            setInterventionUiState(InterventionUiState::Completed);
            break;

        case 3:
            m_hasPendingIntervention = false;
            setInterventionUiState(InterventionUiState::Failed);
            break;

        case 4:
            m_hasPendingIntervention = false;
            setInterventionUiState(InterventionUiState::Rejected);
            break;

        default:
            qWarning() << "[UI] Unknown intervention result state =" << result.state;
            break;
        }

    if (m_lblDbgLastRx) {
        m_lblDbgLastRx->setText(
            QString("Last RX: RESULT plan=%1 state=%2")
                .arg(result.planId)
                .arg(result.state));
    }

}
/*========================================================================================*/
/*========================================================================================*/

// ======================================================
// Centralized Intervention UI state handler.
//
// Keeps intervention card behavior deterministic.
//
// NOTE:
// This is UI-side state only.
// Main Board remains the execution source of truth.
// ======================================================
void MainWindow::setInterventionUiState(InterventionUiState state)
{
    m_interventionState = state;

    QString statusText;
    QString statusColor = "#95A5A6";


    switch (state)
    {
    case InterventionUiState::Idle:
        statusText = "Status: Idle";

        if (m_btnApproveIntervention)
            m_btnApproveIntervention->setEnabled(false);

        if (m_btnRejectIntervention)
            m_btnRejectIntervention->setEnabled(false);

        break;

    case InterventionUiState::PendingApproval:
        statusText = "Status: Waiting for approval";
        statusColor = "#F39C12";

        if (m_btnApproveIntervention)
            m_btnApproveIntervention->setEnabled(true);

        if (m_btnRejectIntervention)
            m_btnRejectIntervention->setEnabled(true);

        break;

    case InterventionUiState::WaitingResult:
        statusText = "Status: Waiting for Main Board...";
        statusColor = "#F1C40F";


        if (m_btnApproveIntervention)
            m_btnApproveIntervention->setEnabled(false);

        if (m_btnRejectIntervention)
            m_btnRejectIntervention->setEnabled(false);

        break;

    case InterventionUiState::Executing:
        statusText = "Status: Executing...";
        statusColor = "#3498DB";

        break;

    case InterventionUiState::Completed:
        statusText = "Status: Completed";
        statusColor = "#2ECC71";

        break;

    case InterventionUiState::Failed:
        statusText = "Status: Failed";

        break;

    case InterventionUiState::Rejected:
        statusText = "Status: Rejected";
        statusColor = "#7F8C8D";

        break;

    case InterventionUiState::Timeout:
        statusText = "Status: Timeout";
        statusColor = "#E74C3C";

        break;
    }

    #if TALMA_DEBUG_INTERVENTION
    qDebug() << "[UI STATE]"
             << "state =" << static_cast<int>(state)
             << "text =" << statusText;
    #endif

    if (m_lblInterventionStatus)
        m_lblInterventionStatus->setText(statusText);

    m_lblInterventionStatus->setStyleSheet(
        QString(
            "QLabel {"
            " color: %1;"
            " font-weight: 700;"
            "}"
            ).arg(statusColor));

    #if TALMA_DEBUG_INTERVENTION
        qDebug() << "[UI STATE] Intervention state changed to"
                 << static_cast<int>(state);
    #endif
}
/*========================================================================================*/

/*========================================================================================*/

/*========================================================================================*/

/*========================================================================================*/

/*========================================================================================*/

/*========================================================================================*/
