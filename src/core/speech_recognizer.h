#ifndef SPEECH_RECOGNIZER_H
#define SPEECH_RECOGNIZER_H

#include <string>
#include <vector>
#include <memory>
#include "sherpa-onnx/c-api/cxx-api.h"
#include "config.h"

using namespace sherpa_onnx::cxx;

class SpeechRecognizer {
public:
    SpeechRecognizer();
    ~SpeechRecognizer();

    bool Initialize(const ModelConfig& config);
    std::string Decode(float sample_rate, const float* samples, int32_t num_samples);

    bool IsStreaming() const { return is_streaming_; }
    void CreateStream();
    void AcceptWaveform(float sample_rate, const float* samples, int32_t num_samples);
    void InputFinished();
    bool IsReady();
    void Decode();
    std::string GetResult();
    void Reset();

private:
    std::unique_ptr<OfflineRecognizer> offline_recognizer_;
    std::unique_ptr<OnlineRecognizer> online_recognizer_;
    std::unique_ptr<OnlineStream> online_stream_;
    bool initialized_;
    bool is_streaming_;
};

#endif // SPEECH_RECOGNIZER_H
