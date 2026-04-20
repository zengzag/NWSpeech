#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QTextEdit>
#include "config.h"

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    void setConfig(const AppConfig &config);
    AppConfig getConfig() const;

private slots:
    void onBrowseOutputDir();
    void onRealtimeModelTypeChanged(int index);
    void onOfflineModelTypeChanged(int index);
    void onRefreshMicrophoneDevices();
    void onAudioSourceChanged(int index);

private:
    void setupUI();
    void refreshMicrophoneDevices();
    bool isModelFolderExists(ModelType type);

    QComboBox *m_audioSourceCombo;
    QComboBox *m_microphoneDeviceCombo;
    QPushButton *m_refreshMicBtn;
    QLineEdit *m_outputDirEdit;
    QPushButton *m_browseOutputBtn;
    QCheckBox *m_saveTextCheck;
    QCheckBox *m_saveAudioCheck;

    QComboBox *m_realtimeModelTypeCombo;
    QDoubleSpinBox *m_realtimeVadThresholdSpin;
    QDoubleSpinBox *m_realtimeMinSilenceSpin;
    QDoubleSpinBox *m_realtimeMinSpeechSpin;
    QDoubleSpinBox *m_realtimeMaxSpeechSpin;

    QComboBox *m_offlineModelTypeCombo;
    QDoubleSpinBox *m_offlineVadThresholdSpin;
    QDoubleSpinBox *m_offlineMinSilenceSpin;
    QDoubleSpinBox *m_offlineMinSpeechSpin;
    QDoubleSpinBox *m_offlineMaxSpeechSpin;

    QSpinBox *m_subtitleWidthSpin;
    QSpinBox *m_subtitleHeightSpin;
    QSpinBox *m_subtitleFontSizeSpin;
    QLineEdit *m_subtitleTextColorEdit;
    QLineEdit *m_subtitleBgColorEdit;
    QSpinBox *m_subtitleOpacitySpin;

    QCheckBox *m_llmOptimizerEnabledCheck;
    QLineEdit *m_llmOptimizerApiUrlEdit;
    QLineEdit *m_llmOptimizerModelNameEdit;
    QLineEdit *m_llmOptimizerApiKeyEdit;
    QSpinBox *m_llmOptimizerContextSentencesSpin;
    QTextEdit *m_llmOptimizerPromptEdit;

    QCheckBox *m_llmSummaryEnabledCheck;
    QLineEdit *m_llmSummaryApiUrlEdit;
    QLineEdit *m_llmSummaryModelNameEdit;
    QLineEdit *m_llmSummaryApiKeyEdit;
    QTextEdit *m_llmSummaryPromptEdit;

    AppConfig m_config;
};

#endif // SETTINGSDIALOG_H
