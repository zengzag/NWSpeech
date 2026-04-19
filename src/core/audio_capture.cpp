#include "core/audio_capture.h"
#include <iostream>

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

AudioCapture::AudioCapture()
    : enumerator_(nullptr)
    , device_(nullptr)
    , audio_client_(nullptr)
    , capture_client_(nullptr)
    , wave_format_(nullptr)
    , capture_thread_(nullptr)
    , stop_(false)
    , com_initialized_(false)
    , sample_rate_(0) {
}

AudioCapture::~AudioCapture() {
    Stop();
}

void AudioCapture::Cleanup() {
    HANDLE thread_to_wait = capture_thread_;
    capture_thread_ = NULL;

    if (thread_to_wait) {
        WaitForSingleObject(thread_to_wait, INFINITE);
        CloseHandle(thread_to_wait);
    }

    if (capture_client_) {
        capture_client_->Release();
        capture_client_ = NULL;
    }
    if (audio_client_) {
        audio_client_->Stop();
        audio_client_->Release();
        audio_client_ = NULL;
    }
    if (wave_format_) {
        CoTaskMemFree(wave_format_);
        wave_format_ = NULL;
    }
    if (device_) {
        device_->Release();
        device_ = NULL;
    }
    if (enumerator_) {
        enumerator_->Release();
        enumerator_ = NULL;
    }

    if (com_initialized_) {
        CoUninitialize();
        com_initialized_ = false;
    }
}

std::vector<AudioDeviceInfo> AudioCapture::GetMicrophoneDevices() {
    std::vector<AudioDeviceInfo> devices;
    HRESULT hr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "CoInitializeEx failed: " << hr << std::endl;
        return devices;
    }
    bool com_init = (hr == S_OK);

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        IID_IMMDeviceEnumerator,
        (void**)&enumerator);
    if (FAILED(hr)) {
        std::cerr << "CoCreateInstance failed: " << hr << std::endl;
        if (com_init) CoUninitialize();
        return devices;
    }

    IMMDeviceCollection* collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) {
        std::cerr << "EnumAudioEndpoints failed: " << hr << std::endl;
        enumerator->Release();
        if (com_init) CoUninitialize();
        return devices;
    }

    UINT count = 0;
    hr = collection->GetCount(&count);
    if (FAILED(hr)) {
        std::cerr << "GetCount failed: " << hr << std::endl;
        collection->Release();
        enumerator->Release();
        if (com_init) CoUninitialize();
        return devices;
    }

    for (UINT i = 0; i < count; i++) {
        IMMDevice* device = nullptr;
        hr = collection->Item(i, &device);
        if (FAILED(hr)) continue;

        LPWSTR device_id = nullptr;
        hr = device->GetId(&device_id);
        if (FAILED(hr)) {
            device->Release();
            continue;
        }

        IPropertyStore* property_store = nullptr;
        hr = device->OpenPropertyStore(STGM_READ, &property_store);
        if (FAILED(hr)) {
            CoTaskMemFree(device_id);
            device->Release();
            continue;
        }

        PROPVARIANT var_name;
        PropVariantInit(&var_name);
        hr = property_store->GetValue(PKEY_Device_FriendlyName, &var_name);

        std::wstring device_name_w;
        if (SUCCEEDED(hr) && var_name.vt == VT_LPWSTR) {
            device_name_w = var_name.pwszVal;
        }

        PropVariantClear(&var_name);
        property_store->Release();

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, device_id, -1, NULL, 0, NULL, NULL);
        std::string device_id_str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, device_id, -1, &device_id_str[0], size_needed, NULL, NULL);

        size_needed = WideCharToMultiByte(CP_UTF8, 0, device_name_w.c_str(), -1, NULL, 0, NULL, NULL);
        std::string device_name_str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, device_name_w.c_str(), -1, &device_name_str[0], size_needed, NULL, NULL);

        AudioDeviceInfo info;
        info.id = device_id_str;
        info.name = device_name_str;
        devices.push_back(info);

        CoTaskMemFree(device_id);
        device->Release();
    }

    collection->Release();
    enumerator->Release();
    if (com_init) CoUninitialize();

    return devices;
}

