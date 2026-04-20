#include "ui/settings_dialog.h"
#include "core/audio_capture.h"
#include <QFileDialog>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>
#include <QCoreApplication>
#include <QDir>
#include <QHBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::setupUI()
{
    setWindowTitle("设置");
    setMinimumSize(550, 520);
    resize(550, 520);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    QTabWidget *tabWidget = new QTabWidget(this);

    QWidget *basicTab = new QWidget(this);
    QVBoxLayout *basicTabLayout = new QVBoxLayout(basicTab);
    basicTabLayout->setSpacing(10);

    QGroupBox *generalGroup = new QGroupBox("通用配置", this);
    QFormLayout *generalLayout = new QFormLayout(generalGroup);
    generalLayout->setSpacing(8);

    generalLayout->addRow(new QLabel("音频来源:", this), m_audioSourceCombo = new QComboBox(this));
    m_audioSourceCombo->addItem("系统音频", static_cast<int>(AudioSource::kSystemAudio));
    m_audioSourceCombo->addItem("麦克风", static_cast<int>(AudioSource::kMicrophone));
    m_audioSourceCombo->addItem("系统音频+麦克风", static_cast<int>(AudioSource::kBoth));

    QHBoxLayout *micDeviceLayout = new QHBoxLayout();
    m_microphoneDeviceCombo = new QComboBox(this);
    m_refreshMicBtn = new QPushButton("刷新", this);
    micDeviceLayout->addWidget(m_microphoneDeviceCombo);
    micDeviceLayout->addWidget(m_refreshMicBtn);
    generalLayout->addRow(new QLabel("麦克风设备:", this), micDeviceLayout);

    QHBoxLayout *outputDirLayout = new QHBoxLayout();
    m_outputDirEdit = new QLineEdit(this);
    m_browseOutputBtn = new QPushButton("浏览...", this);
    outputDirLayout->addWidget(m_outputDirEdit);
    outputDirLayout->addWidget(m_browseOutputBtn);
    generalLayout->addRow(new QLabel("输出目录:", this), outputDirLayout);

    m_saveTextCheck = new QCheckBox("保存识别文本", this);
    m_saveTextCheck->setChecked(true);
    generalLayout->addRow("", m_saveTextCheck);

    m_saveAudioCheck = new QCheckBox("保存音频文件", this);
    m_saveAudioCheck->setChecked(true);
    generalLayout->addRow("", m_saveAudioCheck);

    basicTabLayout->addWidget(generalGroup);
    basicTabLayout->addStretch();

    QWidget *realtimeTab = new QWidget(this);
    QVBoxLayout *realtimeTabLayout = new QVBoxLayout(realtimeTab);
    realtimeTabLayout->setSpacing(10);

    QGroupBox *realtimeGroup = new QGroupBox("实时识别配置", this);
    QFormLayout *realtimeLayout = new QFormLayout(realtimeGroup);
    realtimeLayout->setSpacing(8);

    realtimeLayout->addRow(new QLabel("模型类型:", this), m_realtimeModelTypeCombo = new QComboBox(this));

    if (isModelFolderExists(ModelType::kStreamingZipformer)) {
        m_realtimeModelTypeCombo->addItem("Streaming Zipformer (快，效果一般)", static_cast<int>(ModelType::kStreamingZipformer));
    }
    if (isModelFolderExists(ModelType::kFireRedAsrCtc)) {
        m_realtimeModelTypeCombo->addItem("fire-red-asr2-ctc(较慢，效果好)", static_cast<int>(ModelType::kFireRedAsrCtc));
    }
    if (isModelFolderExists(ModelType::kFireRedAsr)) {
        m_realtimeModelTypeCombo->addItem("fire-red-asr2(极慢，效果最好)", static_cast<int>(ModelType::kFireRedAsr));
    }

    m_realtimeVadThresholdSpin = new QDoubleSpinBox(this);
    m_realtimeVadThresholdSpin->setRange(0.0, 1.0);
    m_realtimeVadThresholdSpin->setSingleStep(0.05);
    m_realtimeVadThresholdSpin->setValue(0.3);
    realtimeLayout->addRow(new QLabel("VAD阈值:", this), m_realtimeVadThresholdSpin);

    m_realtimeMinSilenceSpin = new QDoubleSpinBox(this);
    m_realtimeMinSilenceSpin->setRange(0.1, 5.0);
    m_realtimeMinSilenceSpin->setSingleStep(0.1);
    m_realtimeMinSilenceSpin->setValue(0.3);
    m_realtimeMinSilenceSpin->setSuffix(" 秒");
    realtimeLayout->addRow(new QLabel("最小静音时长:", this), m_realtimeMinSilenceSpin);

    m_realtimeMinSpeechSpin = new QDoubleSpinBox(this);
    m_realtimeMinSpeechSpin->setRange(0.05, 2.0);
    m_realtimeMinSpeechSpin->setSingleStep(0.05);
    m_realtimeMinSpeechSpin->setValue(0.1);
    m_realtimeMinSpeechSpin->setSuffix(" 秒");
    realtimeLayout->addRow(new QLabel("最小语音时长:", this), m_realtimeMinSpeechSpin);

    m_realtimeMaxSpeechSpin = new QDoubleSpinBox(this);
    m_realtimeMaxSpeechSpin->setRange(1.0, 120.0);
    m_realtimeMaxSpeechSpin->setSingleStep(1.0);
    m_realtimeMaxSpeechSpin->setValue(10.0);
    m_realtimeMaxSpeechSpin->setSuffix(" 秒");
    realtimeLayout->addRow(new QLabel("最大语音时长:", this), m_realtimeMaxSpeechSpin);

    realtimeTabLayout->addWidget(realtimeGroup);
    realtimeTabLayout->addStretch();

    QWidget *offlineTab = new QWidget(this);
    QVBoxLayout *offlineTabLayout = new QVBoxLayout(offlineTab);
    offlineTabLayout->setSpacing(10);

    QGroupBox *offlineGroup = new QGroupBox("离线识别配置", this);
    QFormLayout *offlineLayout = new QFormLayout(offlineGroup);
    offlineLayout->setSpacing(8);

    offlineLayout->addRow(new QLabel("模型类型:", this), m_offlineModelTypeCombo = new QComboBox(this));
    if (isModelFolderExists(ModelType::kStreamingZipformer)) {
        m_offlineModelTypeCombo->addItem("Streaming Zipformer (快，效果一般)", static_cast<int>(ModelType::kStreamingZipformer));
    }
    if (isModelFolderExists(ModelType::kFireRedAsrCtc)) {
        m_offlineModelTypeCombo->addItem("fire-red-asr2-ctc(较慢，效果好)", static_cast<int>(ModelType::kFireRedAsrCtc));
    }
    if (isModelFolderExists(ModelType::kFireRedAsr)) {
        m_offlineModelTypeCombo->addItem("fire-red-asr2(极慢，效果最好)", static_cast<int>(ModelType::kFireRedAsr));
    }

    m_offlineVadThresholdSpin = new QDoubleSpinBox(this);
    m_offlineVadThresholdSpin->setRange(0.0, 1.0);
    m_offlineVadThresholdSpin->setSingleStep(0.05);
    m_offlineVadThresholdSpin->setValue(0.3);
    offlineLayout->addRow(new QLabel("VAD阈值:", this), m_offlineVadThresholdSpin);

    m_offlineMinSilenceSpin = new QDoubleSpinBox(this);
    m_offlineMinSilenceSpin->setRange(0.1, 5.0);
    m_offlineMinSilenceSpin->setSingleStep(0.1);
    m_offlineMinSilenceSpin->setValue(0.5);
    m_offlineMinSilenceSpin->setSuffix(" 秒");
    offlineLayout->addRow(new QLabel("最小静音时长:", this), m_offlineMinSilenceSpin);

    m_offlineMinSpeechSpin = new QDoubleSpinBox(this);
    m_offlineMinSpeechSpin->setRange(0.05, 2.0);
    m_offlineMinSpeechSpin->setSingleStep(0.05);
    m_offlineMinSpeechSpin->setValue(0.1);
    m_offlineMinSpeechSpin->setSuffix(" 秒");
    offlineLayout->addRow(new QLabel("最小语音时长:", this), m_offlineMinSpeechSpin);

    m_offlineMaxSpeechSpin = new QDoubleSpinBox(this);
    m_offlineMaxSpeechSpin->setRange(1.0, 120.0);
    m_offlineMaxSpeechSpin->setSingleStep(1.0);
    m_offlineMaxSpeechSpin->setValue(30.0);
    m_offlineMaxSpeechSpin->setSuffix(" 秒");
    offlineLayout->addRow(new QLabel("最大语音时长:", this), m_offlineMaxSpeechSpin);

    offlineTabLayout->addWidget(offlineGroup);
    offlineTabLayout->addStretch();

    QWidget *subtitleTab = new QWidget(this);
    QVBoxLayout *subtitleTabLayout = new QVBoxLayout(subtitleTab);
    subtitleTabLayout->setSpacing(10);

    QGroupBox *subtitleGroup = new QGroupBox("浮窗设置", this);
    QFormLayout *subtitleLayout = new QFormLayout(subtitleGroup);
    subtitleLayout->setSpacing(8);

    m_subtitleWidthSpin = new QSpinBox(this);
    m_subtitleWidthSpin->setRange(200, 1000);
    m_subtitleWidthSpin->setValue(500);
    subtitleLayout->addRow(new QLabel("窗口宽度:", this), m_subtitleWidthSpin);

    m_subtitleHeightSpin = new QSpinBox(this);
    m_subtitleHeightSpin->setRange(50, 300);
    m_subtitleHeightSpin->setValue(80);
    subtitleLayout->addRow(new QLabel("窗口高度:", this), m_subtitleHeightSpin);

    m_subtitleFontSizeSpin = new QSpinBox(this);
    m_subtitleFontSizeSpin->setRange(12, 48);
    m_subtitleFontSizeSpin->setValue(16);
    subtitleLayout->addRow(new QLabel("文字大小:", this), m_subtitleFontSizeSpin);

    m_subtitleTextColorEdit = new QLineEdit(this);
    m_subtitleTextColorEdit->setText("#FFFFFF");
    subtitleLayout->addRow(new QLabel("文字颜色:", this), m_subtitleTextColorEdit);

    m_subtitleBgColorEdit = new QLineEdit(this);
    m_subtitleBgColorEdit->setText("#000000");
    subtitleLayout->addRow(new QLabel("背景颜色:", this), m_subtitleBgColorEdit);

    m_subtitleOpacitySpin = new QSpinBox(this);
    m_subtitleOpacitySpin->setRange(0, 255);
    m_subtitleOpacitySpin->setValue(180);
    subtitleLayout->addRow(new QLabel("背景透明度 (0-255):", this), m_subtitleOpacitySpin);

    subtitleTabLayout->addWidget(subtitleGroup);
    subtitleTabLayout->addStretch();

    QWidget *llmOptimizerTab = new QWidget(this);
    QVBoxLayout *llmOptimizerTabLayout = new QVBoxLayout(llmOptimizerTab);
    llmOptimizerTabLayout->setSpacing(10);

    QGroupBox *llmOptimizerGroup = new QGroupBox("文本优化设置", this);
    QFormLayout *llmOptimizerLayout = new QFormLayout(llmOptimizerGroup);
    llmOptimizerLayout->setSpacing(8);

    m_llmOptimizerEnabledCheck = new QCheckBox("启用文本优化", this);
    llmOptimizerLayout->addRow("", m_llmOptimizerEnabledCheck);

    m_llmOptimizerApiUrlEdit = new QLineEdit(this);
    llmOptimizerLayout->addRow(new QLabel("API 地址:", this), m_llmOptimizerApiUrlEdit);

    m_llmOptimizerModelNameEdit = new QLineEdit(this);
    llmOptimizerLayout->addRow(new QLabel("模型名称:", this), m_llmOptimizerModelNameEdit);

    m_llmOptimizerApiKeyEdit = new QLineEdit(this);
    m_llmOptimizerApiKeyEdit->setEchoMode(QLineEdit::Password);
    llmOptimizerLayout->addRow(new QLabel("API Key:", this), m_llmOptimizerApiKeyEdit);

    m_llmOptimizerContextSentencesSpin = new QSpinBox(this);
    m_llmOptimizerContextSentencesSpin->setRange(0, 10);
    m_llmOptimizerContextSentencesSpin->setValue(3);
    llmOptimizerLayout->addRow(new QLabel("上下文句子数:", this), m_llmOptimizerContextSentencesSpin);

    m_llmOptimizerPromptEdit = new QTextEdit(this);
    m_llmOptimizerPromptEdit->setMinimumHeight(150);
    m_llmOptimizerPromptEdit->setPlaceholderText("提示词模板，支持 {context} 和 {current} 占位符");
    llmOptimizerLayout->addRow(new QLabel("提示词模板:", this), m_llmOptimizerPromptEdit);

    llmOptimizerTabLayout->addWidget(llmOptimizerGroup);
    llmOptimizerTabLayout->addStretch();

    QWidget *llmSummaryTab = new QWidget(this);
    QVBoxLayout *llmSummaryTabLayout = new QVBoxLayout(llmSummaryTab);
    llmSummaryTabLayout->setSpacing(10);

    QGroupBox *llmSummaryGroup = new QGroupBox("会议纪要设置", this);
    QFormLayout *llmSummaryLayout = new QFormLayout(llmSummaryGroup);
    llmSummaryLayout->setSpacing(8);

    m_llmSummaryEnabledCheck = new QCheckBox("启用会议纪要", this);
    llmSummaryLayout->addRow("", m_llmSummaryEnabledCheck);

    m_llmSummaryApiUrlEdit = new QLineEdit(this);
    llmSummaryLayout->addRow(new QLabel("API 地址:", this), m_llmSummaryApiUrlEdit);

    m_llmSummaryModelNameEdit = new QLineEdit(this);
    llmSummaryLayout->addRow(new QLabel("模型名称:", this), m_llmSummaryModelNameEdit);

    m_llmSummaryApiKeyEdit = new QLineEdit(this);
    m_llmSummaryApiKeyEdit->setEchoMode(QLineEdit::Password);
    llmSummaryLayout->addRow(new QLabel("API Key:", this), m_llmSummaryApiKeyEdit);

    m_llmSummaryPromptEdit = new QTextEdit(this);
    m_llmSummaryPromptEdit->setMinimumHeight(150);
    m_llmSummaryPromptEdit->setPlaceholderText("提示词模板，支持 {content} 占位符");
    llmSummaryLayout->addRow(new QLabel("提示词模板:", this), m_llmSummaryPromptEdit);

    llmSummaryTabLayout->addWidget(llmSummaryGroup);
    llmSummaryTabLayout->addStretch();

    tabWidget->addTab(basicTab, "基础");
    tabWidget->addTab(realtimeTab, "实时识别");
    tabWidget->addTab(offlineTab, "离线识别");
    tabWidget->addTab(subtitleTab, "浮窗");
    tabWidget->addTab(llmOptimizerTab, "LLM 优化");
    tabWidget->addTab(llmSummaryTab, "LLM 纪要");

    mainLayout->addWidget(tabWidget);

    QPushButton *okBtn = new QPushButton("确定", this);
    QPushButton *cancelBtn = new QPushButton("取消", this);
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_audioSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onAudioSourceChanged);
    connect(m_refreshMicBtn, &QPushButton::clicked, this, &SettingsDialog::onRefreshMicrophoneDevices);
    connect(m_realtimeModelTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onRealtimeModelTypeChanged);
    connect(m_offlineModelTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onOfflineModelTypeChanged);
    connect(m_browseOutputBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseOutputDir);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    refreshMicrophoneDevices();

    this->setStyleSheet(R"(
        QDialog {
            background-color: #000000;
        }
        QTabWidget::pane {
            border: 1px solid #3A3A3C;
            background-color: #000000;
        }
        QTabBar::tab {
            background-color: #1C1C1E;
            color: #8E8E93;
            padding: 6px 16px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #2C2C2E;
            color: #E5E5EA;
        }
        QGroupBox {
            color: #E5E5EA;
            font-weight: bold;
            margin-top: 10px;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
        QLabel {
            color: #E5E5EA;
        }
        QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox {
            background-color: #1C1C1E;
            border: 1px solid #3A3A3C;
            border-radius: 4px;
            padding: 4px 8px;
            color: #E5E5EA;
        }
        QComboBox QAbstractItemView {
            background-color: #1C1C1E;
            border: 1px solid #3A3A3C;
            color: #E5E5EA;
            selection-background-color: #3A3A3C;
        }
        QCheckBox {
            color: #E5E5EA;
        }
    )");
}

