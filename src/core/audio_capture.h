#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include "config.h"
#include <windows.h>
#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <Endpointvolume.h>
#include <ks.h>
#include <ksmedia.h>
#include <functiondiscoverykeys_devpkey.h>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")

class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    bool Start(AudioSource source, const std::string& device_id = "");
    void Stop();
    bool GetSamples(std::vector<float>& samples);
    int32_t GetSampleRate() const { return sample_rate_; }

    static std::vector<AudioDeviceInfo> GetMicrophoneDevices();

private:
    static DWORD WINAPI CaptureThreadProc(LPVOID lpParam);
    void CaptureLoop();
    void Cleanup();

    std::queue<std::vector<float>> samples_queue_;
    std::mutex mutex_;
    std::condition_variable condition_variable_;
    std::atomic<bool> stop_;
    HANDLE capture_thread_;
    bool com_initialized_;

    IMMDeviceEnumerator* enumerator_;
    IMMDevice* device_;
    IAudioClient* audio_client_;
    IAudioCaptureClient* capture_client_;
    WAVEFORMATEX* wave_format_;

    int32_t sample_rate_;
};

#endif // AUDIO_CAPTURE_H
