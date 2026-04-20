#include "ui/main_window.h"
#include "ui/settings_dialog.h"
#include <QMessageBox>
#include <QCloseEvent>
#include <QStyle>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QMenuBar>
#include <QToolButton>
#include <QStandardPaths>
#include <QSizePolicy>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_forceExit(false)
    , m_isOfflineRunning(false)
    , m_floatingWindowVisible(false)
{
    setupUI();
    setupTrayIcon();
    loadConfig();

    m_floatingWindow = std::make_unique<FloatingWindow>();
    m_floatingWindow->setConfig(m_config.subtitle_config);
    connect(m_floatingWindow.get(), &FloatingWindow::windowHidden, this, [this]() {
        m_floatingWindowVisible = false;
        m_floatingWindowAction->setText("浮窗");
        m_config.floating_window_visible = false;
        saveConfig();
    });

    if (m_floatingWindow) {
        m_floatingWindow->updateLabelVisibility(m_config.audio_source);
    }

    m_floatingWindowVisible = m_config.floating_window_visible;
    if (m_floatingWindowVisible) {
        m_floatingWindow->show();
        m_floatingWindowAction->setText("隐藏");
    } else {
        m_floatingWindowAction->setText("浮窗");
    }

    m_offlineService = std::make_unique<OfflineRecognitionService>(this);
    connect(m_offlineService.get(), &OfflineRecognitionService::progressChanged,
            this, &MainWindow::onOfflineProgressChanged);
    connect(m_offlineService.get(), &OfflineRecognitionService::resultReceived,
            this, &MainWindow::onOfflineResultReceived);
    connect(m_offlineService.get(), &OfflineRecognitionService::finished,
            this, &MainWindow::onOfflineFinished);
    connect(m_offlineService.get(), &OfflineRecognitionService::errorOccurred,
            this, &MainWindow::onOfflineError);
}

MainWindow::~MainWindow()
{
    stopRecognition();
}

