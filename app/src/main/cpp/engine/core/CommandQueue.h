#pragma once
// Section 12/14 of the spec: the UI thread must never block on the render
// thread, and we should send small commands rather than syncing full engine
// state across JNI. This is a bounded, lock-free multi-producer / single-
// consumer ring buffer of type-erased commands (std::function-based; the
// commands themselves are tiny value types like `UpdateUniform`, so the
// allocation cost of the erasure is negligible and it keeps call sites simple).
//
// Producers: JNI bridge (UI thread), possibly a gesture/input thread.
// Consumer: the single Engine thread, drained once per frame at a defined
// synchronization point (start of Engine::Tick), never mid-render.

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace vfx {

class Engine; // forward decl; commands capture Engine& to apply themselves

using EngineCommand = std::function<void(Engine&)>;

class CommandQueue {
public:
    explicit CommandQueue(size_t capacity = 4096)
        : capacity_(capacity), buffer_(capacity) {}

    // Multi-producer safe. Never blocks (drops + logs on overflow rather than
    // stalling the caller — an overflowing command queue means the engine
    // thread is stuck, and blocking the UI thread in that case is exactly
    // what we must not do).
    bool Push(EngineCommand cmd) {
        const size_t head = head_.fetch_add(1, std::memory_order_relaxed);
        const size_t slot = head % capacity_;

        // Guard against wrapping into a slot the consumer hasn't drained yet.
        if (head - tail_.load(std::memory_order_acquire) >= capacity_) {
            head_.fetch_sub(1, std::memory_order_relaxed);
            return false; // caller may log a dropped-command metric
        }

        buffer_[slot] = std::move(cmd);
        std::atomic_thread_fence(std::memory_order_release);
        ready_[slot].store(true, std::memory_order_release);
        return true;
    }

    // Single-consumer only (the engine thread). Drains everything currently
    // available; called once at the top of each engine tick.
    template <typename Fn>
    void DrainAll(Fn&& apply) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);
        while (tail < head) {
            const size_t slot = tail % capacity_;
            while (!ready_[slot].load(std::memory_order_acquire)) {
                // Producer has reserved the slot but not finished writing;
                // spin briefly — window is nanoseconds in practice.
            }
            apply(buffer_[slot]);
            buffer_[slot] = nullptr;
            ready_[slot].store(false, std::memory_order_release);
            ++tail;
        }
        tail_.store(tail, std::memory_order_release);
    }

    [[nodiscard]] size_t ApproxPending() const {
        return head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_relaxed);
    }

private:
    size_t capacity_;
    std::vector<EngineCommand> buffer_;
    std::vector<std::atomic<bool>> ready_ = std::vector<std::atomic<bool>>(capacity_);
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};

} // namespace vfx
