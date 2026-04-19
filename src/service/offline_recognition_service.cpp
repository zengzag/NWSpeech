#include "service/offline_recognition_service.h"
#include <QFile>
#include <QDataStream>
#include <vector>
#include <iostream>
#include <algorithm>

OfflineRecognitionWorker::OfflineRecognitionWorker(QObject *parent)
    : QObject(parent)
    , m_running(false)
{
}

OfflineRecognitionWorker::~OfflineRecognitionWorker()
{
    stop();
}

void OfflineRecognitionWorker::setData(const QString &file_path, const AppConfig &config)
{
    m_filePath = file_path;
    m_config = config;
}

void OfflineRecognitionWorker::stop()
{
    m_running = false;
}

void OfflineRecognitionWorker::process()
{
    if (m_running) {
        return;
    }

    m_running = true;

    try {
        m_vad = std::make_unique<VoiceActivityDetectorWrapper>();
        m_recognizer = std::make_unique<SpeechRecognizer>();
        m_resampler = std::make_unique<AudioResampler>();

        if (!m_vad->Initialize(m_config.vad_model_path, m_config.offline_config.vad_threshold,
                               m_config.offline_config.min_silence_duration, m_config.offline_config.min_speech_duration,
                               m_config.offline_config.max_speech_duration)) {
            emit errorOccurred("初始化VAD失败");
            m_running = false;
            emit finished();
            return;
        }

        if (!m_recognizer->Initialize(m_config.offline_config.model_config)) {
            emit errorOccurred("初始化语音识别器失败");
            m_running = false;
            emit finished();
            return;
        }

        std::vector<float> samples;
        int32_t sample_rate;
        if (!LoadWavFile(m_filePath, samples, sample_rate)) {
            emit errorOccurred("加载音频文件失败");
            m_running = false;
            emit finished();
            return;
        }

        const int32_t target_sample_rate = m_config.sample_rate;
        if (sample_rate != target_sample_rate) {
            if (!m_resampler->Initialize(static_cast<float>(sample_rate), static_cast<float>(target_sample_rate))) {
                emit errorOccurred("初始化重采样器失败");
                m_running = false;
                emit finished();
                return;
            }
            samples = m_resampler->Resample(samples.data(), static_cast<int32_t>(samples.size()));
            sample_rate = target_sample_rate;
        }

        ProcessWithVAD(samples, sample_rate);

        m_running = false;
        emit finished();
        return;

    } catch (const std::exception &e) {
        emit errorOccurred(QString("处理失败: %1").arg(e.what()));
        m_running = false;
        emit finished();
        return;
    }
}

bool OfflineRecognitionWorker::LoadWavFile(const QString &file_path, std::vector<float> &samples, int32_t &sample_rate)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Failed to open file: " << file_path.toStdString() << std::endl;
        return false;
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    char riff[4];
    in.readRawData(riff, 4);
    if (std::string(riff, 4) != "RIFF") {
        std::cerr << "Not a RIFF file" << std::endl;
        return false;
    }

    quint32 file_size;
    in >> file_size;

    char wave[4];
    in.readRawData(wave, 4);
    if (std::string(wave, 4) != "WAVE") {
        std::cerr << "Not a WAVE file" << std::endl;
        return false;
    }

    char fmt[4];
    in.readRawData(fmt, 4);
    if (std::string(fmt, 4) != "fmt ") {
        std::cerr << "Not a fmt chunk" << std::endl;
        return false;
    }

    quint32 fmt_size;
    in >> fmt_size;

    quint16 format_tag;
    in >> format_tag;

    quint16 channels;
    in >> channels;

    quint32 sample_rate_32;
    in >> sample_rate_32;
    sample_rate = static_cast<int32_t>(sample_rate_32);

    quint32 byte_rate;
    in >> byte_rate;

    quint16 block_align;
    in >> block_align;

    quint16 bits_per_sample;
    in >> bits_per_sample;

    char data[4];
    in.readRawData(data, 4);
    while (std::string(data, 4) != "data") {
        quint32 skip_size;
        in >> skip_size;
        file.seek(file.pos() + skip_size);
        in.readRawData(data, 4);
        if (file.atEnd()) {
            std::cerr << "No data chunk found" << std::endl;
            return false;
        }
    }

    quint32 data_size;
    in >> data_size;

    samples.clear();
    int num_samples = data_size / (bits_per_sample / 8) / channels;
    samples.reserve(num_samples);

    for (int i = 0; i < num_samples; ++i) {
        if (bits_per_sample == 16) {
            qint16 value;
            in >> value;
            if (channels == 2) {
                qint16 right;
                in >> right;
                value = (value + right) / 2;
            }
            samples.push_back(static_cast<float>(value) / 32768.0f);
        } else if (bits_per_sample == 32) {
            float value;
            in >> value;
            if (channels == 2) {
                float right;
                in >> right;
                value = (value + right) / 2.0f;
            }
            samples.push_back(value);
        } else {
            std::cerr << "Unsupported bits per sample: " << bits_per_sample << std::endl;
            return false;
        }
    }

    std::cout << "Loaded WAV file: " << file_path.toStdString() << std::endl;
    std::cout << "  Sample rate: " << sample_rate << " Hz" << std::endl;
    std::cout << "  Channels: " << channels << std::endl;
    std::cout << "  Bits per sample: " << bits_per_sample << std::endl;
    std::cout << "  Number of samples: " << samples.size() << std::endl;

    return true;
}

