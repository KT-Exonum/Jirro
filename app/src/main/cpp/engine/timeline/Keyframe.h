#pragma once
// Section 11. Pure math, no GPU/JNI dependency, so this is fully implemented
// rather than stubbed — there's no reason to defer it to a later phase.
// Shader uniforms, transforms, colors, and effect params all animate through
// KeyframeTrack<float>; compose higher-dimensional properties (e.g. a
// position) as several tracks (x, y) rather than templating on vector types,
// which keeps evaluation trivial and cache-friendly.

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace vfx {

enum class InterpolationType {
    Step,     // holds previous keyframe's value until the next timestamp
    Linear,
    Bezier,   // cubic bezier through per-keyframe tangent handles
    Custom,   // user-supplied easing function, see Keyframe::customEase
};

struct BezierHandle {
    double timeOffset = 0.0;  // seconds, relative to the keyframe's own time
    float valueOffset = 0.0f;
};

struct Keyframe {
    double timestamp = 0.0; // seconds
    float value = 0.0f;
    InterpolationType interpolation = InterpolationType::Linear;

    // Only used when interpolation == Bezier. outHandle belongs to this
    // keyframe (leaving it), inHandle belongs to the *next* keyframe's
    // incoming tangent — stored here for authoring convenience so a node
    // editor can drag both handles of a given control point together.
    BezierHandle outHandle{};
    BezierHandle nextInHandle{};

    // Only used when interpolation == Custom: maps normalized t in [0,1]
    // between this keyframe and the next to eased t in [0,1].
    std::function<double(double)> customEase;
};

class KeyframeTrack {
public:
    void AddKeyframe(Keyframe kf) {
        auto it = std::lower_bound(keyframes_.begin(), keyframes_.end(), kf.timestamp,
                                    [](const Keyframe& a, double t) { return a.timestamp < t; });
        if (it != keyframes_.end() && it->timestamp == kf.timestamp) {
            *it = std::move(kf); // replace existing keyframe at same time
        } else {
            keyframes_.insert(it, std::move(kf));
        }
    }

    void RemoveKeyframeAt(double timestamp) {
        std::erase_if(keyframes_, [timestamp](const Keyframe& k) {
            return std::abs(k.timestamp - timestamp) < 1e-9;
        });
    }

    [[nodiscard]] bool Empty() const { return keyframes_.empty(); }
    [[nodiscard]] size_t Count() const { return keyframes_.size(); }
    [[nodiscard]] const std::vector<Keyframe>& Keyframes() const { return keyframes_; }

    // Deterministic: same timestamp always yields the same value, required
    // by Phase 4's success criterion (arbitrary-timestamp render must be
    // reproducible for scrubbing, export, and re-renders after edits).
    [[nodiscard]] float Evaluate(double timestamp) const {
        if (keyframes_.empty()) return 0.0f;
        if (keyframes_.size() == 1 || timestamp <= keyframes_.front().timestamp) {
            return keyframes_.front().value;
        }
        if (timestamp >= keyframes_.back().timestamp) return keyframes_.back().value;

        // Find the bracketing pair [a, b] with a.timestamp <= timestamp < b.timestamp.
        auto it = std::upper_bound(keyframes_.begin(), keyframes_.end(), timestamp,
                                    [](double t, const Keyframe& k) { return t < k.timestamp; });
        const Keyframe& b = *it;
        const Keyframe& a = *(it - 1);

        const double span = b.timestamp - a.timestamp;
        const double t = span > 0.0 ? (timestamp - a.timestamp) / span : 0.0;

        switch (a.interpolation) {
            case InterpolationType::Step:
                return a.value;
            case InterpolationType::Linear:
                return static_cast<float>(a.value + (b.value - a.value) * t);
            case InterpolationType::Bezier:
                return EvaluateBezier(a, b, t);
            case InterpolationType::Custom: {
                const double easedT = a.customEase ? a.customEase(t) : t;
                return static_cast<float>(a.value + (b.value - a.value) * easedT);
            }
        }
        return a.value;
    }

private:
    // Cubic bezier in (time, value) space using the authored handles,
    // solved for value at parametric t via De Casteljau (numerically stable,
    // no Newton-Raphson root-find needed since we already have t from time
    // for the common case of roughly-monotonic-in-time handles; a node
    // editor should clamp handle time offsets to keep the curve a function
    // of time, same constraint most NLEs impose).
    [[nodiscard]] static float EvaluateBezier(const Keyframe& a, const Keyframe& b, double t) {
        const float p0 = a.value;
        const float p1 = a.value + a.outHandle.valueOffset;
        const float p2 = b.value + b.nextInHandle.valueOffset;
        const float p3 = b.value;

        const double u = 1.0 - t;
        const double w0 = u * u * u;
        const double w1 = 3.0 * u * u * t;
        const double w2 = 3.0 * u * t * t;
        const double w3 = t * t * t;

        return static_cast<float>(w0 * p0 + w1 * p1 + w2 * p2 + w3 * p3);
    }

    std::vector<Keyframe> keyframes_; // kept sorted by timestamp
};

} // namespace vfx
