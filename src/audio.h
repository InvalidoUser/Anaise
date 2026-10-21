// Anaise audio -- the three CC0 ambient tracks, embedded as RCDATA.
//
// The tracks are mono 22.05 kHz 16-bit WAVs (see assets/audio/CREDITS.txt for
// the sources and the loop edits). Audio is streamed through waveOut from a
// worker thread; the device callback only signals an event, so nothing that
// can block runs on the driver thread. No files are read from disk at run
// time and nothing is written at all.
#pragma once

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace anaise {

enum class Music {
    None = -1,
    Intro = 0,
    Menu = 1,
    Room = 2,
};

class Audio {
public:
    ~Audio() { close(); }

    // Loads the three clips and opens the default wave output. Returns false
    // (and stays silent) on a machine with no usable audio device.
    bool open(HINSTANCE instance) {
        if (open_) return true;
        instance_ = instance;
        for (int i = 0; i < kTrackCount; ++i) {
            tracks_[i] = loadClip(instance, kResourceIds[i]);
            if (tracks_[i].samples.empty()) {
                releaseTracks();
                return false;
            }
        }

        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 1;
        format.nSamplesPerSec = kSampleRate;
        format.wBitsPerSample = 16;
        format.nBlockAlign = static_cast<WORD>(format.nChannels *
                                               format.wBitsPerSample / 8);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        if (waveOutOpen(&device_, WAVE_MAPPER, &format,
                        reinterpret_cast<DWORD_PTR>(&Audio::waveCallback),
                        reinterpret_cast<DWORD_PTR>(this),
                        CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
            device_ = nullptr;
            releaseTracks();
            return false;
        }

        doneEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!doneEvent_) {
            close();
            return false;
        }
        stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!stopEvent_) {
            close();
            return false;
        }

        for (int i = 0; i < kBuffers; ++i) {
            samples_[i].assign(kBlockSamples, 0);
            headers_[i] = WAVEHDR{};
            headers_[i].lpData = reinterpret_cast<LPSTR>(samples_[i].data());
            headers_[i].dwBufferLength =
                static_cast<DWORD>(samples_[i].size() * sizeof(std::int16_t));
            if (waveOutPrepareHeader(device_, &headers_[i],
                                     sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
                close();
                return false;
            }
            prepared_[i] = true;
        }

        open_ = true;
        current_ = kTrackCount;  // nothing yet
        requested_.store(static_cast<int>(Music::None));
        InterlockedExchange(&running_, 1);
        for (int i = 0; i < kBuffers; ++i) {
            fillBuffer(samples_[i]);
            headers_[i].dwFlags &= ~WHDR_DONE;
            if (waveOutWrite(device_, &headers_[i], sizeof(WAVEHDR)) !=
                MMSYSERR_NOERROR) {
                close();
                return false;
            }
        }
        worker_ = CreateThread(nullptr, 0, &Audio::workerProc, this, 0,
                               nullptr);
        if (!worker_) {
            close();
            return false;
        }
        return true;
    }

    void close() {
        if (worker_) {
            InterlockedExchange(&running_, 0);
            if (stopEvent_) SetEvent(stopEvent_);
            WaitForSingleObject(worker_, 2000);
            CloseHandle(worker_);
            worker_ = nullptr;
        }
        if (device_) {
            waveOutReset(device_);
            for (int i = 0; i < kBuffers; ++i) {
                if (prepared_[i]) {
                    waveOutUnprepareHeader(device_, &headers_[i],
                                           sizeof(WAVEHDR));
                    prepared_[i] = false;
                }
            }
            waveOutClose(device_);
            device_ = nullptr;
        }
        if (doneEvent_) {
            CloseHandle(doneEvent_);
            doneEvent_ = nullptr;
        }
        if (stopEvent_) {
            CloseHandle(stopEvent_);
            stopEvent_ = nullptr;
        }
        releaseTracks();
        open_ = false;
        current_ = kTrackCount;
        requested_.store(static_cast<int>(Music::None));
    }

    // Crossfades to another track. Safe to call every frame.
    void play(Music music) {
        if (!open_) return;
        const int wanted = static_cast<int>(music);
        if (wanted >= 0 && wanted < kTrackCount) requested_.store(wanted);
    }

    bool muted() const { return muted_.load(); }

    bool toggleMuted() {
        muted_.store(!muted_.load());
        return muted_.load();
    }

    bool available() const { return open_; }

    // Checks that the embedded clips parse, without opening a device.
    static bool resourcesPresent(HINSTANCE instance) {
        for (int id : kResourceIds) {
            Clip clip = loadClip(instance, id);
            if (clip.samples.empty()) return false;
        }
        return true;
    }

private:
    static constexpr int kTrackCount = 3;
    static constexpr int kSampleRate = 22050;
    static constexpr int kBuffers = 4;
    // 100 ms per buffer: short enough that a mute or a track change is felt
    // immediately, long enough that the worker wakes only ten times a second.
    static constexpr int kBlockSamples = kSampleRate / 10;
    static constexpr int kResourceIds[kTrackCount] = {201, 202, 203};
    static constexpr double kFadeSeconds = 0.45;
    static constexpr double kMuteSeconds = 0.06;
    static constexpr double kMasterGain = 0.85;

    struct Clip {
        std::vector<std::int16_t> samples;
        bool loop = true;
    };

    static std::uint16_t read16(const std::uint8_t* p) {
        return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
    }

    static std::uint32_t read32(const std::uint8_t* p) {
        return static_cast<std::uint32_t>(p[0]) |
               (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) |
               (static_cast<std::uint32_t>(p[3]) << 24);
    }

    static Clip loadClip(HINSTANCE instance, int id) {
        Clip clip;
        HRSRC resource =
            FindResourceW(instance, MAKEINTRESOURCEW(id), RT_RCDATA);
        if (!resource) return clip;
        const DWORD size = SizeofResource(instance, resource);
        HGLOBAL handle = LoadResource(instance, resource);
        if (!handle || size < 44) return clip;
        const auto* data =
            static_cast<const std::uint8_t*>(LockResource(handle));
        if (!data) return clip;
        if (std::memcmp(data, "RIFF", 4) != 0 ||
            std::memcmp(data + 8, "WAVE", 4) != 0) {
            return clip;
        }

        const std::uint8_t* pcm = nullptr;
        std::uint32_t pcmBytes = 0;
        bool formatOk = false;
        std::uint32_t offset = 12;
        while (offset + 8 <= size) {
            const std::uint8_t* chunk = data + offset;
            const std::uint32_t chunkSize = read32(chunk + 4);
            const std::uint8_t* body = chunk + 8;
            if (chunkSize > size - offset - 8) break;
            if (std::memcmp(chunk, "fmt ", 4) == 0 && chunkSize >= 16) {
                const std::uint16_t tag = read16(body);
                const std::uint16_t channels = read16(body + 2);
                const std::uint32_t rate = read32(body + 4);
                const std::uint16_t bits = read16(body + 14);
                formatOk = tag == WAVE_FORMAT_PCM && channels == 1 &&
                           rate == kSampleRate && bits == 16;
            } else if (std::memcmp(chunk, "data", 4) == 0) {
                pcm = body;
                pcmBytes = chunkSize;
            }
            offset += 8 + chunkSize + (chunkSize & 1u);
        }
        if (!formatOk || !pcm || pcmBytes < sizeof(std::int16_t)) return clip;

        const std::size_t count = pcmBytes / sizeof(std::int16_t);
        clip.samples.resize(count);
        std::memcpy(clip.samples.data(), pcm, count * sizeof(std::int16_t));
        // The intro is a one-shot; the other two are gapless loops.
        clip.loop = id != kResourceIds[0];
        return clip;
    }

    void releaseTracks() {
        for (Clip& clip : tracks_) {
            clip.samples.clear();
            clip.samples.shrink_to_fit();
        }
    }

    static void CALLBACK waveCallback(HWAVEOUT, UINT message, DWORD_PTR instance,
                                      DWORD_PTR, DWORD_PTR) {
        if (message != WOM_DONE) return;
        auto* self = reinterpret_cast<Audio*>(instance);
        // Only an event is set here: waveOutWrite is not safe from the driver
        // thread, so the worker does all of the buffer work.
        if (self && self->doneEvent_) SetEvent(self->doneEvent_);
    }

    static DWORD WINAPI workerProc(LPVOID parameter) {
        static_cast<Audio*>(parameter)->run();
        return 0;
    }

    void run() {
        HANDLE waits[2] = {doneEvent_, stopEvent_};
        while (InterlockedCompareExchange(&running_, 1, 1) != 0) {
            const DWORD result = WaitForMultipleObjects(2, waits, FALSE, 200);
            if (result == WAIT_OBJECT_0 + 1) break;
            if (result != WAIT_OBJECT_0) continue;
            // The event is auto-reset and completions can coalesce, so clear
            // it first and then refill every buffer the device has released.
            ResetEvent(doneEvent_);
            refillFinishedBuffers();
        }
    }

    void refillFinishedBuffers() {
        for (int i = 0; i < kBuffers; ++i) {
            WAVEHDR& header = headers_[i];
            if ((header.dwFlags & WHDR_DONE) == 0) continue;
            fillBuffer(samples_[i]);
            header.dwFlags &= ~WHDR_DONE;
            waveOutWrite(device_, &header, sizeof(WAVEHDR));
        }
    }

    void fillBuffer(std::vector<std::int16_t>& buffer) {
        const double fadeStep = 1.0 / (kFadeSeconds * kSampleRate);
        const double muteStep = 1.0 / (kMuteSeconds * kSampleRate);
        const double muteTarget = muted_.load() ? 0.0 : 1.0;
        for (std::int16_t& out : buffer) {
            const int wanted = requested_.load();
            if (wanted != current_) {
                if (current_ >= kTrackCount) {
                    current_ = wanted;
                    position_ = 0;
                    fade_ = 0.0;
                } else if (fade_ > 0.0) {
                    fade_ = std::max(0.0, fade_ - fadeStep);
                } else {
                    current_ = wanted;
                    position_ = 0;
                }
            } else if (current_ >= 0 && current_ < kTrackCount &&
                       fade_ < 1.0) {
                fade_ = std::min(1.0, fade_ + fadeStep);
            }

            if (muteGain_ < muteTarget) {
                muteGain_ = std::min(muteTarget, muteGain_ + muteStep);
            } else if (muteGain_ > muteTarget) {
                muteGain_ = std::max(muteTarget, muteGain_ - muteStep);
            }

            double value = 0.0;
            if (current_ >= 0 && current_ < kTrackCount) {
                Clip& clip = tracks_[current_];
                if (!clip.samples.empty()) {
                    value = clip.samples[position_];
                    ++position_;
                    if (position_ >= clip.samples.size()) {
                        if (clip.loop) {
                            position_ = 0;
                        } else {
                            // The intro ends; fall into the menu loop.
                            requested_.store(
                                static_cast<int>(Music::Menu));
                            current_ = kTrackCount;
                            fade_ = 0.0;
                            position_ = 0;
                        }
                    }
                }
            }
            const double gain = kMasterGain * fade_ * muteGain_;
            const long mixed = std::lround(value * gain);
            out = static_cast<std::int16_t>(
                std::clamp<long>(mixed, -32768, 32767));
        }
    }

    HINSTANCE instance_ = nullptr;
    HWAVEOUT device_ = nullptr;
    HANDLE doneEvent_ = nullptr;
    HANDLE stopEvent_ = nullptr;
    HANDLE worker_ = nullptr;
    volatile LONG running_ = 1;

    Clip tracks_[kTrackCount];
    std::vector<std::int16_t> samples_[kBuffers];
    WAVEHDR headers_[kBuffers]{};
    bool prepared_[kBuffers]{};

    int current_ = kTrackCount;
    std::size_t position_ = 0;
    double fade_ = 0.0;
    double muteGain_ = 1.0;
    std::atomic<int> requested_{static_cast<int>(Music::None)};
    std::atomic<bool> muted_{false};
    bool open_ = false;
};

}  // namespace anaise