void SettingsDialog::setConfig(const AppConfig &config)
{
    m_config = config;
    m_outputDirEdit->setText(QString::fromStdString(config.output_dir));
    m_saveTextCheck->setChecked(config.save_text);
    m_saveAudioCheck->setChecked(config.save_audio);

    int audioSourceIndex = m_audioSourceCombo->findData(static_cast<int>(config.audio_source));
    if (audioSourceIndex >= 0) {
        m_audioSourceCombo->setCurrentIndex(audioSourceIndex);
    }

    refreshMicrophoneDevices();

    int micIndex = m_microphoneDeviceCombo->findData(QString::fromStdString(config.microphone_device_id));
    if (micIndex >= 0) {
        m_microphoneDeviceCombo->setCurrentIndex(micIndex);
    }

    onAudioSourceChanged(m_audioSourceCombo->currentIndex());

    int realtimeModelIndex = m_realtimeModelTypeCombo->findData(static_cast<int>(config.realtime_config.model_config.type));
    if (realtimeModelIndex >= 0) {
        m_realtimeModelTypeCombo->setCurrentIndex(realtimeModelIndex);
    } else if (m_realtimeModelTypeCombo->count() > 0) {
        m_realtimeModelTypeCombo->setCurrentIndex(0);
    }
    m_realtimeVadThresholdSpin->setValue(config.realtime_config.vad_threshold);
    m_realtimeMinSilenceSpin->setValue(config.realtime_config.min_silence_duration);
    m_realtimeMinSpeechSpin->setValue(config.realtime_config.min_speech_duration);
    m_realtimeMaxSpeechSpin->setValue(config.realtime_config.max_speech_duration);

    int offlineModelIndex = m_offlineModelTypeCombo->findData(static_cast<int>(config.offline_config.model_config.type));
    if (offlineModelIndex >= 0) {
        m_offlineModelTypeCombo->setCurrentIndex(offlineModelIndex);
    } else if (m_offlineModelTypeCombo->count() > 0) {
        m_offlineModelTypeCombo->setCurrentIndex(0);
    }
    m_offlineVadThresholdSpin->setValue(config.offline_config.vad_threshold);
    m_offlineMinSilenceSpin->setValue(config.offline_config.min_silence_duration);
    m_offlineMinSpeechSpin->setValue(config.offline_config.min_speech_duration);
    m_offlineMaxSpeechSpin->setValue(config.offline_config.max_speech_duration);

    m_subtitleWidthSpin->setValue(config.subtitle_config.window_width);
    m_subtitleHeightSpin->setValue(config.subtitle_config.window_height);
    m_subtitleFontSizeSpin->setValue(config.subtitle_config.font_size);
    m_subtitleTextColorEdit->setText(QString::fromStdString(config.subtitle_config.text_color));
    m_subtitleBgColorEdit->setText(QString::fromStdString(config.subtitle_config.background_color));
    m_subtitleOpacitySpin->setValue(config.subtitle_config.background_opacity);

    m_llmOptimizerEnabledCheck->setChecked(config.llm_optimizer_config.enabled);
    m_llmOptimizerApiUrlEdit->setText(QString::fromStdString(config.llm_optimizer_config.api_url));
    m_llmOptimizerModelNameEdit->setText(QString::fromStdString(config.llm_optimizer_config.model_name));
    m_llmOptimizerApiKeyEdit->setText(QString::fromStdString(config.llm_optimizer_config.api_key));
    m_llmOptimizerContextSentencesSpin->setValue(config.llm_optimizer_config.context_sentences);
    m_llmOptimizerPromptEdit->setPlainText(QString::fromStdString(config.llm_optimizer_config.prompt));

    m_llmSummaryEnabledCheck->setChecked(config.llm_summary_config.enabled);
    m_llmSummaryApiUrlEdit->setText(QString::fromStdString(config.llm_summary_config.api_url));
    m_llmSummaryModelNameEdit->setText(QString::fromStdString(config.llm_summary_config.model_name));
    m_llmSummaryApiKeyEdit->setText(QString::fromStdString(config.llm_summary_config.api_key));
    m_llmSummaryPromptEdit->setPlainText(QString::fromStdString(config.llm_summary_config.prompt));
}

