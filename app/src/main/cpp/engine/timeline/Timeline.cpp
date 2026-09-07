#include "Timeline.h"

#include <algorithm>
#include <cmath>

namespace vfx {

Timeline::Timeline(double frameRate) : frameRate_(frameRate) {}

void Timeline::AddClip(Clip clip) {
    clips_.push_back(std::move(clip));
}

void Timeline::RemoveClip(const std::string& clipId) {
    std::erase_if(clips_, [&](const Clip& c) { return c.clipId == clipId; });
}

void Timeline::AddTransition(Transition t) {
    transitions_.push_back(std::move(t));
}

double Timeline::FrameRate() const {
    return frameRate_;
}

void Timeline::Seek(double seconds) {
    currentTime_ = TimelineTime::FromSeconds(std::max(0.0, seconds), frameRate_);
}

void Timeline::SetPlaybackState(PlaybackState s) {
    state_ = s;
}

PlaybackState Timeline::State() const {
    return state_;
}

void Timeline::Advance(double wallClockDeltaSeconds, double masterSpeed) {
    if (state_ != PlaybackState::Playing) return;
    Seek(currentTime_.seconds + wallClockDeltaSeconds * masterSpeed);
}

const TimelineTime& Timeline::CurrentTime() const {
    return currentTime_;
}

std::vector<const Clip*> Timeline::ActiveClipsAt(double timelineT) const {
    std::vector<const Clip*> active;
    for (const auto& c : clips_) {
        if (c.ContainsTimelineTime(timelineT)) active.push_back(&c);
    }
    std::sort(active.begin(), active.end(),
              [](const Clip* a, const Clip* b) { return a->layer < b->layer; });
    return active;
}

std::vector<const Clip*> Timeline::ActiveClipsOfTypeAt(double timelineT, ClipType type) const {
    std::vector<const Clip*> active;
    for (const auto& c : clips_) {
        if (c.type == type && c.ContainsTimelineTime(timelineT)) active.push_back(&c);
    }
    std::sort(active.begin(), active.end(),
              [](const Clip* a, const Clip* b) { return a->layer < b->layer; });
    return active;
}

const Transition* Timeline::ActiveTransitionAt(double timelineT) const {
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

const Clip* Timeline::FindClip(const std::string& id) const {
    for (const auto& c : clips_) if (c.clipId == id) return &c;
    return nullptr;
}

Clip* Timeline::FindClipMutable(const std::string& id) {
    for (auto& c : clips_) if (c.clipId == id) return &c;
    return nullptr;
}

const std::vector<Clip>& Timeline::AllClips() const {
    return clips_;
}

const std::vector<Transition>& Timeline::AllTransitions() const {
    return transitions_;
}

// Split a clip at the given timeline position
bool Timeline::SplitClip(const std::string& clipId, double timelinePosition) {
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
bool Timeline::TrimClip(const std::string& clipId, double newSourceIn, double newSourceOut) {
    Clip* clip = FindClipMutable(clipId);
    if (!clip) return false;

    if (newSourceIn >= newSourceOut) return false;
    if (newSourceIn < 0) return false;

    clip->sourceInPoint = newSourceIn;
    clip->sourceOutPoint = newSourceOut;
    return true;
}

// Create a transition between two clips
bool Timeline::CreateTransition(const std::string& fromClipId, const std::string& toClipId, double duration, const std::string& blendShaderNodeId) {
    const Clip* from = FindClip(fromClipId);
    const Clip* to = FindClip(toClipId);
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
bool Timeline::DeleteClip(const std::string& clipId) {
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
bool Timeline::MoveClip(const std::string& clipId, double newTimelineStart) {
    Clip* clip = FindClipMutable(clipId);
    if (!clip) return false;
    if (newTimelineStart < 0) return false;

    clip->timelineStart = newTimelineStart;
    return true;
}

// Change clip layer (compositing order)
bool Timeline::SetClipLayer(const std::string& clipId, int newLayer) {
    Clip* clip = FindClipMutable(clipId);
    if (!clip) return false;

    clip->layer = newLayer;
    return true;
}

// Get total timeline duration
double Timeline::GetTotalDuration() const {
    double maxEnd = 0.0;
    for (const auto& c : clips_) {
        double end = c.timelineStart + c.Duration();
        if (end > maxEnd) maxEnd = end;
    }
    return maxEnd;
}

// Get clips that overlap with a time range
std::vector<const Clip*> Timeline::ClipsInRange(double startT, double endT) const {
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

} // namespace vfx