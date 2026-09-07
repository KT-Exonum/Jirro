#pragma once
// Audio Engine: decoding, mixing, analysis (beat detection, spectrum, waveform)
// Integrates with MediaEngine for video audio tracks and standalone audio files

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

#include "engine/core/GraphicsDevice.h"
#include "AudioOutput.h"

namespace vfx {

// Audio format info
struct AudioFormat {
    int sampleRate = 48000;
    int channels = 2;
    int64_t durationUs = 0; // microseconds
    std::string mimeType;
};

// Decoded audio frame
struct AudioFrame {
    std::vector<float> samples; // interleaved: L, R, L, R...
    int64_t presentationTimeUs = 0;
    bool isEndOfStream = false;
    int channels = 2;

    [[nodiscard]] size_t GetFrameCount() const { return samples.size() / channels; }
};

// Audio clip reference (for timeline)
struct AudioClip {
    std::string clipId;
    std::string sourcePath;
    int64_t startTimeUs = 0;
    int64_t endTimeUs = 0; // 0 = full duration
    float volume = 1.0f;
    float pan = 0.0f; // -1 left, 1 right
    bool mute = false;
    bool solo = false;
    float speed = 1.0f; // playback speed

    // Analysis results (cached)
    std::vector<float> waveform; // downsampled for UI
    std::vector<float> spectrumHistory; // for spectrum display
    std::vector<float> beatTimes; // detected beat times in seconds
};

// Beat detection result
struct BeatInfo {
    float tempo = 120.0f; // BPM
    std::vector<float> beatTimes; // absolute times in seconds
    std::vector<float> beatStrengths; // 0-1
    float confidence = 0.0f;
    int64_t lastBeatTimeUs = 0;
};

// Spectrum analysis
struct SpectrumData {
    std::vector<float> magnitudes; // frequency bins
    std::vector<float> frequencies; // Hz per bin
    float sampleRate = 48000;
    int fftSize = 1024;

    [[nodiscard]] float GetMagnitudeAtHz(float hz) const {
        if (frequencies.empty()) return 0.0f;
        int bin = static_cast<int>(hz / (sampleRate / static_cast<float>(fftSize)) * fftSize);
        bin = std::clamp(bin, 0, static_cast<int>(magnitudes.size()) - 1);
        return magnitudes[bin];
    }
};

// Audio analyzer for real-time analysis
class AudioAnalyzer {
public:
    explicit AudioAnalyzer(int sampleRate = 48000, int fftSize = 1024);

    // Process audio buffer, returns spectrum
    SpectrumData AnalyzeSpectrum(std::span<const float> samples, int channels);

    // Beat detection - call periodically with audio data
    std::optional<BeatInfo> DetectBeats(std::span<const float> samples, int channels, double currentTimeSec);

    // Get waveform data for UI (downsampled)
    std::vector<float> GetWaveform(std::span<const float> samples, int channels, size_t targetPoints);

    // Get RMS level
    [[nodiscard]] float GetRMS(std::span<const float> samples, int channels) const;

private:
    int sampleRate_;
    int fftSize_;
    std::vector<float> window_;
    std::vector<float> fftBuffer_;
    std::vector<std::complex<float>> fftComplex_;

    // Beat detection state
    struct BeatState {
        std::vector<float> energyHistory;
        std::vector<float> beatTimes;
        float lastEnergy = 0.0f;
        float avgEnergy = 0.0f;
        int64_t lastBeatSample = 0;
    } beatState_;

    void ComputeFFT(std::span<const float> input);
    void ApplyWindow(std::span<float> buffer);
};

// Audio mixer - combines multiple audio sources
class AudioMixer {
public:
    struct MixInput {
        std::string id;
        std::vector<float> buffer; // ring buffer of audio
        size_t readPos = 0;
        size_t writePos = 0;
        float volume = 1.0f;
        float pan = 0.0f;
        bool mute = false;
        int channels = 2;
        int sampleRate = 48000;
    };

    AudioMixer(int sampleRate = 48000, int channels = 2, int bufferFrames = 48000);

    // Add a mix input (audio track)
    void AddInput(const std::string& id, int channels = 2);
    void RemoveInput(const std::string& id);

    // Write audio to an input (called by decoders)
    bool WriteInput(const std::string& id, std::span<const float> samples);

