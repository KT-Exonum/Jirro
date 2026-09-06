#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <android/log.h>

#define LOG_TAG "AudioEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

// ============================================================================
// AudioAnalyzer
// ============================================================================

namespace {
float HannWindow(size_t i, size_t size) {
    return 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (size - 1)));
}

std::vector<float> GenerateWindow(int fftSize) {
    std::vector<float> w(fftSize);
    for (int i = 0; i < fftSize; ++i) w[i] = HannWindow(i, fftSize);
    return w;
}

void FFT(std::vector<std::complex<float>>& data) {
    const size_t n = data.size();
    if (n <= 1) return;

    for (size_t i = 0, j = 0; i < n; ++i) {
        if (i < j) std::swap(data[i], data[j]);
        size_t bit = n >> 1;
        while (bit && j >= bit) { j -= bit; bit >>= 1; }
        j += bit;
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * M_PI / len;
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j) {
                std::complex<float> u = data[i + j];
                std::complex<float> v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}
} // anonymous

AudioAnalyzer::AudioAnalyzer(int sampleRate, int fftSize)
    : sampleRate_(sampleRate), fftSize_(fftSize),
      window_(GenerateWindow(fftSize)),
      fftBuffer_(fftSize),
      fftComplex_(fftSize) {}

void AudioAnalyzer::ApplyWindow(float* buffer, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        buffer[i] *= (i < window_.size()) ? window_[i] : 1.0f;
    }
}

void AudioAnalyzer::ComputeFFT(const float* input) {
    std::memcpy(fftBuffer_.data(), input, fftSize_ * sizeof(float));
    ApplyWindow(fftBuffer_.data(), fftSize_);
    for (int i = 0; i < fftSize_; ++i) {
        fftComplex_[i] = std::complex<float>(fftBuffer_[i], 0.0f);
    }
    FFT(fftComplex_);
}

SpectrumData AudioAnalyzer::AnalyzeSpectrum(const float* samples, size_t frameCount, int channels) {
    SpectrumData data;
    data.sampleRate = sampleRate_;
    data.fftSize = fftSize_;
    data.frequencies.resize(fftSize_ / 2);
    data.magnitudes.resize(fftSize_ / 2);

    float binHz = sampleRate_ / static_cast<float>(fftSize_);
    for (int i = 0; i < fftSize_ / 2; ++i) {
        data.frequencies[i] = i * binHz;
    }

    if (frameCount < fftSize_) return data;

    ComputeFFT(samples);
    for (int i = 0; i < fftSize_ / 2; ++i) {
        float mag = std::abs(fftComplex_[i]) / fftSize_;
        data.magnitudes[i] = mag * 2.0f;
    }

    return data;
}

std::optional<BeatInfo> AudioAnalyzer::DetectBeats(const float* samples, size_t frameCount, int channels, double currentTimeSec) {
    float rms = GetRMS(samples, frameCount, channels);
    const float attackCoef = 0.2f;
    const float decayCoef = 0.05f;
    float energy = rms * rms;

    if (beatState_.avgEnergy == 0.0f) {
        beatState_.avgEnergy = energy;
        beatState_.lastEnergy = energy;
        return std::nullopt;
    }

    float variance = std::abs(energy - beatState_.avgEnergy) / (beatState_.avgEnergy + 1e-6f);
    bool isBeat = energy > beatState_.avgEnergy * 1.4f && variance > 0.3f;

    BeatInfo info;
    if (isBeat && (currentTimeSec - beatState_.lastBeatSample / sampleRate_) > 0.25) {
        info.beatTimes.push_back(static_cast<float>(currentTimeSec));
        info.beatStrengths.push_back(std::clamp(energy / (beatState_.avgEnergy + 1e-6f), 0.0f, 1.0f));
        beatState_.lastBeatSample = static_cast<int64_t>(currentTimeSec * sampleRate_);
        beatState_.avgEnergy = beatState_.avgEnergy * (1.0f - decayCoef) + energy * decayCoef;
    } else {
        beatState_.avgEnergy = beatState_.avgEnergy * (1.0f - attackCoef) + energy * attackCoef;
    }

    if (!info.beatTimes.empty()) {
        if (info.beatTimes.size() >= 2) {
            float interval = info.beatTimes.back() - info.beatTimes[info.beatTimes.size() - 2];
            if (interval > 0.0f) info.tempo = 60.0f / interval;
        }
        info.confidence = 0.7f;
    }

    return info;
}

