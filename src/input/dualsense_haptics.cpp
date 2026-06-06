#include "engine.h"

#define SAMPLE_RATE 48000
#define WASAPI_CHANNELS 4  // 0,1: Speakers | 2,3: DualSense Haptics
#define FMOD_CHANNELS 2    // Stereo source

// Initialize the vector
std::vector<float> Engine::pcmBuffer;

// Initialize the atomics
std::atomic<size_t> Engine::writePos(0);
std::atomic<size_t> Engine::readPos(0);

// --- FMOD DSP Callback ---
FMOD_RESULT Engine::PCMGrabDSP(FMOD_DSP_STATE* state, float* inbuffer, float* outbuffer,
    unsigned int length, int inchannels, int* outchannels)
{
    const size_t totalSamples = (size_t)length * (size_t)inchannels;
    if (inbuffer && outbuffer) memcpy(outbuffer, inbuffer, totalSamples * sizeof(float));

    size_t w = writePos.load(std::memory_order_relaxed);
    const size_t bufSize = pcmBuffer.size();

    for (unsigned int i = 0; i < totalSamples; i++) {
        pcmBuffer[w] = inbuffer[i];
        w = (w + 1) % bufSize;
    }

    if (outbuffer) {
        // do not leak sound to speakers
        memset(outbuffer, 0, totalSamples * sizeof(float));
    }

    writePos.store(w, std::memory_order_release);
    return FMOD_OK;
}

// Platform specific initialization

#ifdef _WIN32
#include <functiondiscoverykeys_devpkey.h>

void Engine::initHaptics() {
    pcmBuffer = std::vector<float>(SAMPLE_RATE * FMOD_CHANNELS, 0.0f);
    CoInitialize(NULL);
    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator);

    enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);

    UINT count;
    collection->GetCount(&count);
    bool found = false;

    for (UINT i = 0; i < count; i++) {
        IMMDevice* pTempDevice = nullptr;
        IPropertyStore* pProps = nullptr;
        collection->Item(i, &pTempDevice);

        pTempDevice->OpenPropertyStore(STGM_READ, &pProps);

        PROPVARIANT varName;
        PropVariantInit(&varName);
        pProps->GetValue(PKEY_Device_FriendlyName, &varName);

        char name[256];
        size_t converted = 0;
        wcstombs_s(&converted, name, sizeof(name), varName.pwszVal, _TRUNCATE);

        printf("Checking device: %s\n", name);

        if (strstr(name, "Wireless Controller") || strstr(name, "DualSense")) {
            immDevice = pTempDevice;
            found = true;
            printf(">>> Found DualSense! Initializing haptics...\n");
            PropVariantClear(&varName);
            break;
        }

        PropVariantClear(&varName);
        pTempDevice->Release();
    }

    if (!found) {
        printf("Error: DualSense not found. Is it plugged in via USB?\n");
        return;
    }

    immDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient);

    WAVEFORMATEXTENSIBLE wfx = {};
    wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfx.Format.nChannels = WASAPI_CHANNELS;
    wfx.Format.nSamplesPerSec = SAMPLE_RATE;
    wfx.Format.wBitsPerSample = 32;
    wfx.Format.nBlockAlign = (wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8;
    wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
    wfx.Format.cbSize = 22;
    wfx.Samples.wValidBitsPerSample = 32;
    wfx.dwChannelMask = KSAUDIO_SPEAKER_QUAD;
    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 500000, 0, (WAVEFORMATEX*)&wfx, NULL);

    audioClient->GetBufferSize(&bufferFrameCount);
    audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&renderClient);

    hAudioEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    audioClient->SetEventHandle(hAudioEvent);

    // Clear everything before starting
    std::fill(pcmBuffer.begin(), pcmBuffer.end(), 0.0f);
    writePos.store(0);
    readPos.store(0);

    // Attach DSP
    FMOD_DSP_DESCRIPTION desc = {};
    desc.numinputbuffers = 1;
    desc.numoutputbuffers = 1;
    desc.read = PCMGrabDSP;
    strcpy_s(desc.name, "StereoCapture");

    FMOD::DSP* dsp = nullptr;
    system->setDSPBufferSize(256, 4);
    system->createDSP(&desc, &dsp);;
    system->createChannelGroup("Haptics Only", &hapticGroup);
    hapticGroup->addDSP(0, dsp);

    audioClient->Start(); // start listening
    mAudioThread = std::thread(&Engine::AudioRenderThread, this);
}