    // Set input parameters
    void SetInputVolume(const std::string& id, float volume);
    void SetInputPan(const std::string& id, float pan);
    void SetInputMute(const std::string& id, bool mute);

    // Mix all inputs into output buffer
    // Returns number of frames mixed
    size_t Mix(std::span<float> output);

    // Get mixed audio for export
    std::vector<float> RenderMix(double startTimeSec, double endTimeSec);

    // Master output
    float masterVolume_ = 1.0f;
    float masterPan_ = 0.0f;

private:
    int sampleRate_;
    int channels_;
    int bufferFrames_;
    std::unordered_map<std::string, MixInput> inputs_;
    std::mutex mutex_;
};

// Audio decoder using MediaCodec
class AudioDecoder {
public:
    AudioDecoder();
    ~AudioDecoder();

    std::expected<void, std::string> Open(std::string_view path);
    std::expected<void, std::string> OpenFromMediaExtractor(AMediaExtractor* extractor, int trackIndex);
    void Close();

    [[nodiscard]] bool IsOpen() const { return codec_ != nullptr; }
    [[nodiscard]] AudioFormat GetFormat() const { return format_; }

    // Decode next frame
    std::optional<AudioFrame> DecodeFrame();

    // Seek to time
    bool Seek(int64_t timeUs);

    // Get duration
    [[nodiscard]] int64_t GetDurationUs() const { return format_.durationUs; }

private:
    AMediaExtractor* extractor_ = nullptr;
    AMediaCodec* codec_ = nullptr;
    AMediaFormat* format_ = nullptr;
    int trackIndex_ = -1;
    AudioFormat format_;
    bool sawEOS_ = false;

    std::vector<uint8_t> inputBuffer_;
    std::vector<float> outputBuffer_;
};

// Audio engine - main entry point
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // Initialize with graphics device (for compute-based analysis)
    bool Initialize(GraphicsDevice* device = nullptr);
    void Shutdown();

    // Load audio file
    std::string LoadAudio(std::string_view path);

    // Create audio clip for timeline
    AudioClip* CreateClip(const std::string& audioId, int64_t startUs = 0, int64_t endUs = 0);
    void RemoveClip(const std::string& clipId);

    // Get clip
    [[nodiscard]] AudioClip* GetClip(const std::string& clipId) const;
    [[nodiscard]] std::vector<AudioClip*> GetAllClips() const;

    // Playback control
    void SetPlaybackTime(double timeSec);
    void SetPlaybackSpeed(float speed);
    void SetMasterVolume(float volume);

    // Audio output
    void StartAudioOutput();
    void StopAudioOutput();
    bool IsAudioOutputRunning() const;

    // Get mixed audio for current time (for export/render)
    [[nodiscard]] std::vector<float> GetMixedAudio(double timeSec, double durationSec);

    // Real-time analysis (called from render thread)
    [[nodiscard]] SpectrumData GetCurrentSpectrum();
    [[nodiscard]] BeatInfo GetCurrentBeatInfo();
    [[nodiscard]] std::vector<float> GetCurrentWaveform(size_t points = 512);

    // Audio reactive values (for expression engine / uniform binding)
    [[nodiscard]] float GetAudioLevel(const std::string& clipId, float frequency = 0.0f);
    [[nodiscard]] float GetBeatPhase(const std::string& clipId);
    [[nodiscard]] bool IsOnBeat(const std::string& clipId, float threshold = 0.5f);

    // Update - called each frame
    void Update(double deltaTime);

private:
    std::unique_ptr<AudioMixer> mixer_;
    std::unique_ptr<AudioAnalyzer> analyzer_;
    std::unique_ptr<AudioOutput> audioOutput_;
    std::unordered_map<std::string, std::unique_ptr<AudioDecoder>> decoders_;
    std::unordered_map<std::string, std::unique_ptr<AudioClip>> clips_;
    std::vector<std::string> clipOrder_;
    std::atomic<size_t> nextClipId_{0};

    std::mutex mutex_;
    double currentTimeSec_ = 0.0;
    float playbackSpeed_ = 1.0f;
    float masterVolume_ = 1.0f;
    bool audioOutputRunning_ = false;

    // Analysis thread
    std::jthread analysisThread_;
    std::stop_source analysisStopSource_;
    std::condition_variable analysisCV_;
    std::mutex analysisMutex_;

    void AnalysisThreadMain(std::stop_token stopToken);
    void UpdateClipPositions();
};

} // namespace vfx
