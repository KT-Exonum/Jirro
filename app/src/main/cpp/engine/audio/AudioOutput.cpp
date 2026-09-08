#include "AudioOutput.h"

#include <android/log.h>
#include <aaudio/AAudio.h>

#define LOG_TAG "AudioOutput"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace vfx {

aaudio_data_callback_result_t AudioOutput::DataCallback(
    AAudioStream* stream, void* userData, void* audioData, int32_t numFrames) {
    
    AudioOutput* self = static_cast<AudioOutput*>(userData);
    if (!self) return AAUDIO_CALLBACK_RESULT_CONTINUE;

    float* output = static_cast<float*>(audioData);
    int channels = AAudioStream_getChannelCount(stream);
    int frames = numFrames;

    std::lock_guard<std::mutex> lock(self->callbackMutex_);
    if (self->callback_ && self->timelineProvider_) {
        double timelinePos = self->timelineProvider_();
        double frameDuration = static_cast<double>(frames) / self->sampleRate_;
        self->callback_({output, static_cast<size_t>(frames * channels)}, frames, timelinePos, frameDuration);
    } else {
        // Silence if no callback
        std::fill(output, output + frames * channels, 0.0f);
    }

    // Apply master volume
    float vol = self->volume_.load();
    if (vol != 1.0f) {
        for (int i = 0; i < frames * channels; ++i) {
            output[i] *= vol;
        }
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

bool AudioOutput::Start(int sampleRate, int channels, int framesPerBurst) {
    if (stream_) return true; // Already started

    sampleRate_ = sampleRate;
    channels_ = channels;
    framesPerBurst_ = framesPerBurst;

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK) {
        LOGE("Failed to create stream builder: %d", result);
        return false;
    }

    AAudioStreamBuilder_setDeviceId(builder, AAUDIO_UNSPECIFIED);
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setSampleRate(builder, sampleRate);
    AAudioStreamBuilder_setChannelCount(builder, channels);
    AAudioStreamBuilder_setFramesPerDataCallback(builder, framesPerBurst);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDataCallback(builder, DataCallback, this);

    result = AAudioStreamBuilder_openStream(builder, &stream_);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK) {
        LOGE("Failed to open audio stream: %d", result);
        stream_ = nullptr;
        return false;
    }

    state_ = AAudioStream_getState(stream_);
    if (state_ != AAUDIO_STREAM_STATE_OPEN && state_ != AAUDIO_STREAM_STATE_STARTING && state_ != AAUDIO_STREAM_STATE_STARTED) {
        LOGE("Stream not initialized: %d", state_);
        AAudioStream_close(stream_);
        stream_ = nullptr;
        return false;
    }

    result = AAudioStream_requestStart(stream_);
    if (result != AAUDIO_OK) {
        LOGE("Failed to start audio stream: %d", result);
        AAudioStream_close(stream_);
        stream_ = nullptr;
        return false;
    }

    // Wait for started state
    for (int i = 0; i < 100; ++i) {
        state_ = AAudioStream_getState(stream_);
        if (state_ == AAUDIO_STREAM_STATE_STARTED) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (state_ != AAUDIO_STREAM_STATE_STARTED) {
        LOGE("Stream failed to start: %d", state_);
        Stop();
        return false;
    }

    LOGI("AudioOutput started: %d Hz, %d ch, %d frames/burst",
         sampleRate, channels, framesPerBurst);
    return true;
}

void AudioOutput::Stop() {
    if (!stream_) return;

    AAudioStream_requestStop(stream_);
    AAudioStream_close(stream_);
    stream_ = nullptr;
    state_ = AAUDIO_STREAM_STATE_UNINITIALIZED;
    LOGI("AudioOutput stopped");
}

int AudioOutput::GetLatencyFrames() const {
    if (!stream_) return 0;
    return AAudioStream_getFramesPerBurst(stream_) * 2; // Approximate
}

} // namespace vfx