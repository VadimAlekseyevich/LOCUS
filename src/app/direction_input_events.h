#pragma once

#include "core/route.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace locus::app {

// Physical transitions, captured before the frame-level raylib key state
// collapses repeated key transitions. Times use the same monotonic origin
// as the gameplay audio clock. Release edges prevent auto-repeat / ghost hits.
struct DirectionKeyEvent {
    core::Heading arrow = core::Heading::Up;
    bool pressed = false;
    double capturedAt = 0.0;
    std::uint64_t order = 0;
};

struct DirectionKeyBatch {
    static constexpr std::size_t kCapacity = 256;
    std::array<DirectionKeyEvent, kCapacity> events{};
    std::size_t size = 0;

    void Clear() noexcept { size = 0; }
    bool Empty() const noexcept { return size == 0; }
};

// Single-threaded (same window thread as the GLFW pump), fixed-capacity,
// allocation-free hot path. Key bindings are the configured PLAY directions;
// menu/editor shortcuts stay with raylib's normal key handling.
class DirectionInputEvents {
public:
    void Configure(const std::array<int, 4>& bindings) noexcept {
        if (configured_ && bindings == keys_) return;
        keys_ = bindings;
        configured_ = true;
        Reset();
    }

    bool Observe(int key, bool pressed, double at,
                 bool osRepeat = false) noexcept {
        if (!configured_ || !std::isfinite(at)) return false;
        static constexpr std::array<core::Heading, 4> headings{{
            core::Heading::Up, core::Heading::Right,
            core::Heading::Down, core::Heading::Left
        }};
        for (std::size_t lane = 0; lane < keys_.size(); ++lane) {
            if (key != keys_[lane]) continue;
            if (suppressed_) {
                if (!pressed) held_[lane] = false;
                if (std::none_of(held_.begin(), held_.end(),
                                 [](bool down) { return down; }))
                    suppressed_ = false;
                return false;
            }
            if (held_[lane] == pressed || (pressed && osRepeat)) return false;
            held_[lane] = pressed;
            if (pending_.size == DirectionKeyBatch::kCapacity) {
                // The burst cannot be reconstructed after overflow. Do NOT
                // deliver a partial group that might create false judgements.
                pending_.Clear();
                suppressed_ = true;
                overflowed_ = true;
                return false;
            }
            pending_.events[pending_.size++] = {
                headings[lane], pressed, at, nextOrder_++
            };
            return true;
        }
        return false;
    }

    DirectionKeyBatch Drain() noexcept {
        DirectionKeyBatch events = pending_;
        pending_.Clear();
        std::sort(events.events.begin(),
                  events.events.begin() + events.size,
                  [](const DirectionKeyEvent& a, const DirectionKeyEvent& b) {
                      return a.capturedAt < b.capturedAt ||
                          (a.capturedAt == b.capturedAt && a.order < b.order);
                  });
        return events;
    }

    // Menu, pause and modal transitions consume no buffered scored input.
    // The held state is retained so an already-down key cannot re-trigger.
    void DiscardPending() noexcept { pending_.Clear(); }

    // On lost window focus, the OS may never send the corresponding key-up.
    void Reset() noexcept {
        pending_.Clear();
        held_.fill(false);
        suppressed_ = false;
        overflowed_ = false;
    }

    bool Overflowed() const noexcept { return overflowed_; }
    const std::array<bool, 4>& Held() const noexcept { return held_; }

private:
    std::array<int, 4> keys_{};
    std::array<bool, 4> held_{};
    DirectionKeyBatch pending_{};
    std::uint64_t nextOrder_ = 0;
    bool configured_ = false;
    bool suppressed_ = false;
    bool overflowed_ = false;
};

} // namespace locus::app
