#pragma once

#include "raylib.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace locus::app {

struct UiFocusItem {
    int id = -1; // Stable within the current screen/dialog.
    Rectangle bounds{};
    bool disabled = false;
};

// Screen controller supplies focus order each frame and owns the input keys.
// Only rectangles are shared with render and mouse hit-testing. This layer
// does NOT poll raylib keys and cannot eat gameplay/editor shortcuts itself.
class UiFocusChain {
public:
    void Begin() { items_.clear(); }

    bool Add(int id, Rectangle bounds, bool disabled = false) {
        if (id < 0 || Has(id)) return false;
        items_.push_back({id, bounds, disabled});
        return true;
    }

    void Clear() noexcept {
        items_.clear();
        focused_ = -1;
    }

    bool Has(int id) const noexcept {
        return std::any_of(items_.begin(), items_.end(),
            [id](const UiFocusItem& item) { return item.id == id; });
    }

    bool Enabled(int id) const noexcept {
        const auto it = std::find_if(items_.begin(), items_.end(),
            [id](const UiFocusItem& item) { return item.id == id; });
        return it != items_.end() && !it->disabled;
    }

    bool Focus(int id) noexcept {
        if (!Enabled(id)) return false;
        focused_ = id;
        return true;
    }

    // Tab/Shift+Tab traversal skips disabled widgets and wraps predictably.
    bool Move(bool backwards = false) noexcept {
        if (items_.empty()) { focused_ = -1; return false; }
        const int size = static_cast<int>(items_.size());
        int current = -1;
        for (int i = 0; i < size; ++i) {
            if (items_[static_cast<std::size_t>(i)].id == focused_) {
                current = i;
                break;
            }
        }
        for (int i = 1; i <= size; ++i) {
            const int next = current < 0
                ? (backwards ? (size - i) : (i - 1))
                : (current + (backwards ? -i : i) + size * 2) % size;
            const auto& candidate = items_[static_cast<std::size_t>(next)];
            if (!candidate.disabled) {
                focused_ = candidate.id;
                return true;
            }
        }
        focused_ = -1;
        return false;
    }

    int Focused() const noexcept { return focused_; }

    // Controller decides when Enter/Space is activation rather than typing.
    std::optional<int> Activate() const noexcept {
        return Enabled(focused_) ? std::optional<int>{focused_} : std::nullopt;
    }

    // If a previously focused widget disappeared or became disabled, do
    // not leave an invisible focus target active between screen updates.
    void Validate() noexcept {
        if (focused_ >= 0 && !Enabled(focused_)) focused_ = -1;
    }

    std::optional<Rectangle> FocusBounds() const noexcept {
        for (const auto& item : items_) {
            if (item.id == focused_ && !item.disabled) return item.bounds;
        }
        return std::nullopt;
    }

private:
    std::vector<UiFocusItem> items_;
    int focused_ = -1;
};

inline bool UiPointerHit(Rectangle bounds, Vector2 point,
                         bool disabled = false) noexcept {
    return !disabled && point.x >= bounds.x && point.y >= bounds.y &&
           point.x < bounds.x + bounds.width &&
           point.y < bounds.y + bounds.height;
}

} // namespace locus::app