AppConfig SettingsDialog::getConfig() const
{
    AppConfig config = m_config;

    QString appDir = QCoreApplication::applicationDirPath();
    config.models_base_path = (appDir + "/models").toStdString();
    config.vad_model_path = config.models_base_path + "/silero_vad.onnx";

    config.output_dir = m_outputDirEdit->text().toStdString();
    config.save_text = m_saveTextCheck->isChecked();
    config.save_audio = m_saveAudioCheck->isChecked();
    config.audio_source = static_cast<AudioSource>(m_audioSourceCombo->currentData().toInt());
    config.microphone_device_id = m_microphoneDeviceCombo->currentData().toString().toStdString();

    config.realtime_config.model_config.type = static_cast<ModelType>(m_realtimeModelTypeCombo->currentData().toInt());
    config.realtime_config.vad_threshold = m_realtimeVadThresholdSpin->value();
    config.realtime_config.min_silence_duration = m_realtimeMinSilenceSpin->value();
    config.realtime_config.min_speech_duration = m_realtimeMinSpeechSpin->value();
    config.realtime_config.max_speech_duration = m_realtimeMaxSpeechSpin->value();
    InitializeModelConfig(config.realtime_config.model_config, config.models_base_path);

    config.offline_config.model_config.type = static_cast<ModelType>(m_offlineModelTypeCombo->currentData().toInt());
    config.offline_config.vad_threshold = m_offlineVadThresholdSpin->value();
    config.offline_config.min_silence_duration = m_offlineMinSilenceSpin->value();
    config.offline_config.min_speech_duration = m_offlineMinSpeechSpin->value();
    config.offline_config.max_speech_duration = m_offlineMaxSpeechSpin->value();
    InitializeModelConfig(config.offline_config.model_config, config.models_base_path);

    config.subtitle_config.window_width = m_subtitleWidthSpin->value();
    config.subtitle_config.window_height = m_subtitleHeightSpin->value();
    config.subtitle_config.font_size = m_subtitleFontSizeSpin->value();
    config.subtitle_config.text_color = m_subtitleTextColorEdit->text().toStdString();
    config.subtitle_config.background_color = m_subtitleBgColorEdit->text().toStdString();
    config.subtitle_config.background_opacity = m_subtitleOpacitySpin->value();

    config.llm_optimizer_config.enabled = m_llmOptimizerEnabledCheck->isChecked();
    config.llm_optimizer_config.api_url = m_llmOptimizerApiUrlEdit->text().toStdString();
    config.llm_optimizer_config.model_name = m_llmOptimizerModelNameEdit->text().toStdString();
    config.llm_optimizer_config.api_key = m_llmOptimizerApiKeyEdit->text().toStdString();
    config.llm_optimizer_config.context_sentences = m_llmOptimizerContextSentencesSpin->value();
    config.llm_optimizer_config.prompt = m_llmOptimizerPromptEdit->toPlainText().toStdString();

    config.llm_summary_config.enabled = m_llmSummaryEnabledCheck->isChecked();
    config.llm_summary_config.api_url = m_llmSummaryApiUrlEdit->text().toStdString();
    config.llm_summary_config.model_name = m_llmSummaryModelNameEdit->text().toStdString();
    config.llm_summary_config.api_key = m_llmSummaryApiKeyEdit->text().toStdString();
    config.llm_summary_config.prompt = m_llmSummaryPromptEdit->toPlainText().toStdString();

    return config;
}