bool AudioCapture::Start(AudioSource source, const std::string& device_id) {
    HRESULT hr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "CoInitializeEx failed: " << hr << std::endl;
        return false;
    }
    com_initialized_ = (hr == S_OK);

    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        IID_IMMDeviceEnumerator,
        (void**)&enumerator_);
    if (FAILED(hr)) {
        std::cerr << "CoCreateInstance failed: " << hr << std::endl;
        Cleanup();
        return false;
    }

    if (source == AudioSource::kSystemAudio) {
        hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    } else {
        if (!device_id.empty()) {
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, NULL, 0);
            std::wstring device_id_w(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, &device_id_w[0], size_needed);

            hr = enumerator_->GetDevice(device_id_w.c_str(), &device_);
            if (FAILED(hr)) {
                std::cerr << "GetDevice failed for specified device, using default: " << hr << std::endl;
                hr = enumerator_->GetDefaultAudioEndpoint(eCapture, eConsole, &device_);
            }
        } else {
            hr = enumerator_->GetDefaultAudioEndpoint(eCapture, eConsole, &device_);
        }
    }
    if (FAILED(hr)) {
        std::cerr << "GetDefaultAudioEndpoint failed: " << hr << std::endl;
        Cleanup();
        return false;
    }

    hr = device_->Activate(
        IID_IAudioClient,
        CLSCTX_ALL,
        NULL,
        (void**)&audio_client_);
    if (FAILED(hr)) {
        std::cerr << "Activate failed: " << hr << std::endl;
        Cleanup();
        return false;
    }

    hr = audio_client_->GetMixFormat(&wave_format_);
    if (FAILED(hr)) {
        std::cerr << "GetMixFormat failed: " << hr << std::endl;
        Cleanup();
        return false;
    }

    sample_rate_ = wave_format_->nSamplesPerSec;

    std::cout << "Audio format: " << std::endl;
    std::cout << "  Sample rate: " << wave_format_->nSamplesPerSec << " Hz" << std::endl;
    std::cout << "  Channels: " << wave_format_->nChannels << std::endl;
    std::cout << "  Bits per sample: " << wave_format_->wBitsPerSample << std::endl;
    std::cout << "  Format tag: " << wave_format_->wFormatTag << std::endl;
    std::cout << "  Block align: " << wave_format_->nBlockAlign << std::endl;

    if (wave_format_->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE* pWfxExt = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wave_format_);
        std::cout << "  Extensible - Valid bits: " << pWfxExt->Samples.wValidBitsPerSample << std::endl;
        std::cout << "  Extensible - Container bits: " << pWfxExt->Format.wBitsPerSample << std::endl;
    }

    DWORD streamFlags = 0;
    if (source == AudioSource::kSystemAudio) {
        streamFlags = AUDCLNT_STREAMFLAGS_LOOPBACK;
    }

    hr = audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        streamFlags,
        10000000,
        0,
        wave_format_,
        NULL);
    if (FAILED(hr)) {
        std::cerr << "Initialize failed: " << hr << std::endl;
        Cleanup();
        return false;
    }

    UINT32 bufferFrameCount;
    hr = audio_client_->GetBufferSize(&bufferFrameCount);
    if (FAILED(hr)) {
        std::cerr << "GetBufferSize failed: " << hr << std::endl;
        Cleanup();
        return false;
    }

    hr = audio_client_->GetService(
        IID_IAudioCaptureClient,
        (void**)&capture_client_);
    if (FAILED(hr)) {
        std::cerr << "GetService failed: " << hr << std::endl;
        Cleanup();
        return false;
    }

    stop_ = false;
    capture_thread_ = CreateThread(NULL, 0, CaptureThreadProc, this, 0, NULL);
    if (capture_thread_ == NULL) {
        std::cerr << "CreateThread failed: " << GetLastError() << std::endl;
        Cleanup();
        return false;
    }

    return true;
}

void AudioCapture::Stop() {
    if (!stop_) {
        stop_ = true;
        condition_variable_.notify_all();
        Cleanup();
    }
}

bool AudioCapture::GetSamples(std::vector<float>& samples) {
    std::unique_lock<std::mutex> lock(mutex_);
    while (samples_queue_.empty() && !stop_) {
        if (!condition_variable_.wait_for(lock, std::chrono::milliseconds(100), [this] { return !samples_queue_.empty() || stop_; })) {
            if (stop_) {
                return false;
            }
        }
    }

    if (stop_) {
        return false;
    }

    if (samples_queue_.empty()) {
        return false;
    }

    samples = std::move(samples_queue_.front());
    samples_queue_.pop();
    return true;
}

DWORD WINAPI AudioCapture::CaptureThreadProc(LPVOID lpParam) {
    AudioCapture* capture = static_cast<AudioCapture*>(lpParam);
    capture->CaptureLoop();
    return 0;
}