std::vector<float> AudioAnalyzer::GetWaveform(const float* samples, size_t frameCount, int channels, size_t targetPoints) {
    std::vector<float> result(targetPoints, 0.0f);
    if (frameCount == 0 || targetPoints == 0) return result;

    size_t samplesPerPoint = frameCount / targetPoints;
    for (size_t i = 0; i < targetPoints; ++i) {
        float maxVal = 0.0f;
        size_t start = i * samplesPerPoint;
        size_t end = std::min(start + samplesPerPoint, frameCount);
        for (size_t j = start; j < end; j += channels) {
            float val = samples[j];
            maxVal = std::max(maxVal, std::abs(val));
        }
        result[i] = maxVal;
    }
    return result;
}

float AudioAnalyzer::GetRMS(const float* samples, size_t frameCount, int channels) {
    if (frameCount == 0) return 0.0f;
    float sum = 0.0f;
    for (size_t i = 0; i < frameCount; i += channels) {
        float val = samples[i];
        sum += val * val;
    }
    return std::sqrt(sum / (frameCount / channels));
}

// ============================================================================
// AudioMixer
// ============================================================================

AudioMixer::AudioMixer(int sampleRate, int channels, int bufferFrames)
    : sampleRate_(sampleRate), channels_(channels), bufferFrames_(bufferFrames) {}

void AudioMixer::AddInput(const std::string& id, int channels) {
    std::lock_guard<std::mutex> lock(mutex_);
    MixInput& input = inputs_[id];
    input.id = id;
    input.channels = channels;
    input.buffer.resize(bufferFrames_ * channels_, 0.0f);
    input.readPos = 0;
    input.writePos = 0;
    input.volume = 1.0f;
    input.pan = 0.0f;
    input.mute = false;
    input.sampleRate = sampleRate_;
}

void AudioMixer::RemoveInput(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    inputs_.erase(id);
}

bool AudioMixer::WriteInput(const std::string& id, const float* samples, size_t frameCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = inputs_.find(id);
    if (it == inputs_.end()) return false;

    MixInput& input = it->second;
    size_t writePos = input.writePos;
    size_t available = bufferFrames_ - ((writePos >= input.readPos) ? (writePos - input.readPos) : (bufferFrames_ - input.readPos + writePos));

    for (size_t i = 0; i < frameCount; ++i) {
        input.buffer[writePos * channels_] = samples[i * channels_];
        if (channels_ > 1) input.buffer[writePos * channels_ + 1] = samples[i * channels_ + 1];
        writePos = (writePos + 1) % bufferFrames_;
    }
    input.writePos = writePos;
    return true;
}

void AudioMixer::SetInputVolume(const std::string& id, float volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = inputs_.find(id); it != inputs_.end()) it->second.volume = std::clamp(volume, 0.0f, 2.0f);
}

void AudioMixer::SetInputPan(const std::string& id, float pan) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = inputs_.find(id); it != inputs_.end()) it->second.pan = std::clamp(pan, -1.0f, 1.0f);
}

void AudioMixer::SetInputMute(const std::string& id, bool mute) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = inputs_.find(id); it != inputs_.end()) it->second.mute = mute;
}

