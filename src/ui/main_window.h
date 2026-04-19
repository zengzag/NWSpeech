#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QDateTime>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QProgressBar>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <memory>
#include "config.h"
#include "service/speech_recognition_service.h"
#include "service/offline_recognition_service.h"
#include "ui/floating_window.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onSettingsClicked();
    void updateStatus();
    void onPartialResultChanged(const QString &text, AudioSourceTag source);
    void onFinalResultReceived(const QString &text, AudioSourceTag source);
    void onServiceError(const QString &error);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowMainWindow();
    void onExitApp();
    void onOpenFileClicked();
    void onOfflineProgressChanged(int progress, int total);
    void onOfflineResultReceived(const QString &text);
    void onOfflineFinished();
    void onOfflineError(const QString &error);
    void onToggleFloatingWindow();
    void styleToolbarButtons();

private:
    void setupUI();
    void setupTrayIcon();
    void loadConfig();
    void saveConfig();
    void startRecognition();
    void stopRecognition();
    void updatePartialLabelVisibility();
    std::string getConfigFilePath();

    QAction *m_startAction;
    QAction *m_stopAction;
    QAction *m_floatingWindowAction;
    QAction *m_moreAction;
    QAction *m_openFileMenuAction;
    QAction *m_settingsMenuAction;
    QMenu *m_moreMenu;
    QToolBar *m_toolBar;
    QTextEdit *m_logTextEdit;
    QLabel *m_statusLabel;
    QLabel *m_timeLabel;
    QLabel *m_systemPartialLabel;
    QLabel *m_micPartialLabel;

    std::unique_ptr<SpeechRecognitionService> m_service;
    std::unique_ptr<FloatingWindow> m_floatingWindow;
    std::unique_ptr<OfflineRecognitionService> m_offlineService;
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QTimer *m_statusTimer;
    QDateTime m_startTime;
    QProgressBar *m_progressBar;
    bool m_isOfflineRunning;
    bool m_floatingWindowVisible;

    AppConfig m_config;
    bool m_forceExit;
};

#endif // MAINWINDOW_H
