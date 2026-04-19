#ifndef SPEECH_RECOGNITION_SERVICE_H
#define SPEECH_RECOGNITION_SERVICE_H

#include <QObject>
#include <QString>
#include <thread>
#include <atomic>
#include <memory>
#include "config.h"
#include "core/audio_capture.h"
#include "core/speech_recognizer.h"
#include "core/voice_activity_detector.h"
#include "core/audio_resampler.h"
#include "core/file_saver.h"

struct RecognitionPipeline {
    AudioSourceTag tag;
    AudioSource audio_source;
    std::string device_id;
    std::unique_ptr<AudioCapture> audio_capture;
    std::unique_ptr<VoiceActivityDetectorWrapper> vad;
    std::unique_ptr<SpeechRecognizer> recognizer;
    std::unique_ptr<AudioResampler> resampler;
    std::thread recognition_thread;
    std::atomic<bool> running;

    RecognitionPipeline() : running(false) {}
};

class SpeechRecognitionService : public QObject
{
    Q_OBJECT

public:
    explicit SpeechRecognitionService(QObject *parent = nullptr);
    ~SpeechRecognitionService();

    bool Start(const AppConfig &config);
    void Stop();
    bool IsRunning() const { return m_running; }

signals:
    void partialResultChanged(const QString &text, AudioSourceTag source);
    void finalResultReceived(const QString &text, AudioSourceTag source);
    void errorOccurred(const QString &error);

private:
    void recognitionThread(RecognitionPipeline *pipeline);
    void initPipeline(RecognitionPipeline *pipeline, const AppConfig &config);

    RecognitionPipeline m_systemPipeline;
    RecognitionPipeline m_micPipeline;

    std::unique_ptr<FileSaver> m_fileSaver;

    std::atomic<bool> m_running;

    AppConfig m_config;
};

#endif // SPEECH_RECOGNITION_SERVICE_H
