#pragma once

#include "app/chart_editor_layout.h"
#include "core/tempo_map.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace locus::app {

// Pure UI state/input handler. All shared persistence is supplied through
// apply(), so an invalid tempo edit never mutates a chart or a Song Set.
template<class Apply>
bool HandleTempoTable(EditorUiState& ui, const EditorControls& input,
                      const std::vector<core::TempoMarker>& current,
                      const core::ChartEditor& draft, Apply&& apply) {
    if (ui.tab != EditorTab::Audio || ui.active != EditorField::None ||
        ui.editingSetPreview) return false;
    const auto error = [&](const char* message) {
        ui.feedback = message;
        ui.feedbackError = true;
    };
    const int count = static_cast<int>(current.size());
    ui.tempoFirstVisible = std::clamp(ui.tempoFirstVisible, 0,
                                      std::max(0, count - 5));
    if (ui.tempoEditingRow >= 0) {
        if (input.escape) {
            ui.tempoEditingRow = -1;
            ui.tempoBuffer.clear();
            return true;
        }
        if (input.backspace) {
            if (ui.tempoReplaceOnType) ui.tempoBuffer.clear();
            else if (!ui.tempoBuffer.empty()) ui.tempoBuffer.pop_back();
            ui.tempoReplaceOnType = false;
        }
        for (const unsigned char ch : input.typedUtf8) {
            if ((ch < '0' || ch > '9') && ch != '-' &&
                !(ui.tempoEditingBpm && (ch == '.' || ch == ','))) continue;
            if (ui.tempoReplaceOnType) {
                ui.tempoBuffer.clear();
                ui.tempoReplaceOnType = false;
            }
            if (ui.tempoBuffer.size() < 32)
                ui.tempoBuffer.push_back(static_cast<char>(ch));
        }
        if (input.enter || input.tab || input.mouseClick) {
            auto next = current;
            if (ui.tempoEditingRow >= count) {
                error("TEMPO ROW NO LONGER EXISTS");
                return true;
            }
            std::string raw = ui.tempoBuffer;
            if (raw.empty() || raw.size() > 32) {
                error("ENTER A VALID BPM OR MILLISECOND OFFSET");
                return true;
            }
            double parsed = 0.0;
            if (ui.tempoEditingBpm) {
                std::replace(raw.begin(), raw.end(), ',', '.');
                char* end = nullptr;
                errno = 0;
                parsed = std::strtod(raw.c_str(), &end);
                if (errno || end == raw.c_str() || *end != '\0' ||
                    !std::isfinite(parsed) || parsed <= 0.0 ||
                    parsed > 10000.0) {
                    error("BPM MUST BE POSITIVE AND FINITE");
                    return true;
                }
                next[static_cast<std::size_t>(ui.tempoEditingRow)].bpm = parsed;
            } else {
                for (std::size_t j = 0; j < raw.size(); ++j)
                    if ((raw[j] < '0' || raw[j] > '9') &&
                        !(j == 0 && raw[j] == '-')) {
                        error("OFFSET MUST BE AN INTEGER NUMBER OF MS");
                        return true;
                    }
                try {
                    std::size_t used = 0;
                    const auto ms = std::stoll(raw, &used);
                    if (used != raw.size() || ms < -3600000 ||
                        ms > 86400000) {
                        error("OFFSET MS OUT OF RANGE");
                        return true;
                    }
                    next[static_cast<std::size_t>(ui.tempoEditingRow)].timeMs = ms;
                } catch (...) {
                    error("OFFSET MS OUT OF RANGE");
                    return true;
                }
            }
            std::string validation;
            if (!core::ValidTempoMap(next, validation)) {
                error("BPM MARKERS MUST HAVE INCREASING OFFSETS");
                return true;
            }
            if (!apply(next)) return true;
            ui.tempoEditingRow = -1;
            ui.tempoBuffer.clear();
            ui.tempoReplaceOnType = false;
        }
        return true;
    }
    if (input.mouseWheel != 0.0f &&
        detail::Hit(detail::kTempoTable, input.mouse)) {
        ui.tempoFirstVisible = std::clamp(ui.tempoFirstVisible +
            (input.mouseWheel > 0.0f ? -1 : 1),
            0, std::max(0, count - 5));
        return true;
    }
    if (!input.mouseClick) return false;
    const Vector2 mouse = input.mouse;
    if (detail::Hit(detail::kTempoPrev, mouse)) {
        ui.tempoFirstVisible = std::max(0, ui.tempoFirstVisible - 5);
        return true;
    }
    if (detail::Hit(detail::kTempoNext, mouse)) {
        ui.tempoFirstVisible = std::min(std::max(0, count - 5),
                                        ui.tempoFirstVisible + 5);
        return true;
    }
    if (detail::Hit(detail::kTempoAdd, mouse)) {
        if (count >= 256 || !draft.HasSongDuration()) {
            error("LOAD AUDIO BEFORE ADDING BPM MARKERS");
            return true;
        }
        const auto when = static_cast<std::int64_t>(std::llround(
            draft.AudioSecondsAtBeat(draft.SelectedBeat()) * 1000.0));
        auto next = current;
        const auto at = std::upper_bound(next.begin(), next.end(), when,
            [](std::int64_t time, const core::TempoMarker& marker) {
                return time < marker.timeMs;
            });
        if (at == next.begin() || (at-1)->timeMs == when ||
            when >= static_cast<std::int64_t>(
                std::llround(draft.SongDurationSeconds() * 1000.0))) {
            error("SELECT A FREE MOMENT AFTER THE FIRST BPM MARKER");
            return true;
        }
        const int index = static_cast<int>(at - next.begin());
        const double bpm = (at - 1)->bpm;
        next.insert(at, {when, bpm});
        if (apply(next))
            ui.tempoFirstVisible = std::clamp(index - 2, 0,
                                              std::max(0, count - 4));
        return true;
    }
    for (int row = 0; row < 5; ++row) {
        const int index = ui.tempoFirstVisible + row;
        if (index >= count) break;
        if (index > 0 && detail::Hit(detail::TempoDeleteCell(row), mouse)) {
            auto next = current;
            next.erase(next.begin() + index);
            if (apply(next))
                ui.tempoFirstVisible = std::min(ui.tempoFirstVisible,
                                                std::max(0, count - 6));
            return true;
        }
        const bool bpm = detail::Hit(detail::TempoBpmCell(row), mouse);
        const bool ms = detail::Hit(detail::TempoOffsetCell(row), mouse);
        if (!bpm && !ms) continue;
        ui.tempoEditingRow = index;
        ui.tempoEditingBpm = bpm;
        ui.tempoReplaceOnType = true;
        if (bpm) {
            ui.tempoBuffer = std::to_string(current[index].bpm);
            while (!ui.tempoBuffer.empty() &&
                   ui.tempoBuffer.back() == '0') ui.tempoBuffer.pop_back();
            if (!ui.tempoBuffer.empty() && ui.tempoBuffer.back() == '.')
                ui.tempoBuffer.pop_back();
        } else {
            ui.tempoBuffer = std::to_string(current[index].timeMs);
        }
        ui.feedback.clear();
        return true;
    }
    return false;
}

} // namespace locus::app
