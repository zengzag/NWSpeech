#include "core/speech_recognizer.h"
#include <iostream>

SpeechRecognizer::SpeechRecognizer()
    : initialized_(false), is_streaming_(false) {
}

SpeechRecognizer::~SpeechRecognizer() {
}

bool SpeechRecognizer::Initialize(const ModelConfig& config) {
    if (config.type == ModelType::kStreamingZipformer) {
        OnlineRecognizerConfig online_config;

        online_config.model_config.transducer.encoder = config.encoder_file;
        online_config.model_config.transducer.decoder = config.decoder_file;
        online_config.model_config.transducer.joiner = config.joiner_file;
        online_config.model_config.tokens = config.tokens_file;
        online_config.model_config.model_type = "zipformer2";
        online_config.model_config.num_threads = 4;
        online_config.model_config.provider = "cpu";

        std::cout << "Loading Streaming Zipformer model...\n";
        std::cout << "  Encoder: " << config.encoder_file << "\n";
        std::cout << "  Decoder: " << config.decoder_file << "\n";
        std::cout << "  Joiner: " << config.joiner_file << "\n";
        std::cout << "  Tokens: " << config.tokens_file << "\n";

        online_recognizer_ = std::make_unique<OnlineRecognizer>(OnlineRecognizer::Create(online_config));
        if (!online_recognizer_->Get()) {
            std::cerr << "Failed to create online speech recognizer\n";
            return false;
        }
        std::cout << "Streaming Zipformer model loaded\n";
        is_streaming_ = true;
    } else {
        OfflineRecognizerConfig offline_config;

        if (config.type == ModelType::kFireRedAsrCtc) {
            offline_config.model_config.fire_red_asr_ctc.model = config.ctc_model_file;
            offline_config.model_config.tokens = config.tokens_file;
            std::cout << "Loading FireRed ASR CTC model...\n";
        } else if (config.type == ModelType::kFireRedAsr) {
            offline_config.model_config.fire_red_asr.encoder = config.encoder_file;
            offline_config.model_config.fire_red_asr.decoder = config.decoder_file;
            offline_config.model_config.tokens = config.tokens_file;
            std::cout << "Loading FireRed ASR model...\n";
        } else {
            std::cerr << "Unknown model type\n";
            return false;
        }

        offline_config.model_config.num_threads = 8;
        offline_config.model_config.provider = "cpu";

        offline_recognizer_ = std::make_unique<OfflineRecognizer>(OfflineRecognizer::Create(offline_config));
        if (!offline_recognizer_->Get()) {
            std::cerr << "Failed to create offline speech recognizer\n";
            return false;
        }
        std::cout << "Offline speech recognition model loaded\n";
        is_streaming_ = false;
    }

    initialized_ = true;
    return true;
}

std::string SpeechRecognizer::Decode(float sample_rate, const float* samples, int32_t num_samples) {
    if (!initialized_) {
        return "";
    }

    if (is_streaming_) {
        std::cerr << "Use streaming-specific methods for streaming models\n";
        return "";
    }

    if (!offline_recognizer_) {
        return "";
    }

    OfflineStream stream = offline_recognizer_->CreateStream();
    stream.AcceptWaveform(sample_rate, samples, num_samples);

    offline_recognizer_->Decode(&stream);
    OfflineRecognizerResult result = offline_recognizer_->GetResult(&stream);
    return result.text;
}

void SpeechRecognizer::CreateStream() {
    if (!initialized_ || !is_streaming_ || !online_recognizer_) {
        return;
    }
    online_stream_ = std::make_unique<OnlineStream>(online_recognizer_->CreateStream());
}

void SpeechRecognizer::AcceptWaveform(float sample_rate, const float* samples, int32_t num_samples) {
    if (!initialized_ || !is_streaming_ || !online_stream_) {
        return;
    }
    online_stream_->AcceptWaveform(sample_rate, samples, num_samples);
}

void SpeechRecognizer::InputFinished() {
    if (!initialized_ || !is_streaming_ || !online_stream_) {
        return;
    }
    online_stream_->InputFinished();
}

bool SpeechRecognizer::IsReady() {
    if (!initialized_ || !is_streaming_ || !online_recognizer_ || !online_stream_) {
        return false;
    }
    return online_recognizer_->IsReady(online_stream_.get());
}

void SpeechRecognizer::Decode() {
    if (!initialized_ || !is_streaming_ || !online_recognizer_ || !online_stream_) {
        return;
    }
    online_recognizer_->Decode(online_stream_.get());
}

std::string SpeechRecognizer::GetResult() {
    if (!initialized_ || !is_streaming_ || !online_recognizer_ || !online_stream_) {
        return "";
    }
    OnlineRecognizerResult result = online_recognizer_->GetResult(online_stream_.get());
    return result.text;
}

void SpeechRecognizer::Reset() {
    if (is_streaming_) {
        online_stream_.reset();
    }
}
