#include "core/audio_resampler.h"
#include <iostream>
#include <algorithm>

AudioResampler::AudioResampler()
    : initialized_(false) {
}

AudioResampler::~AudioResampler() {
}

bool AudioResampler::Initialize(float input_sample_rate, float output_sample_rate) {
    float min_freq = (std::min)(input_sample_rate, output_sample_rate);
    float lowpass_cutoff = 0.99f * 0.5f * min_freq;
    int32_t lowpass_filter_width = 6;

    resampler_ = std::make_unique<LinearResampler>(LinearResampler::Create(
        input_sample_rate, output_sample_rate,
        lowpass_cutoff, lowpass_filter_width));

    if (!resampler_->Get()) {
        std::cerr << "Failed to create resampler\n";
        return false;
    }

    initialized_ = true;
    return true;
}

std::vector<float> AudioResampler::Resample(const float* samples, int32_t num_samples) {
    if (!initialized_ || !resampler_ || !resampler_->Get()) {
        return std::vector<float>(samples, samples + num_samples);
    }

    return resampler_->Resample(samples, num_samples, false);
}
