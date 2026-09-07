#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <android/log.h>

#define LOG_TAG "AudioEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

namespace {
constexpr float kPi = std::numbers::pi_v<float>;

[[assume(n > 1 && (n & (n - 1)) == 0)]]
void FFT(std::span<std::complex<float>> data) {
    const size_t n = data.size();

    for (size_t i = 0, j = 0; i < n; ++i) {
        if (i < j) std::swap(data[i], data[j]);
        size_t bit = n >> 1;
        while (bit && j >= bit) { j -= bit; bit >>= 1; }
        j += bit;
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * kPi / len;
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

// ============================================================================
// AudioAnalyzer
// ============================================================================

AudioAnalyzer::AudioAnalyzer(int sampleRate, int fftSize)
    : sampleRate_(sampleRate), fftSize_(fftSize),
      window_(fftSize),
      fftBuffer_(fftSize),
      fftComplex_(fftSize) {
    for (int i = 0; i < fftSize; ++i) {
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * kPi * i / (fftSize - 1)));
    }
}

void AudioAnalyzer::ApplyWindow(std::span<float> buffer) {
    for (size_t i = 0; i < buffer.size() && i < window_.size(); ++i) {
        buffer[i] *= window_[i];
    }
}

void AudioAnalyzer::ComputeFFT(std::span<const float> input) {
    std::copy(input.begin(), input.end(), fftBuffer_.begin());
    ApplyWindow(fftBuffer_);
    for (int i = 0; i < fftSize_; ++i) {
        fftComplex_[i] = std::complex<float>(fftBuffer_[i], 0.0f);
    }
    FFT(fftComplex_);
}

SpectrumData AudioAnalyzer::AnalyzeSpectrum(std::span<const float> samples, int channels) {
    SpectrumData data;
    data.sampleRate = sampleRate_;
    data.fftSize = fftSize_;
    data.frequencies.resize(fftSize_ / 2);
    data.magnitudes.resize(fftSize_ / 2);

    float binHz = sampleRate_ / static_cast<float>(fftSize_);
    for (int i = 0; i < fftSize_ / 2; ++i) {
        data.frequencies[i] = i * binHz;
    }

    if (samples.size() < static_cast<size_t>(fftSize_)) return data;

    ComputeFFT(samples);
    for (int i = 0; i < fftSize_ / 2; ++i) {
        float mag = std::abs(fftComplex_[i]) / fftSize_;
        data.magnitudes[i] = mag * 2.0f;
    }

    return data;
}

std::optional<BeatInfo> AudioAnalyzer::DetectBeats(std::span<const float> samples, int channels, double currentTimeSec) {
    float rms = GetRMS(samples, channels);
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
    if (isBeat && (currentTimeSec - static_cast<float>(beatState_.lastBeatSample) / static_cast<float>(sampleRate_)) > 0.25) {
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

std::vector<float> AudioAnalyzer::GetWaveform(std::span<const float> samples, int channels, size_t targetPoints) {
    std::vector<float> result(targetPoints, 0.0f);
    if (samples.empty() || targetPoints == 0) return result;

    size_t samplesPerPoint = samples.size() / channels / targetPoints;
    for (size_t i = 0; i < targetPoints; ++i) {
        float maxVal = 0.0f;
        size_t start = i * samplesPerPoint * channels;
        size_t end = std::min(start + samplesPerPoint * channels, samples.size());
        for (size_t j = start; j < end; j += channels) {
            float val = samples[j];
            maxVal = std::max(maxVal, std::abs(val));
        }
        result[i] = maxVal;
    }
    return result;
}

float AudioAnalyzer::GetRMS(std::span<const float> samples, int channels) const {
    if (samples.empty()) return 0.0f;
    float sum = 0.0f;
    size_t frameCount = samples.size() / channels;
    for (size_t i = 0; i < frameCount; ++i) {
        float val = samples[i * channels];
        sum += val * val;
    }
    return std::sqrt(sum / frameCount);
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
    input.startTimeUs = 0;
    input.sourceStartUs = 0;
    input.speed = 1.0f;
}

void AudioMixer::RemoveInput(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    inputs_.erase(id);
}

void AudioMixer::SetInputTimeline(const std::string& id, int64_t timelineStartUs, int64_t sourceStartUs, float speed) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = inputs_.find(id); it != inputs_.end()) {
        it->second.startTimeUs = timelineStartUs;
        it->second.sourceStartUs = sourceStartUs;
        it->second.speed = speed;
    }
}

void AudioMixer::SetInputClipId(const std::string& id, const std::string& clipId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = inputs_.find(id); it != inputs_.end()) {
        it->second.clipId = clipId;
    }
}

