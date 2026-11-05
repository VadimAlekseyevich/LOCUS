#include "core/chart_editor.h"

#include <algorithm>
#include <cmath>

namespace locus::core {

ChartEditor::ChartEditor() {
    NewDraft();
}

void ChartEditor::NewDraft() {
    chart_ = LoadedChart{};
    chart_.title = "Untitled Chart";
    chart_.audioPath = "song.mp3";
    chart_.route = Chart{};
    chart_.route.bpm = 120.0;
    chart_.route.lengthBeats = 32;
    chart_.authoredLengthBeats = 32;
    chart_.route.start = Cell{1, 1};
    chart_.route.initialHeading = Heading::Right;
    chart_.route.turns.clear();
    chart_.offsetSeconds = 0.0;
    selectedBeat_ = 0;
    dirty_ = false;
}

void ChartEditor::Load(const LoadedChart& source) {
    chart_ = source;
    selectedBeat_ = 0;
    dirty_ = false;
}

double ChartEditor::AudioSecondsAtBeat(int beat) const noexcept {
    return chart_.offsetSeconds +
           static_cast<double>(beat) * 60.0 / chart_.route.bpm;
}

bool ChartEditor::SetAudioPath(const std::string& relativePath) {
    if (!ValidChartAudioPath(relativePath)) return false;
    if (chart_.audioPath == relativePath) return true;
    chart_.audioPath = relativePath;
    MarkDirty();
    return true;
}

bool ChartEditor::SelectBeat(int beat) noexcept {
    if (beat < 0 || beat >= chart_.route.lengthBeats || selectedBeat_ == beat) return false;
    selectedBeat_ = beat;
    return true;
}

bool ChartEditor::StepSelection(int deltaBeats) noexcept {
    // Avoid out-of-range selections at either edge of a short timeline.
    const long long next = std::clamp(
        static_cast<long long>(selectedBeat_) + deltaBeats,
        0LL, static_cast<long long>(chart_.route.lengthBeats - 1));
    return SelectBeat(static_cast<int>(next));
}

bool ChartEditor::SetBpm(double bpm) noexcept {
    // Chart JSON already accepts every finite positive BPM. The editor must
    // not reject legitimate fast charts (e.g. 400 BPM) at an arbitrary 300.
    if (!std::isfinite(bpm) || bpm <= 0.0) return false;
    if (bpm != chart_.route.bpm) {
        chart_.route.bpm = bpm;
        MarkDirty();
    }
    return true;
}

bool ChartEditor::SetOffsetMs(double milliseconds) noexcept {
    if (!std::isfinite(milliseconds) || std::abs(milliseconds) > 3600000.0)
        return false;
    if (milliseconds != chart_.offsetSeconds * 1000.0) {
        chart_.offsetSeconds = milliseconds / 1000.0;
        MarkDirty();
    }
    return true;
}

bool ChartEditor::SetLength(int beats) noexcept {
    if (beats < 1 || beats > 65536) return false;
    if (!chart_.route.turns.empty() && chart_.route.turns.back().beat >= beats)
        return false;
    if (beats != chart_.route.lengthBeats) {
        chart_.route.lengthBeats = beats;
        chart_.authoredLengthBeats = beats;
        selectedBeat_ = std::min(selectedBeat_, beats - 1);
        MarkDirty();
    }
    return true;
}

bool ChartEditor::SetTitle(const std::string& title) {
    if (title.empty() || title.size() > 100 ||
        title.find_first_not_of(" \t\n\r") == std::string::npos) return false;
    if (title != chart_.title) {
        chart_.title = title;
        MarkDirty();
    }
    return true;
}

bool ChartEditor::ChangeBpm(int delta) noexcept {
    const double target = chart_.route.bpm + delta;
    return target != chart_.route.bpm && SetBpm(target);
}

bool ChartEditor::ChangeOffsetMs(int deltaMs) noexcept {
    constexpr long long kLimitMs = 3600000; // one hour; no overflow while editing
    const double before = chart_.offsetSeconds * 1000.0;
    if (!std::isfinite(before) || std::abs(before) > static_cast<double>(kLimitMs))
        return false;
    const long long current = std::llround(before);
    const long long target = current + static_cast<long long>(deltaMs);
    if (target < -kLimitMs || target > kLimitMs || target == current) return false;
    chart_.offsetSeconds = static_cast<double>(target) / 1000.0;
    MarkDirty();
    return true;
}

bool ChartEditor::SetStart(Cell cell) noexcept {
    if (cell.x < 0 || cell.x >= 4 || cell.y < 0 || cell.y >= 4 ||
        (cell.x == chart_.route.start.x && cell.y == chart_.route.start.y)) return false;
    chart_.route.start = cell;
    MarkDirty();
    return true;
}

void ChartEditor::RotateHeading(int quarterTurns) noexcept {
    const int delta = (quarterTurns % 4 + 4) % 4;
    if (delta == 0) return;
    chart_.route.initialHeading = static_cast<Heading>(
        (static_cast<int>(chart_.route.initialHeading) + delta) % 4);
    MarkDirty();
}

bool ChartEditor::ChangeLength(int deltaBeats) noexcept {
    const int current = chart_.route.lengthBeats;
    const int next = current + deltaBeats;
    if (next < 1 || next > 65536 || next == current) return false;
    if (!chart_.route.turns.empty() && chart_.route.turns.back().beat >= next) return false;
    chart_.route.lengthBeats = next;
    chart_.authoredLengthBeats = next;
    selectedBeat_ = std::min(selectedBeat_, next - 1);
    MarkDirty();
    return true;
}

const TurnEvent* ChartEditor::SelectedTurn() const noexcept {
    const auto& events = chart_.route.turns;
    const auto found = std::lower_bound(
        events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    return (found != events.end() && found->beat == selectedBeat_) ? &*found : nullptr;
}

void ChartEditor::PutTurn(Turn turn) {
    auto& events = chart_.route.turns;
    const auto found = std::lower_bound(
        events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    if (found != events.end() && found->beat == selectedBeat_) {
        if (found->direction == turn) return;
        found->direction = turn;
    } else {
        events.insert(found, TurnEvent{selectedBeat_, turn});
    }
    MarkDirty();
}

bool ChartEditor::RemoveTurn() noexcept {
    auto& events = chart_.route.turns;
    const auto found = std::lower_bound(
        events.begin(), events.end(), selectedBeat_,
        [](const TurnEvent& event, int beat) { return event.beat < beat; });
    if (found == events.end() || found->beat != selectedBeat_) return false;
    events.erase(found);
    MarkDirty();
    return true;
}

bool ChartEditor::MoveTurn(int beats) noexcept {
    if (beats == 0 || !SelectedTurn()) return false;
    const int destination = selectedBeat_ + beats;
    if (destination < 0 || destination >= chart_.route.lengthBeats) return false;
    auto& events = chart_.route.turns;
    if (std::any_of(events.begin(), events.end(),
                    [destination](const TurnEvent& event) {
                        return event.beat == destination;
                    })) return false;
    const Turn direction = SelectedTurn()->direction;
    RemoveTurn();
    selectedBeat_ = destination;
    PutTurn(direction);
    return true;
}

} // namespace locus::core
