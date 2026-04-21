#include "service/speech_recognition_service.h"
#include <QDateTime>
#include <vector>
#include <chrono>

SpeechRecognitionService::SpeechRecognitionService(QObject *parent)
    : QObject(parent), m_running(false)
{
    m_llmOptimizer = std::make_unique<LlmOptimizer>(this);
    connect(m_llmOptimizer.get(), &LlmOptimizer::optimizationResult, this, &SpeechRecognitionService::onLlmOptimizationResult);
    connect(this, &SpeechRecognitionService::requestOptimizeText, this, &SpeechRecognitionService::onRequestOptimizeText);
}

SpeechRecognitionService::~SpeechRecognitionService()
{
    Stop();
}

void SpeechRecognitionService::initPipeline(RecognitionPipeline *pipeline, const AppConfig &config)
{
    pipeline->audio_capture = std::make_unique<AudioCapture>();
    pipeline->resampler = std::make_unique<AudioResampler>();

    if (config.recognition_enabled) {
        pipeline->vad = std::make_unique<VoiceActivityDetectorWrapper>();
        pipeline->recognizer = std::make_unique<SpeechRecognizer>();

        if (!pipeline->vad->Initialize(config.vad_model_path, config.realtime_config.vad_threshold,
                                       config.realtime_config.min_silence_duration, config.realtime_config.min_speech_duration,
                                       config.realtime_config.max_speech_duration)) {
            emit errorOccurred("初始化VAD失败");
            return;
        }

        if (!pipeline->recognizer->Initialize(config.realtime_config.model_config)) {
            emit errorOccurred("初始化语音识别器失败");
            return;
        }
    }

    if (!pipeline->audio_capture->Start(pipeline->audio_source, pipeline->device_id)) {
        emit errorOccurred("启动音频捕获失败");
        return;
    }

    const int32_t target_sample_rate = config.sample_rate;
    const int32_t input_sample_rate = pipeline->audio_capture->GetSampleRate();

    if (!pipeline->resampler->Initialize(static_cast<float>(input_sample_rate), static_cast<float>(target_sample_rate))) {
        emit errorOccurred("初始化重采样器失败");
        pipeline->audio_capture->Stop();
        return;
    }
}

bool SpeechRecognitionService::Start(const AppConfig &config)
{
    if (m_running) {
        return true;
    }

    m_config = config;
    m_llmOptimizer->setOptimizerConfig(config.llm_optimizer_config);
    m_llmOptimizer->clearHistory();

    try {
        m_fileSaver = std::make_unique<FileSaver>();
        if (!m_fileSaver->Initialize(m_config.output_dir, m_config.audio_source, m_config.save_text, m_config.save_audio)) {
            emit errorOccurred("初始化文件保存器失败");
            return false;
        }

        bool is_dual = (config.audio_source == AudioSource::kBoth);

        if (is_dual) {
            m_systemPipeline.tag = AudioSourceTag::kSystem;
            m_systemPipeline.audio_source = AudioSource::kSystemAudio;
            m_systemPipeline.device_id = "";
            m_systemPipeline.running = true;

            m_micPipeline.tag = AudioSourceTag::kMicrophone;
            m_micPipeline.audio_source = AudioSource::kMicrophone;
            m_micPipeline.device_id = config.microphone_device_id;
            m_micPipeline.running = true;

            initPipeline(&m_systemPipeline, config);
            initPipeline(&m_micPipeline, config);

            m_systemPipeline.recognition_thread = std::thread(&SpeechRecognitionService::recognitionThread, this, &m_systemPipeline);
            m_micPipeline.recognition_thread = std::thread(&SpeechRecognitionService::recognitionThread, this, &m_micPipeline);
        } else {
            RecognitionPipeline *pipeline = nullptr;
            if (config.audio_source == AudioSource::kSystemAudio) {
                m_systemPipeline.tag = AudioSourceTag::kSystem;
                m_systemPipeline.audio_source = AudioSource::kSystemAudio;
                m_systemPipeline.device_id = "";
                pipeline = &m_systemPipeline;
            } else {
                m_micPipeline.tag = AudioSourceTag::kMicrophone;
                m_micPipeline.audio_source = AudioSource::kMicrophone;
                m_micPipeline.device_id = config.microphone_device_id;
                pipeline = &m_micPipeline;
            }
            pipeline->running = true;
            initPipeline(pipeline, config);
            pipeline->recognition_thread = std::thread(&SpeechRecognitionService::recognitionThread, this, pipeline);
        }

        m_running = true;
        return true;

    } catch (const std::exception &e) {
        emit errorOccurred(QString("启动失败: %1").arg(e.what()));
        Stop();
        return false;
    }
}

void SpeechRecognitionService::Stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;

    bool is_dual = (m_config.audio_source == AudioSource::kBoth);

    if (is_dual) {
        m_systemPipeline.running = false;
        m_micPipeline.running = false;

        if (m_systemPipeline.audio_capture) m_systemPipeline.audio_capture->Stop();
        if (m_micPipeline.audio_capture) m_micPipeline.audio_capture->Stop();

        if (m_systemPipeline.recognition_thread.joinable()) m_systemPipeline.recognition_thread.join();
        if (m_micPipeline.recognition_thread.joinable()) m_micPipeline.recognition_thread.join();
    } else {
        RecognitionPipeline *pipeline = nullptr;
        if (m_config.audio_source == AudioSource::kSystemAudio) {
            pipeline = &m_systemPipeline;
        } else {
            pipeline = &m_micPipeline;
        }
        if (pipeline) {
            pipeline->running = false;
            if (pipeline->audio_capture) pipeline->audio_capture->Stop();
            if (pipeline->recognition_thread.joinable()) pipeline->recognition_thread.join();
        }
    }

    if (m_fileSaver) {
        m_fileSaver->Close();
    }
}