size_t AudioMixer::Mix(float* output, size_t frameCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (inputs_.empty()) {
        std::memset(output, 0, frameCount * channels_ * sizeof(float));
        return frameCount;
    }

    std::memset(output, 0, frameCount * channels_ * sizeof(float));

    for (auto& [id, input] : inputs_) {
        if (input.mute) continue;
        for (size_t i = 0; i < frameCount; ++i) {
            if (input.readPos == input.writePos) break;
            size_t idx = input.readPos * channels_;
            float vol = input.volume * masterVolume_;
            float pan = input.pan + masterPan_;
            float leftGain = std::clamp(1.0f - pan * 0.5f, 0.0f, 1.0f);
            float rightGain = std::clamp(1.0f + pan * 0.5f, 0.0f, 1.0f);

            if (channels_ >= 2) {
                output[i * 2] += input.buffer[idx] * vol * leftGain;
                output[i * 2 + 1] += input.buffer[idx + 1] * vol * rightGain;
            } else {
                output[i] += input.buffer[idx] * vol;
            }
            input.readPos = (input.readPos + 1) % bufferFrames_;
        }
    }

    for (size_t i = 0; i < frameCount * channels_; ++i) {
        output[i] = std::clamp(output[i], -1.0f, 1.0f);
    }

    return frameCount;
}

std::vector<float> AudioMixer::RenderMix(double startTimeSec, double endTimeSec) {
    size_t totalFrames = static_cast<size_t>((endTimeSec - startTimeSec) * sampleRate_);
    std::vector<float> output(totalFrames * channels_);
    Mix(output.data(), totalFrames);
    return output;
}

// ============================================================================
// AudioDecoder
// ============================================================================

AudioDecoder::AudioDecoder() = default;

AudioDecoder::~AudioDecoder() { Close(); }

