#include "config.h"
#include <QSettings>
#include <QFileInfo>
#include <QCoreApplication>

void InitializeModelConfig(ModelConfig &model_config, const std::string &models_base_path) {
    if (model_config.type == ModelType::kFireRedAsrCtc) {
        model_config.model_dir = models_base_path + "/fire-red-asr2-ctc";
        model_config.tokens_file = model_config.model_dir + "/tokens.txt";
        model_config.ctc_model_file = model_config.model_dir + "/model.onnx";
    } else if (model_config.type == ModelType::kFireRedAsr) {
        model_config.model_dir = models_base_path + "/fire-red-asr2";
        model_config.tokens_file = model_config.model_dir + "/tokens.txt";
        model_config.encoder_file = model_config.model_dir + "/encoder.onnx";
        model_config.decoder_file = model_config.model_dir + "/decoder.onnx";
    } else if (model_config.type == ModelType::kStreamingZipformer) {
        model_config.model_dir = models_base_path + "/streaming-zipformer";
        model_config.tokens_file = model_config.model_dir + "/tokens.txt";
        model_config.encoder_file = model_config.model_dir + "/encoder.onnx";
        model_config.decoder_file = model_config.model_dir + "/decoder.onnx";
        model_config.joiner_file = model_config.model_dir + "/joiner.onnx";
    }
}

void InitializeModelPaths(AppConfig &config) {
    QString appDir = QCoreApplication::applicationDirPath();
    config.models_base_path = (appDir + "/models").toStdString();
    config.vad_model_path = config.models_base_path + "/silero_vad.onnx";
    
    InitializeModelConfig(config.realtime_config.model_config, config.models_base_path);
    InitializeModelConfig(config.offline_config.model_config, config.models_base_path);
}

AppConfig CreateDefaultConfig() {
    AppConfig config;
    
    QString appDir = QCoreApplication::applicationDirPath();
    config.models_base_path = (appDir + "/models").toStdString();
    config.vad_model_path = config.models_base_path + "/silero_vad.onnx";
    config.sample_rate = 16000;
    
    config.realtime_config.model_config.type = ModelType::kStreamingZipformer;
    InitializeModelConfig(config.realtime_config.model_config, config.models_base_path);
    config.realtime_config.vad_threshold = 0.3f;
    config.realtime_config.min_silence_duration = 0.3f;
    config.realtime_config.min_speech_duration = 0.1f;
    config.realtime_config.max_speech_duration = 10.0f;
    
    config.offline_config.model_config.type = ModelType::kStreamingZipformer;
    InitializeModelConfig(config.offline_config.model_config, config.models_base_path);
    config.offline_config.vad_threshold = 0.3f;
    config.offline_config.min_silence_duration = 0.5f;
    config.offline_config.min_speech_duration = 0.1f;
    config.offline_config.max_speech_duration = 30.0f;
    
    config.output_dir = "";
    config.save_text = true;
    config.save_audio = true;
    config.audio_source = AudioSource::kSystemAudio;
    config.microphone_device_id = "";
    
    config.subtitle_config.window_width = 500;
    config.subtitle_config.window_height = 80;
    config.subtitle_config.font_size = 16;
    config.subtitle_config.text_color = "#FFFFFF";
    config.subtitle_config.background_color = "#000000";
    config.subtitle_config.background_opacity = 180;

    config.floating_window_visible = false;

    config.llm_optimizer_config.enabled = false;
    config.llm_optimizer_config.api_url = "http://127.0.0.1:1234/v1";
    config.llm_optimizer_config.model_name = "gemma-4-e2b-it";
    config.llm_optimizer_config.api_key = "default_key";
    config.llm_optimizer_config.context_sentences = 3;
    config.llm_optimizer_config.prompt = "你是一个语音识别结果优化助手。你的任务是：\n1. 修正识别中的错词、别字\n2. 添加合适的标点符号\n3. 适当断句，使语句通顺\n4. 保持原意不变\n\n请只返回优化后的当前句子，不要添加其他说明。\n\n上下文历史：\n{context}\n当前句子：\n{current}\n\n优化结果：";
    
    return config;
}

void SaveConfig(const AppConfig &config, const std::string &file_path) {
    QSettings settings(QString::fromStdString(file_path), QSettings::IniFormat);

    settings.beginGroup("General");
    settings.setValue("sample_rate", config.sample_rate);
    settings.setValue("output_dir", QString::fromStdString(config.output_dir));
    settings.setValue("save_text", config.save_text);
    settings.setValue("save_audio", config.save_audio);
    settings.setValue("audio_source", static_cast<int>(config.audio_source));
    settings.setValue("microphone_device_id", QString::fromStdString(config.microphone_device_id));
    settings.endGroup();

    settings.beginGroup("Realtime");
    settings.setValue("model_type", static_cast<int>(config.realtime_config.model_config.type));
    settings.setValue("vad_threshold", config.realtime_config.vad_threshold);
    settings.setValue("min_silence_duration", config.realtime_config.min_silence_duration);
    settings.setValue("min_speech_duration", config.realtime_config.min_speech_duration);
    settings.setValue("max_speech_duration", config.realtime_config.max_speech_duration);
    settings.endGroup();

    settings.beginGroup("Offline");
    settings.setValue("model_type", static_cast<int>(config.offline_config.model_config.type));
    settings.setValue("vad_threshold", config.offline_config.vad_threshold);
    settings.setValue("min_silence_duration", config.offline_config.min_silence_duration);
    settings.setValue("min_speech_duration", config.offline_config.min_speech_duration);
    settings.setValue("max_speech_duration", config.offline_config.max_speech_duration);
    settings.endGroup();

    settings.beginGroup("Subtitle");
    settings.setValue("window_width", config.subtitle_config.window_width);
    settings.setValue("window_height", config.subtitle_config.window_height);
    settings.setValue("font_size", config.subtitle_config.font_size);
    settings.setValue("text_color", QString::fromStdString(config.subtitle_config.text_color));
    settings.setValue("background_color", QString::fromStdString(config.subtitle_config.background_color));
    settings.setValue("background_opacity", config.subtitle_config.background_opacity);
    settings.endGroup();

    settings.beginGroup("UI");
    settings.setValue("floating_window_visible", config.floating_window_visible);
    settings.endGroup();

    settings.beginGroup("LLMOptimizer");
    settings.setValue("enabled", config.llm_optimizer_config.enabled);
    settings.setValue("api_url", QString::fromStdString(config.llm_optimizer_config.api_url));
    settings.setValue("model_name", QString::fromStdString(config.llm_optimizer_config.model_name));
    settings.setValue("api_key", QString::fromStdString(config.llm_optimizer_config.api_key));
    settings.setValue("context_sentences", config.llm_optimizer_config.context_sentences);
    settings.setValue("prompt", QString::fromStdString(config.llm_optimizer_config.prompt));
    settings.endGroup();

    settings.sync();
}