void MainWindow::setupUI()
{
    setWindowTitle("NWSpeech");
    setMinimumSize(550, 400);
    resize(550, 400);

    m_toolBar = addToolBar("MainToolbar");
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setStyleSheet(R"(
        QToolBar {
            background-color: #1C1C1E;
            border: none;
            spacing: 6px;
            padding: 6px 12px;
        }
        QToolButton {
            background-color: #3A3A3C;
            color: #E5E5EA;
            border: none;
            border-radius: 6px;
            padding: 6px 14px;
            font-size: 13px;
            font-weight: 500;
        }
        QToolButton:hover {
            background-color: #4A4A4C;
        }
        QToolButton:pressed {
            background-color: #5A5A5C;
        }
        QToolButton:disabled {
            background-color: #3A3A3A;
            color: #666666;
        }
    )");

    m_startAction = new QAction("开始", this);
    m_startAction->setShortcut(QKeySequence("Ctrl+S"));
    m_startAction->setObjectName("startAction");
    m_stopAction = new QAction("停止", this);
    m_stopAction->setEnabled(false);
    m_stopAction->setObjectName("stopAction");
    m_floatingWindowAction = new QAction("浮窗", this);
    m_floatingWindowAction->setObjectName("floatingAction");

    m_moreMenu = new QMenu(this);
    m_openFileMenuAction = m_moreMenu->addAction("离线识别");
    m_settingsMenuAction = m_moreMenu->addAction("设置");
    m_moreMenu->addSeparator();
    QAction *exitMenuAction = m_moreMenu->addAction("退出");

    m_moreAction = new QAction("更多", this);
    m_moreAction->setObjectName("moreAction");
    m_moreAction->setMenu(m_moreMenu);

    m_toolBar->addAction(m_startAction);
    m_toolBar->addAction(m_stopAction);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_floatingWindowAction);

    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(spacer);

    m_toolBar->addAction(m_moreAction);

    connect(m_openFileMenuAction, &QAction::triggered, this, &MainWindow::onOpenFileClicked);
    connect(m_settingsMenuAction, &QAction::triggered, this, &MainWindow::onSettingsClicked);
    connect(exitMenuAction, &QAction::triggered, this, &MainWindow::onExitApp);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    m_systemPartialLabel = new QLabel("", this);
    m_systemPartialLabel->setWordWrap(true);
    m_systemPartialLabel->setVisible(false);
    m_systemPartialLabel->setMinimumHeight(38);
    m_systemPartialLabel->setMaximumHeight(38);
    m_systemPartialLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_systemPartialLabel->setStyleSheet(R"(
        QLabel {
            background-color: #1C1C1E;
            border: 1px solid #3A3A3C;
            border-radius: 6px;
            padding: 8px;
            font-size: 13px;
            color: #007AFF;
        }
    )");
    mainLayout->addWidget(m_systemPartialLabel);

    m_micPartialLabel = new QLabel("", this);
    m_micPartialLabel->setWordWrap(true);
    m_micPartialLabel->setVisible(false);
    m_micPartialLabel->setMinimumHeight(38);
    m_micPartialLabel->setMaximumHeight(38);
    m_micPartialLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_micPartialLabel->setStyleSheet(R"(
        QLabel {
            background-color: #1C1C1E;
            border: 1px solid #3A3A3C;
            border-radius: 6px;
            padding: 8px;
            font-size: 13px;
            color: #30D158;
        }
    )");
    mainLayout->addWidget(m_micPartialLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumHeight(4);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(R"(
        QProgressBar {
            border: none;
            border-radius: 2px;
            background-color: #2C2C2E;
            height: 4px;
        }
        QProgressBar::chunk {
            background-color: #007AFF;
            border-radius: 2px;
        }
    )");
    mainLayout->addWidget(m_progressBar);

    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setPlaceholderText("识别结果将显示在这里...");
    m_logTextEdit->setStyleSheet(R"(
        QTextEdit {
            background-color: #1C1C1E;
            border: none;
            border-radius: 8px;
            padding: 10px;
            font-size: 13px;
            color: #E5E5EA;
        }
    )");
    mainLayout->addWidget(m_logTextEdit, 1);

    QStatusBar *statusBar = this->statusBar();
    statusBar->setStyleSheet(R"(
        QStatusBar {
            background-color: #1C1C1E;
            color: #8E8E93;
            border: none;
            font-size: 12px;
        }
        QStatusBar::item {
            border: none;
        }
    )");

    m_statusLabel = new QLabel("就绪", this);
    m_timeLabel = new QLabel("00:00:00", this);

    statusBar->addWidget(m_statusLabel);
    statusBar->addPermanentWidget(m_timeLabel);

    connect(m_startAction, &QAction::triggered, this, &MainWindow::onStartClicked);
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::onStopClicked);
    connect(m_floatingWindowAction, &QAction::triggered, this, &MainWindow::onToggleFloatingWindow);

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatus);

    QTimer::singleShot(0, this, &MainWindow::styleToolbarButtons);

    this->setStyleSheet(R"(
        QMainWindow {
            background-color: #000000;
        }
        QMenuBar {
            background-color: #1C1C1E;
            color: #E5E5EA;
            border: none;
        }
        QMenuBar::item {
            background-color: transparent;
            padding: 6px 12px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #3A3A3C;
        }
        QMenu {
            background-color: #1C1C1E;
            color: #E5E5EA;
            border: 1px solid #3A3A3C;
            border-radius: 8px;
        }
        QMenu::item {
            padding: 8px 24px;
        }
        QMenu::item:selected {
            background-color: #3A3A3C;
        }
        QMenu::separator {
            height: 1px;
            background-color: #3A3A3C;
            margin: 4px 8px;
        }
    )");
}

void MainWindow::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);

    QIcon icon = QIcon(":/icon.ico");
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip("NWSpeech");

    m_trayMenu = new QMenu(this);
    QAction *showAction = m_trayMenu->addAction("显示主窗口");
    QAction *exitAction = m_trayMenu->addAction("退出");

    connect(showAction, &QAction::triggered, this, &MainWindow::onShowMainWindow);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExitApp);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);

    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();
}

std::string MainWindow::getConfigFilePath()
{
    QString appDir = QCoreApplication::applicationDirPath();
    return (appDir + "/config.ini").toStdString();
}

void MainWindow::loadConfig()
{
    QString documentsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString defaultOutputDir = documentsDir + "/NWSpeech";

    std::string configPath = getConfigFilePath();
    m_config = LoadConfig(configPath);

    QDir dir;
    if (!dir.exists(defaultOutputDir)) {
        dir.mkpath(defaultOutputDir);
    }

    if (m_config.output_dir.empty()) {
        m_config.output_dir = defaultOutputDir.toStdString();
    }

    InitializeModelPaths(m_config);

    updatePartialLabelVisibility();
}

void MainWindow::saveConfig()
{
    AppConfig saveConfig = m_config;
    QString appDir = QCoreApplication::applicationDirPath();
    QDir modelsDir(saveConfig.models_base_path.c_str());
    if (modelsDir.isAbsolute()) {
        QString relativePath = QDir(appDir).relativeFilePath(saveConfig.models_base_path.c_str());
        if (!relativePath.startsWith("..")) {
            saveConfig.models_base_path = relativePath.toStdString();
        }
    }

    std::string configPath = getConfigFilePath();
    SaveConfig(saveConfig, configPath);
}

