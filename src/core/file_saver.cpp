#include "core/file_saver.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

FileSaver::FileSaver()
    : save_text_(true)
    , save_audio_(true)
    , initialized_(false) {
}

FileSaver::~FileSaver() {
    Close();
}

bool FileSaver::Initialize(const std::string& output_dir, AudioSource audio_source, bool save_text, bool save_audio) {
    output_dir_ = output_dir;
    save_text_ = save_text;
    save_audio_ = save_audio;

    std::string timestamp = GetTimestamp();

    if (save_text_) {
        if (audio_source == AudioSource::kSystemAudio || audio_source == AudioSource::kBoth) {
            std::string system_text_path = output_dir_ + "/" + timestamp + "_recognition_system.txt";
            text_file_paths_[AudioSourceTag::kSystem] = system_text_path;
            text_files_[AudioSourceTag::kSystem].open(system_text_path, std::ios::out | std::ios::app);
            if (text_files_[AudioSourceTag::kSystem].is_open()) {
                std::cout << "System text will be saved to: " << system_text_path << std::endl;
            }
        }
        if (audio_source == AudioSource::kMicrophone || audio_source == AudioSource::kBoth) {
            std::string mic_text_path = output_dir_ + "/" + timestamp + "_recognition_mic.txt";
            text_file_paths_[AudioSourceTag::kMicrophone] = mic_text_path;
            text_files_[AudioSourceTag::kMicrophone].open(mic_text_path, std::ios::out | std::ios::app);
            if (text_files_[AudioSourceTag::kMicrophone].is_open()) {
                std::cout << "Mic text will be saved to: " << mic_text_path << std::endl;
            }
        }
    }

    if (save_audio_) {
        if (audio_source == AudioSource::kSystemAudio || audio_source == AudioSource::kBoth) {
            audio_writers_[AudioSourceTag::kSystem].audio_file_path = output_dir_ + "/" + timestamp + "_audio_system.wav";
            std::cout << "System audio will be saved to: " << audio_writers_[AudioSourceTag::kSystem].audio_file_path << std::endl;
        }
        if (audio_source == AudioSource::kMicrophone || audio_source == AudioSource::kBoth) {
            audio_writers_[AudioSourceTag::kMicrophone].audio_file_path = output_dir_ + "/" + timestamp + "_audio_mic.wav";
            std::cout << "Mic audio will be saved to: " << audio_writers_[AudioSourceTag::kMicrophone].audio_file_path << std::endl;
        }
    }

    initialized_ = true;
    return true;
}

std::string FileSaver::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &now_time);
#else
    localtime_r(&now_time, &local_tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

void FileSaver::SaveText(const std::string& text, AudioSourceTag source) {
    if (!initialized_ || !save_text_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = text_files_.find(source);
    if (it != text_files_.end() && it->second.is_open()) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);

        std::tm local_tm;
#ifdef _WIN32
        localtime_s(&local_tm, &now_time);
#else
        localtime_r(&now_time, &local_tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << " - " << text << std::endl;

        it->second << oss.str();
        it->second.flush();
    }
}

void FileSaver::SaveAudio(const std::vector<float>& samples, int32_t sample_rate, AudioSourceTag source) {
    if (!initialized_ || !save_audio_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = audio_writers_.find(source);
    if (it == audio_writers_.end()) {
        return;
    }

    AudioFileWriter& writer = it->second;

    if (!writer.audio_file.is_open()) {
        writer.audio_file.open(writer.audio_file_path, std::ios::binary | std::ios::out);
        if (!writer.audio_file.is_open()) {
            std::cerr << "Failed to open audio file for writing: " << writer.audio_file_path << std::endl;
            return;
        }
        WriteWavHeader(writer.audio_file, sample_rate);
    }

    for (float sample : samples) {
        int16_t pcm_sample = static_cast<int16_t>(sample * 32767.0f);
        writer.audio_file.write(reinterpret_cast<const char*>(&pcm_sample), sizeof(int16_t));
    }

    writer.total_samples += static_cast<int64_t>(samples.size());
}

void FileSaver::WriteWavHeader(std::ofstream& file, int32_t sample_rate) {
    int32_t bytes_per_sample = 2;
    int32_t placeholder_size = 0;

    WavHeader header;
    header.riff[0] = 'R';
    header.riff[1] = 'I';
    header.riff[2] = 'F';
    header.riff[3] = 'F';
    header.file_size = 36 + placeholder_size;
    header.wave[0] = 'W';
    header.wave[1] = 'A';
    header.wave[2] = 'V';
    header.wave[3] = 'E';
    header.fmt[0] = 'f';
    header.fmt[1] = 'm';
    header.fmt[2] = 't';
    header.fmt[3] = ' ';
    header.fmt_size = 16;
    header.format_tag = 1;
    header.channels = 1;
    header.sample_rate = sample_rate;
    header.bytes_per_sec = sample_rate * bytes_per_sample;
    header.block_align = bytes_per_sample;
    header.bits_per_sample = bytes_per_sample * 8;
    header.data[0] = 'd';
    header.data[1] = 'a';
    header.data[2] = 't';
    header.data[3] = 'a';
    header.data_size = placeholder_size;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

void FileSaver::UpdateWavHeader(AudioFileWriter& writer) {
    if (!writer.audio_file.is_open()) {
        return;
    }

    int32_t bytes_per_sample = 2;
    int32_t data_size = static_cast<int32_t>(writer.total_samples) * bytes_per_sample;

    writer.audio_file.seekp(4);
    uint32_t file_size = 36 + data_size;
    writer.audio_file.write(reinterpret_cast<const char*>(&file_size), sizeof(file_size));

    writer.audio_file.seekp(40);
    writer.audio_file.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
}

void FileSaver::Flush() {
    if (!initialized_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (save_text_) {
        for (auto& pair : text_files_) {
            if (pair.second.is_open()) {
                pair.second.flush();
            }
        }
    }

    if (save_audio_) {
        for (auto& pair : audio_writers_) {
            if (pair.second.audio_file.is_open()) {
                pair.second.audio_file.flush();
            }
        }
    }
}

void FileSaver::Close() {
    if (!initialized_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (save_text_) {
        for (auto& pair : text_files_) {
            if (pair.second.is_open()) {
                pair.second.close();
                std::cout << "Text file closed: " << text_file_paths_[pair.first] << std::endl;
            }
        }
    }

    if (save_audio_) {
        for (auto& pair : audio_writers_) {
            UpdateWavHeader(pair.second);
            if (pair.second.audio_file.is_open()) {
                pair.second.audio_file.close();
                std::cout << "Audio file closed: " << pair.second.audio_file_path << " (" << pair.second.total_samples << " samples)" << std::endl;
            }
        }
    }

    initialized_ = false;
}