void AudioCapture::CaptureLoop() {
    HRESULT hr;
    UINT32 numFramesAvailable;
    DWORD flags;
    BYTE* pData;
    UINT32 framesToRead;

    hr = audio_client_->Start();
    if (FAILED(hr)) {
        std::cerr << "Start failed: " << hr << std::endl;
        return;
    }

    while (!stop_) {
        Sleep(10);

        hr = capture_client_->GetNextPacketSize(&numFramesAvailable);
        if (FAILED(hr)) {
            std::cerr << "GetNextPacketSize failed: " << hr << std::endl;
            break;
        }

        while (numFramesAvailable > 0 && !stop_) {
            hr = capture_client_->GetBuffer(
                &pData,
                &framesToRead,
                &flags,
                NULL,
                NULL);
            if (FAILED(hr)) {
                std::cerr << "GetBuffer failed: " << hr << std::endl;
                break;
            }

            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                std::vector<float> samples(framesToRead);
                UINT32 channels = wave_format_->nChannels;

                bool is_extensible = (wave_format_->wFormatTag == WAVE_FORMAT_EXTENSIBLE);
                bool is_ieee_float = false;
                bool is_pcm = false;

                if (is_extensible) {
                    WAVEFORMATEXTENSIBLE* pWfxExt = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wave_format_);
                    if (pWfxExt->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
                        is_ieee_float = true;
                    } else if (pWfxExt->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
                        is_pcm = true;
                    }
                } else {
                    if (wave_format_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
                        is_ieee_float = true;
                    } else if (wave_format_->wFormatTag == WAVE_FORMAT_PCM) {
                        is_pcm = true;
                    }
                }

                if (is_ieee_float) {
                    float* pFloatData = reinterpret_cast<float*>(pData);
                    for (UINT32 i = 0; i < framesToRead; i++) {
                        float sum = 0.0f;
                        for (UINT32 ch = 0; ch < channels; ch++) {
                            sum += pFloatData[i * channels + ch];
                        }
                        samples[i] = sum / static_cast<float>(channels);
                    }
                } else if (is_pcm) {
                    if (wave_format_->wBitsPerSample == 16) {
                        int16_t* pInt16Data = reinterpret_cast<int16_t*>(pData);
                        for (UINT32 i = 0; i < framesToRead; i++) {
                            float sum = 0.0f;
                            for (UINT32 ch = 0; ch < channels; ch++) {
                                sum += static_cast<float>(pInt16Data[i * channels + ch]);
                            }
                            samples[i] = sum / (static_cast<float>(channels) * 32768.0f);
                        }
                    } else if (wave_format_->wBitsPerSample == 24) {
                        UINT32 frame_size = wave_format_->nBlockAlign;
                        for (UINT32 i = 0; i < framesToRead; i++) {
                            float sum = 0.0f;
                            for (UINT32 ch = 0; ch < channels; ch++) {
                                BYTE* sample_ptr = pData + i * frame_size + ch * 3;
                                int32_t value = static_cast<int32_t>(sample_ptr[0]) |
                                               (static_cast<int32_t>(sample_ptr[1]) << 8) |
                                               (static_cast<int32_t>(sample_ptr[2]) << 16);
                                if (value & 0x800000) {
                                    value |= 0xFF000000;
                                }
                                sum += static_cast<float>(value);
                            }
                            samples[i] = sum / (static_cast<float>(channels) * 8388608.0f);
                        }
                    } else if (wave_format_->wBitsPerSample == 32) {
                        int32_t* pInt32Data = reinterpret_cast<int32_t*>(pData);
                        for (UINT32 i = 0; i < framesToRead; i++) {
                            float sum = 0.0f;
                            for (UINT32 ch = 0; ch < channels; ch++) {
                                sum += static_cast<float>(pInt32Data[i * channels + ch]);
                            }
                            samples[i] = sum / (static_cast<float>(channels) * 2147483648.0f);
                        }
                    }
                } else {
                    std::cerr << "Unsupported audio format: wFormatTag=" << wave_format_->wFormatTag
                              << ", wBitsPerSample=" << wave_format_->wBitsPerSample << std::endl;
                }

                std::lock_guard<std::mutex> lock(mutex_);
                samples_queue_.emplace(std::move(samples));
            } else {
                std::cout << "AUDCLNT_BUFFERFLAGS_SILENT detected, skipping" << std::endl;
            }

            hr = capture_client_->ReleaseBuffer(framesToRead);
            if (FAILED(hr)) {
                std::cerr << "ReleaseBuffer failed: " << hr << std::endl;
                break;
            }

            condition_variable_.notify_one();

            hr = capture_client_->GetNextPacketSize(&numFramesAvailable);
            if (FAILED(hr)) {
                std::cerr << "GetNextPacketSize failed: " << hr << std::endl;
                break;
            }
        }
    }

    audio_client_->Stop();
}