bool AudioMixer::WriteInput(const std::string& id, std::span<const float> samples) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = inputs_.find(id);
    if (it == inputs_.end()) return false;

    MixInput& input = it->second;
    size_t writePos = input.writePos;

    for (size_t i = 0; i < samples.size(); i += input.channels) {
        input.buffer[writePos * channels_] = samples[i];
        if (channels_ > 1 && i + 1 < samples.size()) {
            input.buffer[writePos * channels_ + 1] = samples[i + 1];
        }
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

size_t AudioMixer::Mix(std::span<float> output, double timelinePositionSec, double frameDurationSec) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (inputs_.empty()) {
        std::fill(output.begin(), output.end(), 0.0f);
        return output.size() / channels_;
    }

    std::fill(output.begin(), output.end(), 0.0f);

    int64_t timelinePosUs = static_cast<int64_t>(timelinePositionSec * 1'000'000.0);
    int64_t frameDurUs = static_cast<int64_t>(frameDurationSec * 1'000'000.0);

    for (auto& [id, input] : inputs_) {
        if (input.mute) continue;
        
        // Check if this input is active at current timeline position
        int64_t inputStartUs = input.startTimeUs;
        int64_t inputEndUs = inputStartUs + static_cast<int64_t>((input.buffer.size() / channels_) * 1'000'000.0 / sampleRate_ / input.speed);
        
        if (timelinePosUs + frameDurUs < inputStartUs || timelinePosUs > inputEndUs) {
            continue; // Not active at this timeline position
        }
        
        // Calculate how many frames to read based on frame duration and speed
        size_t framesToRead = static_cast<size_t>((frameDurUs * input.speed) * sampleRate_ / 1'000'000.0);
        framesToRead = std::min(framesToRead, output.size() / channels_);
        
        // Calculate read position based on timeline offset
        int64_t offsetUs = timelinePosUs - inputStartUs;
        if (offsetUs < 0) offsetUs = 0;
        size_t readOffset = static_cast<size_t>(offsetUs * sampleRate_ / 1'000'000.0 * input.speed);
        
        // Ensure we don't read past available data
        size_t available = (input.writePos >= input.readPos)
            ? (input.writePos - input.readPos)
            : (bufferFrames_ - input.readPos + input.writePos);
        
        if (readOffset >= available) continue;
        
        size_t actualFrames = std::min(framesToRead, available - readOffset);
        size_t readPos = (input.readPos + readOffset) % bufferFrames_;
        
        for (size_t i = 0; i < actualFrames; ++i) {
            size_t idx = readPos * channels_;
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
            readPos = (readPos + 1) % bufferFrames_;
        }
    }

    for (float& sample : output) {
        sample = std::clamp(sample, -1.0f, 1.0f);
    }

    return output.size() / channels_;
}

std::vector<float> AudioMixer::RenderMix(double startTimeSec, double endTimeSec) {
    size_t totalFrames = static_cast<size_t>((endTimeSec - startTimeSec) * sampleRate_);
    std::vector<float> output(totalFrames * channels_);
    if (!output.empty()) {
        Mix(output, startTimeSec, endTimeSec - startTimeSec);
    }
    return output;
}

// ============================================================================
// AudioDecoder
// ============================================================================

AudioDecoder::AudioDecoder() = default;

AudioDecoder::~AudioDecoder() { Close(); }

std::expected<void, std::string> AudioDecoder::Open(std::string_view path) {
    extractor_ = AMediaExtractor_new();
    if (!extractor_) return std::unexpected("Failed to create media extractor");

    media_status_t status = AMediaExtractor_setDataSource(extractor_, std::string(path).c_str());
    if (status != AMEDIA_OK) {
        LOGE("Failed to open media: %s", std::string(path).c_str());
        Close();
        return std::unexpected("Failed to set data source");
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
                AMediaFormat_delete(fmt);
                Close();
                return std::unexpected("Failed to create decoder");
            }

            media_status_t cs = AMediaCodec_configure(codec_, fmt, nullptr, nullptr, 0);
            if (cs != AMEDIA_OK) {
                LOGE("Failed to configure codec");
                AMediaFormat_delete(fmt);
                Close();
                return std::unexpected("Failed to configure codec");
            }

            AMediaCodec_start(codec_);
            AMediaFormat_delete(fmt);
            return {};
        }
        AMediaFormat_delete(fmt);
    }

    LOGE("No audio track found in %s", std::string(path).c_str());
    Close();
    return std::unexpected("No audio track found");
}