void SpeechRecognitionService::recognitionThread(RecognitionPipeline *pipeline)
{
    const int32_t target_sample_rate = m_config.sample_rate;
    const int32_t window_size = 512;
    int32_t offset = 0;
    std::vector<float> buffer;
    bool speech_started = false;
    auto started_time = std::chrono::steady_clock::now();

    bool is_streaming = false;
    if (m_config.recognition_enabled && pipeline->recognizer) {
        is_streaming = pipeline->recognizer->IsStreaming();
        if (is_streaming) {
            pipeline->recognizer->CreateStream();
        }
    }

    while (pipeline->running) {
        std::vector<float> samples;
        if (!pipeline->audio_capture->GetSamples(samples)) {
            break;
        }

        auto resampled = pipeline->resampler->Resample(samples.data(), static_cast<int32_t>(samples.size()));
        buffer.insert(buffer.end(), resampled.begin(), resampled.end());

        m_fileSaver->SaveAudio(resampled, target_sample_rate, pipeline->tag);

        if (m_config.recognition_enabled && pipeline->recognizer && pipeline->vad) {
            if (is_streaming) {
                pipeline->recognizer->AcceptWaveform(static_cast<float>(target_sample_rate), resampled.data(), static_cast<int32_t>(resampled.size()));

                while (pipeline->recognizer->IsReady()) {
                    pipeline->recognizer->Decode();
                    std::string result = pipeline->recognizer->GetResult();
                    if (!result.empty()) {
                        emit partialResultChanged(QString::fromStdString(result), pipeline->tag);
                    }
                }
            }

            for (; offset + window_size < static_cast<int32_t>(buffer.size()); offset += window_size) {
                pipeline->vad->AcceptWaveform(buffer.data() + offset, window_size);

                if (!speech_started && pipeline->vad->IsDetected()) {
                    speech_started = true;
                    started_time = std::chrono::steady_clock::now();
                }
            }

            if (!speech_started) {
                if (static_cast<int32_t>(buffer.size()) > 10 * window_size) {
                    offset -= static_cast<int32_t>(buffer.size()) - 10 * window_size;
                    buffer = {buffer.end() - 10 * window_size, buffer.end()};
                }
            }

            if (!is_streaming) {
                auto current_time = std::chrono::steady_clock::now();
                const float elapsed_seconds =
                    std::chrono::duration_cast<std::chrono::milliseconds>(current_time - started_time).count() / 1000.f;

                if (speech_started && elapsed_seconds > 0.2f) {
                    std::string partial_result = pipeline->recognizer->Decode(
                        static_cast<float>(target_sample_rate),
                        buffer.data(),
                        static_cast<int32_t>(buffer.size()));

                    if (!partial_result.empty()) {
                        emit partialResultChanged(QString::fromStdString(partial_result), pipeline->tag);
                    }
                    started_time = std::chrono::steady_clock::now();
                }
            }

            while (!pipeline->vad->IsEmpty()) {
                auto segment_samples = pipeline->vad->GetFrontSamples();
                pipeline->vad->Pop();

                std::string result;
                if (is_streaming) {
                    pipeline->recognizer->InputFinished();
                    while (pipeline->recognizer->IsReady()) {
                        pipeline->recognizer->Decode();
                    }
                    result = pipeline->recognizer->GetResult();
                    pipeline->recognizer->Reset();
                    pipeline->recognizer->CreateStream();
                } else {
                    result = pipeline->recognizer->Decode(
                        static_cast<float>(target_sample_rate),
                        segment_samples.data(),
                        static_cast<int32_t>(segment_samples.size()));
                }

                if (!result.empty()) {
                    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
                    QString sourceLabel = (pipeline->tag == AudioSourceTag::kSystem) ? "[系统]" : "[麦克风]";
                    QString originalText = QString::fromStdString(result);

                    if (m_config.llm_optimizer_config.enabled) {
                        emit requestOptimizeText(originalText, timestamp, sourceLabel, pipeline->tag);
                    } else {
                        QString text = QString("[%1] %2 %3").arg(timestamp, sourceLabel, originalText);
                        emit finalResultReceived(text, pipeline->tag);
                        m_fileSaver->SaveText(result, pipeline->tag);
                    }
                }

                buffer.clear();
                offset = 0;
                speech_started = false;

                if (!is_streaming) {
                    emit partialResultChanged("", pipeline->tag);
                }
            }
        } else {
            buffer.clear();
            offset = 0;
        }
    }

    if (m_config.recognition_enabled && pipeline->recognizer && is_streaming) {
        pipeline->recognizer->Reset();
    }
}

void SpeechRecognitionService::onRequestOptimizeText(const QString &text, const QString &timestamp, const QString &sourceLabel, AudioSourceTag source)
{
    m_pendingTimestamp = timestamp;
    m_pendingSourceLabel = sourceLabel;
    m_pendingSource = source;
    m_llmOptimizer->optimizeText(text);
}

void SpeechRecognitionService::onLlmOptimizationResult(const QString &original, const QString &optimized)
{
    QString textToUse = optimized.isEmpty() ? original : optimized;
    QString text = QString("[%1] %2 %3").arg(m_pendingTimestamp, m_pendingSourceLabel, textToUse);
    emit finalResultReceived(text, m_pendingSource);
    m_fileSaver->SaveText(textToUse.toStdString(), m_pendingSource);
}
