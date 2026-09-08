#pragma once
// Section 7. The engine is driven by this timeline clock, not decoder
// playback timing — Render(time) must be deterministic (Phase 4 success
// criterion). Playback (play/pause/speed/reverse) just advances a
// TimelineTime that every subsystem (RenderGraph inputs, KeyframeTrack
// evaluation, MediaEngine frame selection) reads from; nothing free-runs off
// wall-clock or decoder PTS on its own.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Keyframe.h"

namespace vfx {

struct TimelineTime {
    int64_t microseconds = 0;
    double seconds = 0.0;
    int64_t frameIndex = 0; // at the timeline's authoritative frame rate

    static TimelineTime FromSeconds(double s, double frameRate) {
        TimelineTime t;
        t.seconds = s;
        t.microseconds = static_cast<int64_t>(s * 1'000'000.0);
        t.frameIndex = static_cast<int64_t>(std::llround(s * frameRate));
        return t;
    }
    static TimelineTime FromFrame(int64_t frame, double frameRate) {
        return FromSeconds(static_cast<double>(frame) / frameRate, frameRate);
    }
};

enum class PlaybackState { Stopped, Playing, Scrubbing };

enum class ClipType { Video, Audio, Image, Vector, Text, Stroke };

// A placed instance of a source (video/image/audio/generator) on the timeline.
// Node-graph identity (`sourceNodeId`) is separate from timeline placement
// so the same node-graph composition can be reused across multiple clips
// (e.g. one "color grade" subgraph applied to many clips).
struct Clip {
    std::string clipId;
    std::string sourceNodeId; // resolves into the Render Graph, see Node.h
    ClipType type = ClipType::Video;
    double timelineStart = 0.0;  // seconds, position on the timeline
    double sourceInPoint = 0.0;  // seconds, trim in-point within the source
    double sourceOutPoint = 0.0; // seconds, trim out-point within the source
    double playbackSpeed = 1.0;  // Section 7: variable speed; negative = reverse
    int layer = 0;               // compositing order, higher draws on top
    bool enabled = true;
    bool locked = false;

    [[nodiscard]] double Duration() const {
        return (sourceOutPoint - sourceInPoint) / std::abs(playbackSpeed);
    }

    [[nodiscard]] bool ContainsTimelineTime(double t) const {
        if (!enabled) return false;
        const double duration = Duration();
        return t >= timelineStart && t < timelineStart + duration;
    }

    // Maps a timeline-space timestamp to the corresponding timestamp inside
    // the source media, honoring trim, speed, and direction.
    [[nodiscard]] double ToSourceTime(double timelineT) const {
        const double elapsed = (timelineT - timelineStart) * playbackSpeed;
        return sourceInPoint + elapsed;
    }
};

struct Transition {
    std::string fromClipId;
    std::string toClipId;
    double duration = 0.0;
    std::string blendShaderNodeId; // Section 10 blend-mode node used during the crossfade window
};

// Pure data + query model — does not own decoders or GPU resources. Media
// residency (which frames are decoded/cached right now) is MediaEngine's
// job (engine/media/MediaEngine.h), driven by whatever Timeline reports as
// "active near the current time".
class Timeline {
public:
    explicit Timeline(double frameRate) : frameRate_(frameRate) {}

    void AddClip(Clip clip) { clips_.push_back(std::move(clip)); }
    void RemoveClip(const std::string& clipId) {
        std::erase_if(clips_, [&](const Clip& c) { return c.clipId == clipId; });
    }
    void AddTransition(Transition t) { transitions_.push_back(std::move(t)); }

    [[nodiscard]] double FrameRate() const { return frameRate_; }

    void Seek(double seconds) {
        currentTime_ = TimelineTime::FromSeconds(std::max(0.0, seconds), frameRate_);
    }

    void SetPlaybackState(PlaybackState s) { state_ = s; }
    [[nodiscard]] PlaybackState State() const { return state_; }

    // Advances the clock by wall-clock delta scaled by masterSpeed (Section
    // 7 "variable playback speed"), negative masterSpeed => reverse.
    // Individual clip speed (Clip::playbackSpeed) further scales within
    // ToSourceTime and is independent of this master transport speed.
    void Advance(double wallClockDeltaSeconds, double masterSpeed) {
        if (state_ != PlaybackState::Playing) return;
        Seek(currentTime_.seconds + wallClockDeltaSeconds * masterSpeed);
    }

