#ifndef FILE_SAVER_H
#define FILE_SAVER_H

#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <chrono>
#include <map>
#include "config.h"

struct WavHeader {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t format_tag;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t bytes_per_sec;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
};

struct AudioFileWriter {
    std::ofstream audio_file;
    std::string audio_file_path;
    int32_t sample_rate;
    int64_t total_samples;
    bool is_open;

    AudioFileWriter() : sample_rate(16000), total_samples(0), is_open(false) {}
};

class FileSaver {
public:
    FileSaver();
    ~FileSaver();

    bool Initialize(const std::string& output_dir, AudioSource audio_source, bool save_text = true, bool save_audio = true);
    void SaveText(const std::string& text, AudioSourceTag source);
    void SaveAudio(const std::vector<float>& samples, int32_t sample_rate, AudioSourceTag source);
    void Flush();
    void Close();

private:
    std::string GetTimestamp();
    void WriteWavHeader(std::ofstream& file, int32_t sample_rate);
    void UpdateWavHeader(AudioFileWriter& writer);

    std::string output_dir_;
    std::map<AudioSourceTag, std::string> text_file_paths_;
    std::map<AudioSourceTag, std::ofstream> text_files_;

    std::map<AudioSourceTag, AudioFileWriter> audio_writers_;

    bool save_text_;
    bool save_audio_;
    bool initialized_;

    std::mutex mutex_;
};

#endif // FILE_SAVER_H
