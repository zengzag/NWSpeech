#ifndef CONFIG_H
#define CONFIG_H

#include <string>

enum class ModelType {
    kFireRedAsrCtc,
    kFireRedAsr,
    kStreamingZipformer
};

enum class AudioSource {
    kSystemAudio,
    kMicrophone,
    kBoth
};

enum class AudioSourceTag {
    kSystem,
    kMicrophone
};

struct ModelConfig {
    ModelType type;
    std::string model_dir;
    std::string tokens_file;
    std::string ctc_model_file;
    std::string encoder_file;
    std::string decoder_file;
    std::string joiner_file;
};

struct SubtitleWindowConfig {
    int window_width;
    int window_height;
    int font_size;
    std::string text_color;
    std::string background_color;
    int background_opacity;
};

struct RecognitionModeConfig {
    ModelConfig model_config;
    float vad_threshold;
    float min_silence_duration;
    float min_speech_duration;
    float max_speech_duration;
};

struct AudioDeviceInfo {
    std::string id;
    std::string name;
};

struct AppConfig {
    std::string models_base_path;
    std::string vad_model_path;
    int32_t sample_rate;
    std::string output_dir;
    bool save_text;
    bool save_audio;
    AudioSource audio_source;
    std::string microphone_device_id;
    SubtitleWindowConfig subtitle_config;
    RecognitionModeConfig realtime_config;
    RecognitionModeConfig offline_config;
    bool floating_window_visible;
};

void InitializeModelPaths(AppConfig &config);
void InitializeModelConfig(ModelConfig &model_config, const std::string &models_base_path);

AppConfig CreateDefaultConfig();
void SaveConfig(const AppConfig &config, const std::string &file_path);
AppConfig LoadConfig(const std::string &file_path);

#endif // CONFIG_H