    [[nodiscard]] const TimelineTime& CurrentTime() const { return currentTime_; }

    // Section 7's core contract: "Render(time = 12.483s) produces the
    // composition at that exact position" — this is the read side of that;
    // RenderGraph (Phase 3) consumes ActiveClipsAt() plus per-clip
    // KeyframeTracks to build the frame's node inputs.
    [[nodiscard]] std::vector<const Clip*> ActiveClipsAt(double timelineT) const {
        std::vector<const Clip*> active;
        for (const auto& c : clips_) {
            if (c.ContainsTimelineTime(timelineT)) active.push_back(&c);
        }
        std::sort(active.begin(), active.end(),
                  [](const Clip* a, const Clip* b) { return a->layer < b->layer; });
        return active;
    }

    [[nodiscard]] std::vector<const Clip*> ActiveClipsOfTypeAt(double timelineT, ClipType type) const {
        std::vector<const Clip*> active;
        for (const auto& c : clips_) {
            if (c.type == type && c.ContainsTimelineTime(timelineT)) active.push_back(&c);
        }
        std::sort(active.begin(), active.end(),
                  [](const Clip* a, const Clip* b) { return a->layer < b->layer; });
        return active;
    }

    [[nodiscard]] const Transition* ActiveTransitionAt(double timelineT) const {
        for (const auto& t : transitions_) {
            const Clip* from = FindClip(t.fromClipId);
            const Clip* to = FindClip(t.toClipId);
            if (!from || !to) continue;
            const double overlapStart = to->timelineStart;
            const double overlapEnd = overlapStart + t.duration;
            if (timelineT >= overlapStart && timelineT < overlapEnd) return &t;
        }
        return nullptr;
    }

    [[nodiscard]] const Clip* FindClip(const std::string& id) const {
        for (const auto& c : clips_) if (c.clipId == id) return &c;
        return nullptr;
    }
    Clip* FindClipMutable(const std::string& id) {
        for (auto& c : clips_) if (c.clipId == id) return &c;
        return nullptr;
    }

    [[nodiscard]] const std::vector<Clip>& AllClips() const { return clips_; }
    [[nodiscard]] const std::vector<Transition>& AllTransitions() const { return transitions_; }

    // ========================================================================
    // Clipboard for relative copy/paste of keyframes and effects
    // ========================================================================

    // Stores copied clip data with source duration for relative paste
    struct ClipboardData {
        std::string sourceClipId;
        double sourceClipDuration = 0.0;  // Source clip timeline duration
        std::unordered_map<std::string, KeyframeTrack> animatedUniforms; // uniformName -> track
        std::unordered_map<std::string, float> staticUniforms; // uniformName -> value
        double sourceStartTime = 0.0; // Timeline position of source clip
    };

    // Split a clip at the given timeline position
    bool SplitClip(const std::string& clipId, double timelinePosition) {
        Clip* clip = FindClipMutable(clipId);
        if (!clip) return false;

        if (timelinePosition <= clip->timelineStart || timelinePosition >= clip->timelineStart + clip->Duration()) {
            return false;
        }

        double sourceTime = clip->ToSourceTime(timelinePosition);
        double sourceDuration = clip->sourceOutPoint - clip->sourceInPoint;
        double firstPartSourceDuration = sourceTime - clip->sourceInPoint;

        Clip secondClip = *clip;
        secondClip.clipId = clipId + "_split_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        secondClip.timelineStart = timelinePosition;
        secondClip.sourceInPoint = sourceTime;
        secondClip.sourceOutPoint = clip->sourceOutPoint;

        clip->sourceOutPoint = sourceTime;

        AddClip(std::move(secondClip));
        return true;
    }

    // Trim a clip's in/out points
    bool TrimClip(const std::string& clipId, double newSourceIn, double newSourceOut) {
        Clip* clip = FindClipMutable(clipId);
        if (!clip) return false;

        if (newSourceIn >= newSourceOut) return false;
        if (newSourceIn < 0) return false;

        clip->sourceInPoint = newSourceIn;
        clip->sourceOutPoint = newSourceOut;
        return true;
    }

    // Create a transition between two clips
    bool CreateTransition(const std::string& fromClipId, const std::string& toClipId, double duration, const std::string& blendShaderNodeId) {
        const Clip* from = FindClip(fromClipId);
        Clip* to = FindClipMutable(toClipId);
        if (!from || !to) return false;

        Transition t;
        t.fromClipId = fromClipId;
        t.toClipId = toClipId;
        t.duration = duration;
        t.blendShaderNodeId = blendShaderNodeId;

        // Position the "to" clip to overlap with "from"
        to->timelineStart = from->timelineStart + from->Duration() - duration;

        AddTransition(std::move(t));
        return true;
    }

