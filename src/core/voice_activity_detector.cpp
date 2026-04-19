#include "core/voice_activity_detector.h"
#include <iostream>

VoiceActivityDetectorWrapper::VoiceActivityDetectorWrapper()
    : initialized_(false) {
}

VoiceActivityDetectorWrapper::~VoiceActivityDetectorWrapper() {
}

bool VoiceActivityDetectorWrapper::Initialize(const std::string& vad_model_path, float threshold,
                                               float min_silence, float min_speech, float max_speech) {
    VadModelConfig config;
    config.silero_vad.model = vad_model_path;
    config.silero_vad.threshold = threshold;
    config.silero_vad.min_silence_duration = min_silence;
    config.silero_vad.min_speech_duration = min_speech;
    config.silero_vad.max_speech_duration = max_speech;
    config.sample_rate = 16000;
    config.debug = true;

    std::cout << "Loading VAD model...\n";
    vad_ = std::make_unique<VoiceActivityDetector>(VoiceActivityDetector::Create(config, 20));
    if (!vad_->Get()) {
        std::cerr << "Failed to create VAD\n";
        return false;
    }
    std::cout << "VAD model loaded\n";

    initialized_ = true;
    return true;
}

void VoiceActivityDetectorWrapper::AcceptWaveform(const float* samples, int32_t num_samples) {
    if (initialized_ && vad_) {
        vad_->AcceptWaveform(samples, num_samples);
    }
}

bool VoiceActivityDetectorWrapper::IsDetected() const {
    return initialized_ && vad_ && vad_->IsDetected();
}

bool VoiceActivityDetectorWrapper::IsEmpty() const {
    return !initialized_ || !vad_ || vad_->IsEmpty();
}

void VoiceActivityDetectorWrapper::Pop() {
    if (initialized_ && vad_) {
        vad_->Pop();
    }
}

std::vector<float> VoiceActivityDetectorWrapper::GetFrontSamples() const {
    if (!initialized_ || !vad_) {
        return {};
    }
    auto segment = vad_->Front();
    return segment.samples;
}