void MainWindow::onStartClicked()
{
    startRecognition();
}

void MainWindow::onStopClicked()
{
    if (m_isOfflineRunning) {
        if (m_offlineService) {
            m_offlineService->Stop();
        }
    } else {
        stopRecognition();
    }
}

void MainWindow::onSettingsClicked()
{
    SettingsDialog dialog(this);
    dialog.setConfig(m_config);
    if (dialog.exec() == QDialog::Accepted) {
        m_config = dialog.getConfig();
        if (m_floatingWindow) {
            m_floatingWindow->setConfig(m_config.subtitle_config);
        }
        saveConfig();
        updatePartialLabelVisibility();
    }
}

void MainWindow::startRecognition()
{
    if (m_service && m_service->IsRunning()) {
        return;
    }

    m_service = std::make_unique<SpeechRecognitionService>(this);

    connect(m_service.get(), &SpeechRecognitionService::partialResultChanged,
            this, &MainWindow::onPartialResultChanged);
    connect(m_service.get(), &SpeechRecognitionService::finalResultReceived,
            this, &MainWindow::onFinalResultReceived);
    connect(m_service.get(), &SpeechRecognitionService::errorOccurred,
            this, &MainWindow::onServiceError);

    if (m_service->Start(m_config)) {
        m_startTime = QDateTime::currentDateTime();
        m_statusTimer->start(100);

        m_startAction->setEnabled(false);
        m_stopAction->setEnabled(true);
        m_openFileMenuAction->setEnabled(false);
        m_settingsMenuAction->setEnabled(false);

        m_statusLabel->setText("运行中");
        m_logTextEdit->append("=== 开始识别 ===");

        if (m_floatingWindow && m_floatingWindowVisible) {
            m_floatingWindow->show();
            m_floatingWindow->clearText();
        }
    }
}

void MainWindow::stopRecognition()
{
    if (m_service) {
        m_service->Stop();
        m_statusTimer->stop();

        m_startAction->setEnabled(true);
        m_stopAction->setEnabled(false);
        m_openFileMenuAction->setEnabled(true);
        m_settingsMenuAction->setEnabled(true);

        m_statusLabel->setText("已停止");
        m_logTextEdit->append("=== 识别结束 ===");

        if (m_floatingWindow) {
            m_floatingWindow->clearText();
        }
    }
}

void MainWindow::updateStatus()
{
    if (!m_service || !m_service->IsRunning()) {
        return;
    }

    QDateTime current = QDateTime::currentDateTime();
    qint64 seconds = m_startTime.secsTo(current);
    int h = static_cast<int>(seconds / 3600);
    int m = static_cast<int>((seconds % 3600) / 60);
    int s = static_cast<int>(seconds % 60);
    m_timeLabel->setText(QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0')));
}

void MainWindow::onPartialResultChanged(const QString &text, AudioSourceTag source)
{
    if (m_floatingWindowVisible && m_floatingWindow) {
        m_floatingWindow->setText(text, source);
    }

    bool is_dual = (m_config.audio_source == AudioSource::kBoth);
    if (source == AudioSourceTag::kSystem) {
        m_systemPartialLabel->setText(is_dual ? "[系统] " + text : text);
        m_systemPartialLabel->setVisible(!text.isEmpty());
    } else if (source == AudioSourceTag::kMicrophone) {
        m_micPartialLabel->setText(is_dual ? "[麦克风] " + text : text);
        m_micPartialLabel->setVisible(!text.isEmpty());
    }
}

void MainWindow::onFinalResultReceived(const QString &text, AudioSourceTag source)
{
    m_logTextEdit->append(text);
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);

    if (source == AudioSourceTag::kSystem) {
        m_systemPartialLabel->clear();
    } else if (source == AudioSourceTag::kMicrophone) {
        m_micPartialLabel->clear();
    }
}

void MainWindow::onToggleFloatingWindow()
{
    if (m_floatingWindowVisible) {
        m_floatingWindow->hide();
        m_floatingWindowVisible = false;
        m_floatingWindowAction->setText("浮窗");
        m_config.floating_window_visible = false;
    } else {
        m_floatingWindow->show();
        m_floatingWindowVisible = true;
        m_floatingWindowAction->setText("隐藏");
        m_config.floating_window_visible = true;
    }
    saveConfig();
}

void MainWindow::onServiceError(const QString &error)
{
    QMessageBox::critical(this, "错误", error);
    stopRecognition();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        onShowMainWindow();
    }
}

void MainWindow::onShowMainWindow()
{
    show();
    activateWindow();
    raise();
}

