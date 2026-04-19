#ifndef VOICE_ACTIVITY_DETECTOR_H
#define VOICE_ACTIVITY_DETECTOR_H

#include <string>
#include <vector>
#include <memory>
#include "sherpa-onnx/c-api/cxx-api.h"

using namespace sherpa_onnx::cxx;

class VoiceActivityDetectorWrapper {
public:
    VoiceActivityDetectorWrapper();
    ~VoiceActivityDetectorWrapper();

    bool Initialize(const std::string& vad_model_path, float threshold = 0.3f,
                     float min_silence = 0.3f, float min_speech = 0.1f,
                     float max_speech = 20.0f);
    void AcceptWaveform(const float* samples, int32_t num_samples);
    bool IsDetected() const;
    bool IsEmpty() const;
    void Pop();
    std::vector<float> GetFrontSamples() const;

private:
    std::unique_ptr<VoiceActivityDetector> vad_;
    bool initialized_;
};

#endif // VOICE_ACTIVITY_DETECTOR_H