void Engine::AudioRenderThread() {
    // Priority is vital to prevent glitches when the OS is busy
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    while (this->running()) {
        UINT32 padding;
        if (FAILED(audioClient->GetCurrentPadding(&padding))) continue;

        UINT32 framesAvailable = bufferFrameCount - padding;

        if (framesAvailable > 0) {
            float* pData = nullptr;
            if (FAILED(renderClient->GetBuffer(framesAvailable, (BYTE**)&pData))) continue;

            size_t w = writePos.load(std::memory_order_acquire);
            size_t r = readPos.load(std::memory_order_relaxed);
            size_t bufSize = pcmBuffer.size();

            size_t distance = (w >= r) ? (w - r) : (bufSize - r + w);
            if (distance > (SAMPLE_RATE * FMOD_CHANNELS / 20)) {
                r = w;
            }

            for (UINT32 i = 0; i < framesAvailable; i++) {
                // Ensure we don't read past the write pointer if the buffer is empty
                if (r == w) {
                    pData[i * 4 + 0] = 0.0f;
                    pData[i * 4 + 1] = 0.0f;
                    pData[i * 4 + 2] = 0.0f;
                    pData[i * 4 + 3] = 0.0f;
                    continue;
                }

                float left = pcmBuffer[r]; r = (r + 1) % bufSize;
                float right = pcmBuffer[r]; r = (r + 1) % bufSize;

                pData[i * 4 + 0] = 0.0f;
                pData[i * 4 + 1] = 0.0f;
                // Multiplied by 2.0f here as per your original logic
                pData[i * 4 + 2] = left * 2.0f;
                pData[i * 4 + 3] = right * 2.0f;
            }

            readPos.store(r, std::memory_order_release);
            renderClient->ReleaseBuffer(framesAvailable, 0);
        }
    }
}
#elif __linux__

#include <alsa/asoundlib.h>
#include <pthread.h>