void MainWindow::onExitApp()
{
    m_forceExit = true;
    close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_forceExit) {
        // 先保存配置，再关闭浮窗，避免关闭浮窗触发的信号覆盖状态
        saveConfig(); 
        
        if (m_service && m_service->IsRunning()) {
            stopRecognition();
        }
        if (m_floatingWindow) {
            // 关闭浮窗前先断开信号连接，避免修改配置
            disconnect(m_floatingWindow.get(), &FloatingWindow::windowHidden, nullptr, nullptr);
            m_floatingWindow->close();
        }
        event->accept();
        return;
    }

    if (m_service && m_service->IsRunning()) {
        hide();
        event->ignore();
        return;
    }

    // 先保存配置，再关闭浮窗
    saveConfig(); 
    
    if (m_floatingWindow) {
        // 关闭浮窗前先断开信号连接
        disconnect(m_floatingWindow.get(), &FloatingWindow::windowHidden, nullptr, nullptr);
        m_floatingWindow->close();
    }
    event->accept();
}

void MainWindow::onOpenFileClicked()
{
    QString file_path = QFileDialog::getOpenFileName(
        this,
        "选择音频文件",
        "",
        "WAV音频文件 (*.wav);;所有文件 (*.*)"
    );

    if (file_path.isEmpty()) {
        return;
    }

    m_isOfflineRunning = true;
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_openFileMenuAction->setEnabled(false);
    m_startAction->setEnabled(false);
    m_settingsMenuAction->setEnabled(false);
    m_stopAction->setEnabled(true);

    m_logTextEdit->append("=== 开始离线识别 ===");
    m_logTextEdit->append("文件: " + file_path);
    m_statusLabel->setText("识别文件中");

    m_offlineService->ProcessFile(file_path, m_config);
}

void MainWindow::onOfflineProgressChanged(int progress, int total)
{
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(progress);
}

void MainWindow::onOfflineResultReceived(const QString &text)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    m_logTextEdit->append(QString("[%1] %2").arg(timestamp, text));
}

void MainWindow::onOfflineFinished()
{
    m_isOfflineRunning = false;
    m_progressBar->setVisible(false);
    m_openFileMenuAction->setEnabled(true);
    m_startAction->setEnabled(true);
    m_settingsMenuAction->setEnabled(true);
    m_stopAction->setEnabled(false);
    m_statusLabel->setText("就绪");
    m_logTextEdit->append("=== 离线识别完成 ===");
}

void MainWindow::onOfflineError(const QString &error)
{
    m_isOfflineRunning = false;
    m_progressBar->setVisible(false);
    m_openFileMenuAction->setEnabled(true);
    m_startAction->setEnabled(true);
    m_settingsMenuAction->setEnabled(true);
    m_stopAction->setEnabled(false);
    m_statusLabel->setText("错误");
    QMessageBox::critical(this, "错误", error);
}

void MainWindow::updatePartialLabelVisibility()
{
    bool is_dual = (m_config.audio_source == AudioSource::kBoth);
    m_systemPartialLabel->setVisible(false);
    m_micPartialLabel->setVisible(false);

    if (m_floatingWindow) {
        m_floatingWindow->updateLabelVisibility(m_config.audio_source);
    }
}

void MainWindow::styleToolbarButtons()
{
    QList<QToolButton*> buttons = m_toolBar->findChildren<QToolButton*>();
    for (QToolButton *btn : buttons) {
        btn->setPopupMode(QToolButton::InstantPopup);

        QString baseStyle = R"(
            QToolButton {
                color: #E5E5EA;
                border: none;
                border-radius: 6px;
                padding: 6px 14px;
                font-size: 13px;
                font-weight: 500;
            }
            QToolButton:disabled {
                background-color: #3A3A3A;
                color: #666666;
            }
            QToolButton::menu-indicator {
                image: none;
            }
        )";

        if (btn->defaultAction() == m_startAction) {
            btn->setStyleSheet(baseStyle + R"(
                QToolButton {
                    background-color: #0A84FF;
                }
                QToolButton:hover {
                    background-color: #409CFF;
                }
                QToolButton:pressed {
                    background-color: #006EDB;
                }
            )");
        } else if (btn->defaultAction() == m_stopAction) {
            btn->setStyleSheet(baseStyle + R"(
                QToolButton {
                    background-color: #FF453A;
                }
                QToolButton:hover {
                    background-color: #FF6961;
                }
                QToolButton:pressed {
                    background-color: #D70015;
                }
            )");
        } else {
            btn->setStyleSheet(baseStyle + R"(
                QToolButton {
                    background-color: #3A3A3C;
                }
                QToolButton:hover {
                    background-color: #4A4A4C;
                }
                QToolButton:pressed {
                    background-color: #5A5A5C;
                }
            )");
        }
    }
}