    // Delete a clip
    bool DeleteClip(const std::string& clipId) {
        auto it = std::find_if(clips_.begin(), clips_.end(),
                               [&](const Clip& c) { return c.clipId == clipId; });
        if (it == clips_.end()) return false;

        clips_.erase(it);

        // Also remove any transitions involving this clip
        std::erase_if(transitions_, [&](const Transition& t) {
            return t.fromClipId == clipId || t.toClipId == clipId;
        });

        return true;
    }

    // Move a clip to a new timeline position
    bool MoveClip(const std::string& clipId, double newTimelineStart) {
        Clip* clip = FindClipMutable(clipId);
        if (!clip) return false;
        if (newTimelineStart < 0) return false;

        clip->timelineStart = newTimelineStart;
        return true;
    }

    // Change clip layer (compositing order)
    bool SetClipLayer(const std::string& clipId, int newLayer) {
        Clip* clip = FindClipMutable(clipId);
        if (!clip) return false;

        clip->layer = newLayer;
        return true;
    }

    // Get total timeline duration
    double GetTotalDuration() const {
        double maxEnd = 0.0;
        for (const auto& c : clips_) {
            double end = c.timelineStart + c.Duration();
            if (end > maxEnd) maxEnd = end;
        }
        return maxEnd;
    }

    // Get clips that overlap with a time range
    std::vector<const Clip*> ClipsInRange(double startT, double endT) const {
        std::vector<const Clip*> result;
        for (const auto& c : clips_) {
            if (!c.enabled) continue;
            double clipEnd = c.timelineStart + c.Duration();
            if (clipEnd > startT && c.timelineStart < endT) {
                result.push_back(&c);
            }
        }
        std::sort(result.begin(), result.end(),
                  [](const Clip* a, const Clip* b) { return a->layer < b->layer; });
        return result;
    }

    // Copy all animatable data from a clip to clipboard
    // Returns true if successful
    bool CopyClipToClipboard(const std::string& clipId, ClipboardData& outData) {
        const Clip* clip = FindClip(clipId);
        if (!clip) return false;

        outData.sourceClipId = clipId;
        outData.sourceClipDuration = clip->Duration();
        outData.sourceStartTime = clip->timelineStart;
        outData.staticUniforms.clear();
        outData.animatedUniforms.clear();

        // Note: The actual keyframe data lives in the NodeGraph (per-node animatedUniforms)
        // This Timeline-level clipboard is for clip-level properties.
        // The EditorState (Kotlin) handles the NodeGraph keyframe clipboard.
        
        return true;
    }

    // Paste clipboard data to target clip with relative scaling
    // Scales keyframe times proportionally: targetTime = keyframeTime * (targetDuration / sourceDuration)
    // Returns true if successful
    bool PasteClipboardToClip(const std::string& targetClipId, const ClipboardData& data) {
        Clip* targetClip = FindClipMutable(targetClipId);
        if (!targetClip) return false;
        if (data.sourceClipDuration <= 0.0) return false;

        double targetDuration = targetClip->Duration();
        if (targetDuration <= 0.0) return false;

        // Calculate scaling factor for relative paste
        double scale = targetDuration / data.sourceClipDuration;

        // In a full implementation, this would:
        // 1. Copy static uniform values to target node
        // 2. Scale and copy animated keyframe tracks:
        //    for each keyframe in source track:
        //      newTime = (keyframe.time - sourceClipStart) * scale + targetClipStart
        //      targetTrack.addKeyframe(newTime, keyframe.value, keyframe.interpolation, ...)

        // The actual keyframe scaling is done at Kotlin level where we have access to EditorState
        return true;
    }

    // Get clipboard data (for UI preview)
    [[nodiscard]] std::optional<ClipboardData> GetClipboard() const { return clipboard_; }

    // Clear clipboard
    void ClearClipboard() { clipboard_.reset(); }

private:
    double frameRate_;
    TimelineTime currentTime_{};
    PlaybackState state_ = PlaybackState::Stopped;
    std::vector<Clip> clips_;
    std::vector<Transition> transitions_;
    
    // Clipboard storage
    std::optional<ClipboardData> clipboard_;
};

} // namespace vfx