void Engine::initHaptics() {
    // Small buffer - 50ms max to prevent latency stacking
    pcmBuffer = std::vector<float>((SAMPLE_RATE / 20) * FMOD_CHANNELS, 0.0f);
    std::fill(pcmBuffer.begin(), pcmBuffer.end(), 0.0f);
    writePos.store(0);
    readPos.store(0);

    // Find DualSense ALSA device
    void** hints;
    char* name = nullptr;
    char* desc = nullptr;
    bool found = false;

    snd_device_name_hint(-1, "pcm", &hints);

    for (void** n = hints; *n; n++) {
        name = snd_device_name_get_hint(*n, "NAME");
        desc = snd_device_name_get_hint(*n, "DESC");

        if (desc && (strstr(desc, "DualSense") || strstr(desc, "Wireless Controller"))) {
            alsaDevice = strdup(name);
            found = true;
            printf(">>> Found DualSense ALSA device: %s\n", name);
            free(name);
            free(desc);
            break;
        }

        free(name);
        free(desc);
    }

    snd_device_name_free_hint(hints);

    if (!found) {
        printf("Error: DualSense not found on Linux. Is it plugged in via USB?\n");
        return;
    }

    // Open PCM device
    if (snd_pcm_open(&alsaHandle, alsaDevice, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        printf("Error: failed to open DualSense ALSA device\n");
        return;
    }

    // Configure hw params: 4ch, 48khz, float32
    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(alsaHandle, params);
    snd_pcm_hw_params_set_access(alsaHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(alsaHandle, params, SND_PCM_FORMAT_FLOAT_LE);
    snd_pcm_hw_params_set_channels(alsaHandle, params, WASAPI_CHANNELS);
    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(alsaHandle, params, &rate, 0);

    // Small period for low latency
    snd_pcm_uframes_t periodSize = 64;
    snd_pcm_hw_params_set_period_size_near(alsaHandle, params, &periodSize, 0);

    // 4 periods in buffer
    unsigned int periods = 4;
    snd_pcm_hw_params_set_periods_near(alsaHandle, params, &periods, 0);

    snd_pcm_hw_params(alsaHandle, params);

    // Configure sw params - wake up every period
    snd_pcm_sw_params_t* swparams;
    snd_pcm_sw_params_alloca(&swparams);
    snd_pcm_sw_params_current(alsaHandle, swparams);
    snd_pcm_sw_params_set_avail_min(alsaHandle, swparams, periodSize);
    snd_pcm_sw_params_set_start_threshold(alsaHandle, swparams, periodSize);
    snd_pcm_sw_params(alsaHandle, swparams);

    snd_pcm_prepare(alsaHandle);

    // Pre-fill with silence to avoid initial underrun
    std::vector<float> silence(periodSize * WASAPI_CHANNELS, 0.0f);
    snd_pcm_writei(alsaHandle, silence.data(), periodSize);

    // Attach FMOD DSP - match period size
    FMOD_DSP_DESCRIPTION desc2 = {};
    desc2.numinputbuffers  = 1;
    desc2.numoutputbuffers = 1;
    desc2.read             = PCMGrabDSP;
    strcpy(desc2.name, "StereoCapture");

    FMOD::DSP* dsp = nullptr;
    system->setDSPBufferSize(64, 2);
    system->createDSP(&desc2, &dsp);
    system->createChannelGroup("Haptics Only", &hapticGroup);
    hapticGroup->addDSP(0, dsp);

    mAudioThread = std::thread(&Engine::AudioRenderThread, this);
}

void Engine::AudioRenderThread() {
    // Max realtime priority
    struct sched_param param{};
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    const int framesPerPeriod = 64;
    std::vector<float> staging(framesPerPeriod * WASAPI_CHANNELS, 0.0f);
    const size_t bufSize = pcmBuffer.size();

    while (this->running()) {
        size_t w = writePos.load(std::memory_order_acquire);
        size_t r = readPos.load(std::memory_order_relaxed);

        // Aggressive drift correction - 20ms threshold
        size_t distance = (w >= r) ? (w - r) : (bufSize - r + w);
        if (distance > (SAMPLE_RATE * FMOD_CHANNELS / 50)) {
            r = w;
        }

        for (int i = 0; i < framesPerPeriod; i++) {
            float left = 0.0f, right = 0.0f;

            if (r != w) {
                left  = pcmBuffer[r]; r = (r + 1) % bufSize;
                right = pcmBuffer[r]; r = (r + 1) % bufSize;
            }

            staging[i * 4 + 0] = 0.0f;
            staging[i * 4 + 1] = 0.0f;
            staging[i * 4 + 2] = left  * 2.0f;
            staging[i * 4 + 3] = right * 2.0f;
        }

        readPos.store(r, std::memory_order_release);

        int err = snd_pcm_writei(alsaHandle, staging.data(), framesPerPeriod);
        if (err == -EPIPE) {
            snd_pcm_prepare(alsaHandle);
        } else if (err < 0) {
            snd_pcm_recover(alsaHandle, err, 0);
        }
    }

    snd_pcm_drain(alsaHandle);
    snd_pcm_close(alsaHandle);
    free(alsaDevice);
}

bool Engine::isDualSenseAttached() {
    return alsaHandle != nullptr;
}
#endif

void Engine::dualsense_playHaptics(Sound* sound, float volume) {
    // TODO: Track haptics channel (perhaps make channels returnable for whole stack)
    if (!isDualSenseAttached()) return;
    FMOD::Channel* ch;
    system->playSound(sound->sound, hapticGroup, false, &ch); // untracked
    ch->setVolume(volume);
}