std::expected<void, std::string> AudioDecoder::OpenFromMediaExtractor(AMediaExtractor* extractor, int trackIndex) {
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
        return std::unexpected("Failed to configure codec");
    }
    AMediaCodec_start(codec_);
    AMediaFormat_delete(fmt);
    return {};
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
        if (!buf) return std::nullopt;

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
        if (!buf) return std::nullopt;

        AudioFrame frame;
        frame.channels = format_.channels;
        frame.presentationTimeUs = info.presentationTimeUs;
        frame.isEndOfStream = false;

        size_t sampleCount = info.size / sizeof(int16_t);
        frame.samples.resize(sampleCount);
        int16_t* samples16 = reinterpret_cast<int16_t*>(buf);
        std::ranges::transform(samples16, samples16 + sampleCount, frame.samples.begin(),
            [](int16_t s) { return static_cast<float>(s) / 32768.0f; });

        AMediaCodec_releaseOutputBuffer(codec_, outputIndex, false);
        return frame;
    }

    return std::nullopt;
}

bool AudioDecoder::Seek(int64_t timeUs) {
    if (!extractor_) return false;
    AMediaExtractor_seekTo(extractor_, timeUs, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
    if (codec_) {
        AMediaCodec_flush(codec_);
    }
    sawEOS_ = false;
    return true;
}

// ============================================================================
// AudioEngine
// ============================================================================

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    if (analysisStopSource_.stop_possible()) {
        analysisStopSource_.request_stop();
    }
    analysisCV_.notify_all();
}

bool AudioEngine::Initialize(GraphicsDevice* device) {
    (void)device;
    mixer_ = std::make_unique<AudioMixer>();
    analyzer_ = std::make_unique<AudioAnalyzer>();
    audioOutput_ = std::make_unique<AudioOutput>();
    
    // Set up audio output callback to pull from mixer with timeline sync
    audioOutput_->SetCallback([this](std::span<float> output, int numFrames, double timelinePos, double frameDur) {
        if (mixer_) {
            mixer_->Mix(output, timelinePos, frameDur);
        } else {
            std::fill(output.begin(), output.end(), 0.0f);
        }
    });
    
    // Set timeline provider
    audioOutput_->SetTimelineProvider([this]() { return currentTimeSec_; });
    
    LOGI("AudioEngine initialized");
    return true;
}

void AudioEngine::Shutdown() {
    StopAudioOutput();
    
    analysisStopSource_.request_stop();
    analysisCV_.notify_all();

    std::lock_guard<std::mutex> lock(mutex_);
    decoders_.clear();
    clips_.clear();
    clipOrder_.clear();
    mixer_.reset();
    analyzer_.reset();
    audioOutput_.reset();
}

std::string AudioEngine::LoadAudio(std::string_view path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto decoder = std::make_unique<AudioDecoder>();
    auto result = decoder->Open(path);
    if (!result) {
        LOGE("Failed to load audio: %s", result.error().c_str());
        return {};
    }

    std::string audioId = std::format("audio_{:x}", std::hash<std::string>{}(std::string(path)));
    decoders_[audioId] = std::move(decoder);
    LOGI("Loaded audio: %s -> %s", std::string(path).c_str(), audioId.c_str());
    return audioId;
}

AudioClip* AudioEngine::CreateClip(const std::string& audioId, int64_t startUs, int64_t endUs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = decoders_.find(audioId);
    if (it == decoders_.end()) return nullptr;

    std::string clipId = std::format("clip_{}", nextClipId_.fetch_add(1));
    auto clip = std::make_unique<AudioClip>();
    clip->clipId = clipId;
    clip->sourcePath = audioId;
    clip->startTimeUs = startUs;
    clip->endTimeUs = endUs;

    if (auto* dec = it->second.get()) {
        if (endUs == 0) clip->endTimeUs = dec->GetDurationUs();
        mixer_->AddInput(clipId, dec->GetFormat().channels);
        // Set timeline info for sync
        mixer_->SetInputTimeline(clipId, startUs, 0, clip->speed);
        mixer_->SetInputClipId(clipId, clipId);
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

AudioClip* AudioEngine::GetClip(const std::string& clipId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = clips_.find(clipId); it != clips_.end()) return it->second.get();
    return nullptr;
}

std::vector<AudioClip*> AudioEngine::GetAllClips() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AudioClip*> result;
    result.reserve(clipOrder_.size());
    for (const auto& id : clipOrder_) {
        if (auto it = clips_.find(id); it != clips_.end()) {
            result.push_back(it->second.get());
        }
    }
    return result;
}

void AudioEngine::SetPlaybackTime(double timeSec) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentTimeSec_ = timeSec;
}

void AudioEngine::SetPlaybackSpeed(float speed) { playbackSpeed_ = std::clamp(speed, 0.1f, 4.0f); }

