#include "engine.h"
#include <functiondiscoverykeys_devpkey.h>

#define SAMPLE_RATE 48000
#define WASAPI_CHANNELS 4  // 0,1: Speakers | 2,3: DualSense Haptics
#define FMOD_CHANNELS 2    // Stereo source

// Initialize the vector
std::vector<float> Engine::pcmBuffer;

// Initialize the atomics
std::atomic<size_t> Engine::writePos(0);
std::atomic<size_t> Engine::readPos(0);

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

void Engine::playHaptics(Sound* sound, float volume) {
    // TODO: Track haptics channel (perhaps make channels returnable for whole stack)
    FMOD::Channel* ch;
    system->playSound(sound->sound, hapticGroup, false, &ch); // untracked
    ch->setVolume(volume);
}