void OfflineRecognitionWorker::ProcessWithVAD(const std::vector<float> &samples, int32_t sample_rate)
{
    const int32_t window_size = 512;
    int32_t offset = 0;
    int32_t total_samples = static_cast<int32_t>(samples.size());
    int32_t processed_samples = 0;

    for (; offset < total_samples; offset += window_size) {
        if (!m_running) {
            break;
        }

        int32_t chunk_size = std::min(window_size, total_samples - offset);
        m_vad->AcceptWaveform(samples.data() + offset, chunk_size);

        while (!m_vad->IsEmpty()) {
            auto segment_samples = m_vad->GetFrontSamples();
            m_vad->Pop();

            std::string result = m_recognizer->Decode(
                static_cast<float>(sample_rate),
                segment_samples.data(),
                static_cast<int32_t>(segment_samples.size()));

            if (!result.empty()) {
                emit resultReceived(QString::fromStdString(result));
            }

            processed_samples += static_cast<int32_t>(segment_samples.size());
            emit progressChanged(processed_samples, total_samples);
        }
    }

    emit progressChanged(total_samples, total_samples);
}

OfflineRecognitionService::OfflineRecognitionService(QObject *parent)
    : QObject(parent)
    , m_running(false)
    , m_thread(nullptr)
    , m_worker(nullptr)
{
}

OfflineRecognitionService::~OfflineRecognitionService()
{
    Stop();
}

bool OfflineRecognitionService::ProcessFile(const QString &file_path, const AppConfig &config)
{
    if (m_running) {
        return false;
    }

    m_thread = new QThread();
    m_worker = new OfflineRecognitionWorker();
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &OfflineRecognitionWorker::process);
    connect(m_worker, &OfflineRecognitionWorker::progressChanged, this, &OfflineRecognitionService::progressChanged);
    connect(m_worker, &OfflineRecognitionWorker::resultReceived, this, &OfflineRecognitionService::resultReceived);
    connect(m_worker, &OfflineRecognitionWorker::finished, this, [this]() {
        emit finished();
        if (m_thread) {
            m_thread->quit();
            m_thread->wait();
            delete m_worker;
            m_worker = nullptr;
            delete m_thread;
            m_thread = nullptr;
        }
        m_running = false;
    });
    connect(m_worker, &OfflineRecognitionWorker::errorOccurred, this, &OfflineRecognitionService::errorOccurred);

    m_worker->setData(file_path, config);
    m_running = true;
    m_thread->start();

    return true;
}

void OfflineRecognitionService::Stop()
{
    if (m_worker) {
        m_worker->stop();
    }
}
