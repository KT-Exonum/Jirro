#pragma once
// Audio output sink using AAudio for low-latency playback
// Runs on a dedicated high-priority callback thread

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>

#include <aaudio/AAudio.h>

namespace vfx {

class AudioOutput {
public:
    using Callback = std::function<void(std::span<float> output, int numFrames)>;

    AudioOutput() = default;
    ~AudioOutput() { Stop(); }

    // Initialize and start audio stream
    bool Start(int sampleRate = 48000, int channels = 2, int framesPerBurst = 240);
    void Stop();

    // Set the callback that provides mixed audio
    void SetCallback(Callback cb) { callback_ = std::move(cb); }

    // Get current latency estimate in frames
    [[nodiscard]] int GetLatencyFrames() const;

    // Get stream state
    [[nodiscard]] aaudio_stream_state_t GetState() const { return state_; }

    // Set volume (0.0 - 1.0)
    void SetVolume(float volume) { volume_ = std::clamp(volume, 0.0f, 1.0f); }

private:
    static aaudio_data_callback_result_t DataCallback(
        AAudioStream* stream, void* userData, void* audioData, int32_t numFrames);

    AAudioStream* stream_ = nullptr;
    aaudio_stream_state_t state_ = AAUDIO_STREAM_STATE_UNINITIALIZED;
    Callback callback_;
    std::mutex callbackMutex_;
    std::atomic<float> volume_{1.0f};
    int sampleRate_ = 48000;
    int channels_ = 2;
    int framesPerBurst_ = 240;
};

} // namespace vfx