void SettingsDialog::onBrowseOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择输出目录", m_outputDirEdit->text());
    if (!dir.isEmpty()) {
        m_outputDirEdit->setText(dir);
    }
}

void SettingsDialog::onRealtimeModelTypeChanged(int index)
{
    Q_UNUSED(index);
}

void SettingsDialog::onOfflineModelTypeChanged(int index)
{
    Q_UNUSED(index);
}

void SettingsDialog::onRefreshMicrophoneDevices()
{
    refreshMicrophoneDevices();
}

void SettingsDialog::onAudioSourceChanged(int index)
{
    AudioSource source = static_cast<AudioSource>(m_audioSourceCombo->itemData(index).toInt());
    m_microphoneDeviceCombo->setEnabled(source != AudioSource::kSystemAudio);
    m_refreshMicBtn->setEnabled(source != AudioSource::kSystemAudio);
}

void SettingsDialog::refreshMicrophoneDevices()
{
    QString currentDeviceId = m_microphoneDeviceCombo->currentData().toString();

    m_microphoneDeviceCombo->clear();
    m_microphoneDeviceCombo->addItem("默认麦克风", "");

    auto devices = AudioCapture::GetMicrophoneDevices();
    for (const auto &device : devices) {
        QString deviceId = QString::fromStdString(device.id);
        QString deviceName = QString::fromStdString(device.name);
        m_microphoneDeviceCombo->addItem(deviceName, deviceId);
    }

    int idx = m_microphoneDeviceCombo->findData(currentDeviceId);
    if (idx >= 0) {
        m_microphoneDeviceCombo->setCurrentIndex(idx);
    }
}

bool SettingsDialog::isModelFolderExists(ModelType type)
{
    QString basePath = QCoreApplication::applicationDirPath() + "/models";
    QString folder;

    if (type == ModelType::kFireRedAsrCtc) {
        folder = basePath + "/fire-red-asr2-ctc";
    } else if (type == ModelType::kFireRedAsr) {
        folder = basePath + "/fire-red-asr2";
    } else if (type == ModelType::kStreamingZipformer) {
        folder = basePath + "/streaming-zipformer";
    }

    return QDir(folder).exists();
}