void AudioEngine::SetMasterVolume(float volume) { 
    masterVolume_ = std::clamp(volume, 0.0f, 2.0f);
    if (audioOutput_) audioOutput_->SetVolume(volume);
}

// Audio output
void AudioEngine::StartAudioOutput() {
    if (audioOutput_ && !audioOutputRunning_) {
        if (audioOutput_->Start()) {
            audioOutputRunning_ = true;
            LOGI("Audio output started");
        } else {
            LOGE("Failed to start audio output");
        }
    }
}

void AudioEngine::StopAudioOutput() {
    if (audioOutput_ && audioOutputRunning_) {
        audioOutput_->Stop();
        audioOutputRunning_ = false;
        LOGI("Audio output stopped");
    }
}

bool AudioEngine::IsAudioOutputRunning() const {
    return audioOutputRunning_;
}

std::vector<float> AudioEngine::GetMixedAudio(double timeSec, double durationSec) {
    if (!mixer_) return {};
    return mixer_->RenderMix(timeSec, timeSec + durationSec);
}

SpectrumData AudioEngine::GetCurrentSpectrum() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!analyzer_ || !mixer_) return {};
    
    // Get current mixed audio for spectrum analysis
    // We need a small window of audio around current time
    const double windowSec = 0.1; // 100ms window
    auto mixed = mixer_->RenderMix(currentTimeSec_, currentTimeSec_ + windowSec);
    if (mixed.empty()) return {};
    
    // Analyze the mixed audio
    return analyzer_->AnalyzeSpectrum(mixed, channels_);
}

BeatInfo AudioEngine::GetCurrentBeatInfo() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!analyzer_ || !mixer_) return {};
    
    // Get current mixed audio for beat detection
    const double windowSec = 0.1;
    auto mixed = mixer_->RenderMix(currentTimeSec_, currentTimeSec_ + windowSec);
    if (mixed.empty()) return {};
    
    auto beatInfo = analyzer_->DetectBeats(mixed, channels_, currentTimeSec_);
    if (beatInfo) return *beatInfo;
    return BeatInfo{};
}

std::vector<float> AudioEngine::GetCurrentWaveform(size_t points) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!analyzer_ || !mixer_) return std::vector<float>(points, 0.0f);
    
    // Get current mixed audio for waveform
    const double windowSec = 0.1;
    auto mixed = mixer_->RenderMix(currentTimeSec_, currentTimeSec_ + windowSec);
    if (mixed.empty()) return std::vector<float>(points, 0.0f);
    
    return analyzer_->GetWaveform(mixed, channels_, points);
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
    
    // Decode audio for active clips at current timeline position
    for (const auto& id : clipOrder_) {
        if (auto it = clips_.find(id); it != clips_.end()) {
            AudioClip* clip = it->second.get();
            if (clip->mute) continue;
            
            // Check if clip is active at current time
            int64_t currentTimeUs = static_cast<int64_t>(currentTimeSec_ * 1'000'000.0);
            if (currentTimeUs >= clip->startTimeUs && 
                (clip->endTimeUs == 0 || currentTimeUs < clip->endTimeUs)) {
                
                // Decode audio for this clip at current position
                auto decIt = decoders_.find(clip->sourcePath);
                if (decIt != decoders_.end() && decIt->second) {
                    // Seek to the correct position in the source
                    int64_t sourcePosUs = clip->startTimeUs + static_cast<int64_t>((currentTimeSec_ - clip->startTimeUs / 1'000'000.0) * clip->speed * 1'000'000.0);
                    decIt->second->Seek(sourcePosUs);
                    
                    // Decode frames and feed to mixer
                    // In a real implementation, we'd decode continuously
                    // For now, decode one frame at a time
                    auto frame = decIt->second->DecodeFrame();
                    if (frame) {
                        mixer_->WriteInput(clip->clipId, frame->samples);
                    }
                }
            }
        }
    }
}

void AudioEngine::AnalysisThreadMain(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        std::unique_lock<std::mutex> lock(analysisMutex_);
        analysisCV_.wait_for(lock, std::chrono::milliseconds(50), [this, &stopToken] { return stopToken.stop_requested(); });
    }
}

void AudioEngine::UpdateClipPositions() {
    for (const auto& id : clipOrder_) {
        if (auto it = clips_.find(id); it != clips_.end()) {
            AudioClip* clip = it->second.get();
            bool inRange = currentTimeSec_ * 1e6 >= clip->startTimeUs &&
                          (clip->endTimeUs == 0 || currentTimeSec_ * 1e6 < clip->endTimeUs);
            clip->mute = !inRange;
        }
    }
}

} // namespace vfx