bool AudioDecoder::Open(const std::string& path) {
    extractor_ = AMediaExtractor_new();
    if (!extractor_) return false;

    media_status_t status = AMediaExtractor_setDataSource(extractor_, path.c_str());
    if (status != AMEDIA_OK) {
        LOGE("Failed to open media: %s", path.c_str());
        Close();
        return false;
    }

    int trackCount = AMediaExtractor_getTrackCount(extractor_);
    for (int i = 0; i < trackCount; ++i) {
        AMediaFormat* fmt = AMediaExtractor_getTrackFormat(extractor_, i);
        const char* mime = nullptr;
        AMediaFormat_getString(fmt, AMEDIAFORMAT_KEY_MIME, &mime);
        if (mime && strncmp(mime, "audio/", 6) == 0) {
            trackIndex_ = i;
            AMediaExtractor_selectTrack(extractor_, i);
            format_.mimeType = mime;

            int32_t sampleRate = 48000;
            int32_t channels = 2;
            AMediaFormat_getInteger(fmt, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate);
            AMediaFormat_getInteger(fmt, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
            format_.sampleRate = sampleRate;
            format_.channels = channels;

            int64_t durationUs = 0;
            AMediaFormat_getLongLong(fmt, AMEDIAFORMAT_KEY_DURATION, &durationUs);
            format_.durationUs = durationUs;

            codec_ = AMediaCodec_createDecoderByType(mime);
            if (!codec_) {
                LOGE("Failed to create decoder for %s", mime);
                Close();
                return false;
            }

            media_status_t cs = AMediaCodec_configure(codec_, fmt, nullptr, nullptr, 0);
            if (cs != AMEDIA_OK) {
                LOGE("Failed to configure codec");
                Close();
                return false;
            }

            AMediaCodec_start(codec_);
            AMediaFormat_delete(fmt);
            return true;
        }
        AMediaFormat_delete(fmt);
    }

    LOGE("No audio track found in %s", path.c_str());
    Close();
    return false;
}

bool AudioDecoder::OpenFromMediaExtractor(AMediaExtractor* extractor, int trackIndex) {
    extractor_ = extractor;
    AMediaExtractor_selectTrack(extractor_, trackIndex);
    trackIndex_ = trackIndex;

    AMediaFormat* fmt = AMediaExtractor_getTrackFormat(extractor_, trackIndex);
    const char* mime = nullptr;
    AMediaFormat_getString(fmt, AMEDIAFORMAT_KEY_MIME, &mime);
    format_.mimeType = mime ? mime : "";

    int32_t sampleRate = 48000;
    int32_t channels = 2;
    AMediaFormat_getInteger(fmt, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate);
    AMediaFormat_getInteger(fmt, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
    format_.sampleRate = sampleRate;
    format_.channels = channels;

    int64_t durationUs = 0;
    AMediaFormat_getLongLong(fmt, AMEDIAFORMAT_KEY_DURATION, &durationUs);
    format_.durationUs = durationUs;

    codec_ = AMediaCodec_createDecoderByType(mime);
    media_status_t cs = AMediaCodec_configure(codec_, fmt, nullptr, nullptr, 0);
    if (cs != AMEDIA_OK) {
        AMediaFormat_delete(fmt);
        Close();
        return false;
    }
    AMediaCodec_start(codec_);
    AMediaFormat_delete(fmt);
    return true;
}

void AudioDecoder::Close() {
    if (codec_) {
        AMediaCodec_stop(codec_);
        AMediaCodec_delete(codec_);
        codec_ = nullptr;
    }
    if (extractor_) {
        AMediaExtractor_release(extractor_);
        extractor_ = nullptr;
    }
    sawEOS_ = false;
    inputBuffer_.clear();
    outputBuffer_.clear();
}

std::optional<AudioFrame> AudioDecoder::DecodeFrame() {
    if (!codec_ || sawEOS_) return std::nullopt;

    ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(codec_, 100000);
    if (inputIndex >= 0) {
        size_t bufSize = 0;
        uint8_t* buf = AMediaCodec_getInputBuffer(codec_, inputIndex, &bufSize);
        ssize_t sampleSize = AMediaExtractor_readSampleData(extractor_, buf, static_cast<int32_t>(bufSize));

        if (sampleSize < 0) {
            AMediaCodec_queueInputBuffer(codec_, inputIndex, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
            sawEOS_ = true;
        } else {
            int64_t pts = AMediaExtractor_getSampleTime(extractor_);
            AMediaCodec_queueInputBuffer(codec_, inputIndex, 0, static_cast<size_t>(sampleSize), pts, 0);
            AMediaExtractor_advance(extractor_);
        }
    }

    AMediaCodecBufferInfo info;
    ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(codec_, &info, 100000);
    if (outputIndex >= 0) {
        if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
            AMediaCodec_releaseOutputBuffer(codec_, outputIndex, false);
            return std::make_optional<AudioFrame>();
        }

        size_t bufSize = 0;
        uint8_t* buf = AMediaCodec_getOutputBuffer(codec_, outputIndex, &bufSize);
        AudioFrame frame;
        frame.channels = format_.channels;
        frame.presentationTimeUs = info.presentationTimeUs;
        frame.isEndOfStream = false;

        size_t sampleCount = info.size / sizeof(int16_t);
        frame.samples.resize(sampleCount);
        int16_t* samples16 = reinterpret_cast<int16_t*>(buf);
        for (size_t i = 0; i < sampleCount; ++i) {
            frame.samples[i] = static_cast<float>(samples16[i]) / 32768.0f;
        }

        AMediaCodec_releaseOutputBuffer(codec_, outputIndex, false);
        return frame;
    }

    return std::nullopt;
}

bool AudioDecoder::Seek(int64_t timeUs) {
    if (!extractor_) return false;
    AMediaExtractor_seekTo(extractor_, timeUs, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
    sawEOS_ = false;
    return true;
}

// ============================================================================
// AudioEngine
// ============================================================================

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() { Shutdown(); }

bool AudioEngine::Initialize(GraphicsDevice* device) {
    (void)device;
    mixer_ = std::make_unique<AudioMixer>();
    analyzer_ = std::make_unique<AudioAnalyzer>();
    analysisRunning_.store(true);
    analysisThread_ = std::thread(&AudioEngine::AnalysisThreadMain, this);
    LOGI("AudioEngine initialized");
    return true;
}

void AudioEngine::Shutdown() {
    analysisRunning_.store(false);
    analysisCV_.notify_all();
    if (analysisThread_.joinable()) analysisThread_.join();

    std::lock_guard<std::mutex> lock(mutex_);
    decoders_.clear();
    clips_.clear();
    clipOrder_.clear();
    mixer_.reset();
    analyzer_.reset();
}

std::string AudioEngine::LoadAudio(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "audio_" + std::to_string(std::hash<std::string>{}(path));
    auto decoder = std::make_unique<AudioDecoder>();
    if (!decoder->Open(path)) return "";

    std::string audioId = "audio_" + std::to_string(reinterpret_cast<uintptr_t>(decoder.get()));
    decoders_[audioId] = std::move(decoder);
    LOGI("Loaded audio: %s -> %s", path.c_str(), audioId.c_str());
    return audioId;
}

AudioClip* AudioEngine::CreateClip(const std::string& audioId, int64_t startUs, int64_t endUs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = decoders_.find(audioId);
    if (it == decoders_.end()) return nullptr;

    std::string clipId = "clip_" + std::to_string(clips_.size());
    auto clip = std::make_unique<AudioClip>();
    clip->clipId = clipId;
    clip->sourcePath = audioId;
    clip->startTimeUs = startUs;
    clip->endTimeUs = endUs;

    if (auto* dec = it->second.get()) {
        if (endUs == 0) clip->endTimeUs = dec->GetDurationUs();
        mixer_->AddInput(clipId, dec->GetFormat().channels);
    }

    AudioClip* ptr = clip.get();
    clips_[clipId] = std::move(clip);
    clipOrder_.push_back(clipId);
    return ptr;
}

void AudioEngine::RemoveClip(const std::string& clipId) {
    std::lock_guard<std::mutex> lock(mutex_);
    mixer_->RemoveInput(clipId);
    clips_.erase(clipId);
    clipOrder_.erase(std::remove(clipOrder_.begin(), clipOrder_.end(), clipId), clipOrder_.end());
}

AudioClip* AudioEngine::GetClip(const std::string& clipId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = clips_.find(clipId); it != clips_.end()) return it->second.get();
    return nullptr;
}

const std::vector<AudioClip*>& AudioEngine::GetAllClips() const {
    static std::vector<AudioClip*> empty;
    return empty;
}

void AudioEngine::SetPlaybackTime(double timeSec) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentTimeSec_ = timeSec;
}

void AudioEngine::SetPlaybackSpeed(float speed) { playbackSpeed_ = std::clamp(speed, 0.1f, 4.0f); }

void AudioEngine::SetMasterVolume(float volume) { masterVolume_ = std::clamp(volume, 0.0f, 2.0f); }

std::vector<float> AudioEngine::GetMixedAudio(double timeSec, double durationSec) {
    size_t frameCount = static_cast<size_t>(durationSec * mixer_ ? mixer_->masterVolume_ : 1.0f * 48000);
    if (!mixer_) return {};
    return mixer_->RenderMix(timeSec, timeSec + durationSec);
}

SpectrumData AudioEngine::GetCurrentSpectrum() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!analyzer_) return {};
    return analyzer_->AnalyzeSpectrum(nullptr, 0, 2);
}