AppConfig LoadConfig(const std::string &file_path) {
    AppConfig config = CreateDefaultConfig();

    QFileInfo fileInfo(QString::fromStdString(file_path));
    if (!fileInfo.exists()) {
        return config;
    }

    QSettings settings(QString::fromStdString(file_path), QSettings::IniFormat);

    settings.beginGroup("General");
    if (settings.contains("sample_rate")) {
        config.sample_rate = settings.value("sample_rate").toInt();
    }
    if (settings.contains("output_dir")) {
        config.output_dir = settings.value("output_dir").toString().toStdString();
    }
    if (settings.contains("save_text")) {
        config.save_text = settings.value("save_text").toBool();
    }
    if (settings.contains("save_audio")) {
        config.save_audio = settings.value("save_audio").toBool();
    }
    if (settings.contains("audio_source")) {
        config.audio_source = static_cast<AudioSource>(settings.value("audio_source").toInt());
    }
    if (settings.contains("microphone_device_id")) {
        config.microphone_device_id = settings.value("microphone_device_id").toString().toStdString();
    }
    settings.endGroup();

    settings.beginGroup("Realtime");
    if (settings.contains("model_type")) {
        config.realtime_config.model_config.type = static_cast<ModelType>(settings.value("model_type").toInt());
    }
    if (settings.contains("vad_threshold")) {
        config.realtime_config.vad_threshold = settings.value("vad_threshold").toFloat();
    }
    if (settings.contains("min_silence_duration")) {
        config.realtime_config.min_silence_duration = settings.value("min_silence_duration").toFloat();
    }
    if (settings.contains("min_speech_duration")) {
        config.realtime_config.min_speech_duration = settings.value("min_speech_duration").toFloat();
    }
    if (settings.contains("max_speech_duration")) {
        config.realtime_config.max_speech_duration = settings.value("max_speech_duration").toFloat();
    }
    settings.endGroup();

    settings.beginGroup("Offline");
    if (settings.contains("model_type")) {
        config.offline_config.model_config.type = static_cast<ModelType>(settings.value("model_type").toInt());
    }
    if (settings.contains("vad_threshold")) {
        config.offline_config.vad_threshold = settings.value("vad_threshold").toFloat();
    }
    if (settings.contains("min_silence_duration")) {
        config.offline_config.min_silence_duration = settings.value("min_silence_duration").toFloat();
    }
    if (settings.contains("min_speech_duration")) {
        config.offline_config.min_speech_duration = settings.value("min_speech_duration").toFloat();
    }
    if (settings.contains("max_speech_duration")) {
        config.offline_config.max_speech_duration = settings.value("max_speech_duration").toFloat();
    }
    settings.endGroup();

    settings.beginGroup("Subtitle");
    if (settings.contains("window_width")) {
        config.subtitle_config.window_width = settings.value("window_width").toInt();
    }
    if (settings.contains("window_height")) {
        config.subtitle_config.window_height = settings.value("window_height").toInt();
    }
    if (settings.contains("font_size")) {
        config.subtitle_config.font_size = settings.value("font_size").toInt();
    }
    if (settings.contains("text_color")) {
        config.subtitle_config.text_color = settings.value("text_color").toString().toStdString();
    }
    if (settings.contains("background_color")) {
        config.subtitle_config.background_color = settings.value("background_color").toString().toStdString();
    }
    if (settings.contains("background_opacity")) {
        config.subtitle_config.background_opacity = settings.value("background_opacity").toInt();
    }
    settings.endGroup();

    settings.beginGroup("UI");
    if (settings.contains("floating_window_visible")) {
        config.floating_window_visible = settings.value("floating_window_visible").toBool();
    }
    settings.endGroup();

    settings.beginGroup("LLMOptimizer");
    if (settings.contains("enabled")) {
        config.llm_optimizer_config.enabled = settings.value("enabled").toBool();
    }
    if (settings.contains("api_url")) {
        config.llm_optimizer_config.api_url = settings.value("api_url").toString().toStdString();
    }
    if (settings.contains("model_name")) {
        config.llm_optimizer_config.model_name = settings.value("model_name").toString().toStdString();
    }
    if (settings.contains("api_key")) {
        config.llm_optimizer_config.api_key = settings.value("api_key").toString().toStdString();
    }
    if (settings.contains("context_sentences")) {
        config.llm_optimizer_config.context_sentences = settings.value("context_sentences").toInt();
    }
    if (settings.contains("prompt")) {
        config.llm_optimizer_config.prompt = settings.value("prompt").toString().toStdString();
    }
    settings.endGroup();

    InitializeModelPaths(config);

    return config;
}
