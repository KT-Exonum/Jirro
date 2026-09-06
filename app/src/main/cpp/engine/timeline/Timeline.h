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

// A placed instance of a source (video/image/generator) on the timeline.
// Node-graph identity (`sourceNodeId`) is separate from timeline placement
// so the same node-graph composition can be reused across multiple clips
// (e.g. one "color grade" subgraph applied to many clips).
struct Clip {
    std::string clipId;
    std::string sourceNodeId; // resolves into the Render Graph, see Node.h
    double timelineStart = 0.0;  // seconds, position on the timeline
    double sourceInPoint = 0.0;  // seconds, trim in-point within the source
    double sourceOutPoint = 0.0; // seconds, trim out-point within the source
    double playbackSpeed = 1.0;  // Section 7: variable speed; negative = reverse
    int layer = 0;               // compositing order, higher draws on top

    [[nodiscard]] bool ContainsTimelineTime(double t) const {
        const double duration = (sourceOutPoint - sourceInPoint) / std::abs(playbackSpeed);
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

    [[nodiscard]] const Transition* ActiveTransitionAt(double timelineT) const {
        for (const auto& t : transitions_) {
            // A transition is active while both its clips are; exact overlap
            // window is derived from clip placement rather than stored
            // redundantly, so edits to clip trim automatically stay consistent.
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

private:
    double frameRate_;
    TimelineTime currentTime_{};
    PlaybackState state_ = PlaybackState::Stopped;
    std::vector<Clip> clips_;
    std::vector<Transition> transitions_;
};

} // namespace vfx