BeatInfo AudioEngine::GetCurrentBeatInfo() {
    std::lock_guard<std::mutex> lock(mutex_);
    return BeatInfo{};
}

std::vector<float> AudioEngine::GetCurrentWaveform(size_t points) {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<float>(points, 0.0f);
}

float AudioEngine::GetAudioLevel(const std::string& clipId, float frequency) {
    (void)clipId;
    (void)frequency;
    return 0.0f;
}

float AudioEngine::GetBeatPhase(const std::string& clipId) {
    (void)clipId;
    return 0.0f;
}

bool AudioEngine::IsOnBeat(const std::string& clipId, float threshold) {
    (void)clipId;
    (void)threshold;
    return false;
}

void AudioEngine::Update(double deltaTime) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentTimeSec_ += deltaTime * playbackSpeed_;
    UpdateClipPositions();
}

void AudioEngine::AnalysisThreadMain() {
    while (analysisRunning_.load(std::memory_order_acquire)) {
        std::unique_lock<std::mutex> lock(analysisMutex_);
        analysisCV_.wait_for(lock, std::chrono::milliseconds(50));
    }
}

void AudioEngine::UpdateClipPositions() {
    for (const auto& id : clipOrder_) {
        if (auto it = clips_.find(id); it != clips_.end()) {
            AudioClip* clip = it->second.get();
            if (currentTimeSec_ * 1e6 >= clip->endTimeUs && clip->endTimeUs > 0) {
                clip->mute = true;
            }
        }
    }
}

} // namespace vfx
