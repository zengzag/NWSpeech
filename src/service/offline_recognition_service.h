#ifndef OFFLINE_RECOGNITION_SERVICE_H
#define OFFLINE_RECOGNITION_SERVICE_H

#include <QObject>
#include <QString>
#include <QThread>
#include <vector>
#include <memory>
#include "config.h"
#include "core/speech_recognizer.h"
#include "core/voice_activity_detector.h"
#include "core/audio_resampler.h"

class OfflineRecognitionWorker : public QObject
{
    Q_OBJECT

public:
    explicit OfflineRecognitionWorker(QObject *parent = nullptr);
    ~OfflineRecognitionWorker();

    void setData(const QString &file_path, const AppConfig &config);
    void stop();

public slots:
    void process();

signals:
    void progressChanged(int progress, int total);
    void resultReceived(const QString &text);
    void finished();
    void errorOccurred(const QString &error);

private:
    bool LoadWavFile(const QString &file_path, std::vector<float> &samples, int32_t &sample_rate);
    void ProcessWithVAD(const std::vector<float> &samples, int32_t sample_rate);

    QString m_filePath;
    AppConfig m_config;
    std::unique_ptr<VoiceActivityDetectorWrapper> m_vad;
    std::unique_ptr<SpeechRecognizer> m_recognizer;
    std::unique_ptr<AudioResampler> m_resampler;

    std::atomic<bool> m_running;
};

class OfflineRecognitionService : public QObject
{
    Q_OBJECT

public:
    explicit OfflineRecognitionService(QObject *parent = nullptr);
    ~OfflineRecognitionService();

    bool ProcessFile(const QString &file_path, const AppConfig &config);
    void Stop();
    bool IsRunning() const { return m_running; }

signals:
    void progressChanged(int progress, int total);
    void resultReceived(const QString &text);
    void finished();
    void errorOccurred(const QString &error);

private:
    QThread *m_thread;
    OfflineRecognitionWorker *m_worker;
    std::atomic<bool> m_running;
};

#endif // OFFLINE_RECOGNITION_SERVICE_H
