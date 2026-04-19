#ifndef AUDIO_RESAMPLER_H
#define AUDIO_RESAMPLER_H

#include <string>
#include <vector>
#include <memory>
#include "sherpa-onnx/c-api/cxx-api.h"

using namespace sherpa_onnx::cxx;

class AudioResampler {
public:
    AudioResampler();
    ~AudioResampler();

    bool Initialize(float input_sample_rate, float output_sample_rate);
    std::vector<float> Resample(const float* samples, int32_t num_samples);

private:
    std::unique_ptr<LinearResampler> resampler_;
    bool initialized_;
};

#endif // AUDIO_RESAMPLER_H
