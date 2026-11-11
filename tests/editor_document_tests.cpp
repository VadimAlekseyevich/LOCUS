#include "app/editor_document.h"
#include "app/local_music.h"
#include "core/chart_editor.h"
#include "core/tempo_map.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace locus::app;

namespace {
bool Check(bool ok, const char* message) {
    if (!ok) std::cerr << "EDITOR_DOCUMENT_FAIL " << message << '\n';
    return ok;
}
} // namespace

int main() {
    namespace fs = std::filesystem;
    bool ok = true;
    locus::core::ChartEditor editor;
    editor.SetTitle("Existing unsaved draft");
    const int selected = editor.SelectedBeat();
    LocalMusic song;
    song.SetVolume(0.36f);
    song.SetSpeed(0.75, 0.0);
    std::wstring chartPath = L"original.locus.json";
    std::wstring audioPath = L"original.wav";
    std::string error;

    // Even if an attempted reload cannot read its input, the existing
    // document, unsaved edits and selection must remain intact.
    const bool loadedMissing = LoadDraftFromDisk(
        L"nonexistent-editor-document.locus.json",
        editor, song, chartPath, audioPath, error);
    ok &= Check(!loadedMissing && !error.empty() &&
                editor.Data().title == "Existing unsaved draft" &&
                editor.Dirty() && editor.SelectedBeat() == selected &&
                chartPath == L"original.locus.json" &&
                audioPath == L"original.wav" && !song.Ready() &&
                song.Volume() > 0.35f && song.Volume() < 0.37f,
                "failed load does not replace the working draft or transport");

    const fs::path root = fs::temp_directory_path() /
        ("locus-document-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) return 1;
    const fs::path malformed = root / "malformed.locus.json";
    {
        std::ofstream file(malformed, std::ios::binary);
        file << "{not-valid-json";
    }
    error.clear();
    const bool loadedMalformed = LoadDraftFromDisk(
        malformed.wstring(), editor, song, chartPath, audioPath, error);
    ok &= Check(!loadedMalformed && !error.empty() &&
                editor.Data().title == "Existing unsaved draft" &&
                editor.Dirty() && chartPath == L"original.locus.json",
                "invalid document cannot discard existing unsaved edits");

    error.clear();
    const bool saved = SaveDraftToDisk(editor, song, chartPath,
                                       audioPath, false, error);
    ok &= Check(!saved && !error.empty() && editor.Dirty() &&
                chartPath == L"original.locus.json" &&
                editor.Data().title == "Existing unsaved draft",
                "unattached audio rejects saving without modifying the draft");

    // New difficulties must inherit all tempo markers before any notes are
    // authored. A failure must not discard the previously edited document.
    const std::vector<locus::core::TempoMarker> tempo{
        {125, 150.0}, {1500, 225.0}, {2800, 90.0}};
    locus::core::ChartEditor newDifficulty;
    const bool configured = PrepareNewDifficultyDraft(
        newDifficulty, 12.0, "song.wav", tempo, error);
    const auto actual = locus::core::EffectiveTempoMap(
        newDifficulty.Data().route, newDifficulty.Data().offsetSeconds);
    ok &= Check(configured && actual == tempo &&
                newDifficulty.HasSongDuration() &&
                newDifficulty.Data().audioPath == "song.wav" &&
                newDifficulty.Data().title == "New Difficulty" &&
                newDifficulty.Dirty() && !newDifficulty.CanUndo(),
                "new difficulty inherits every Song Set tempo marker");
    // Undo must affect only real mapping edits, not the initial shared BPM
    // metadata. Returning to an unsaved baseline must remain dirty.
    newDifficulty.SetOutgoingHeading(locus::core::Heading::Up);
    ok &= Check(newDifficulty.CanUndo() && newDifficulty.Undo() &&
                newDifficulty.Dirty() && !newDifficulty.CanUndo() &&
                locus::core::EffectiveTempoMap(
                    newDifficulty.Data().route,
                    newDifficulty.Data().offsetSeconds) == tempo,
                "new difficulty Undo cannot discard inherited Song Set timing");
    const auto originalTempo = actual;
    const bool rejected = PrepareNewDifficultyDraft(
        newDifficulty, 12.0, "../escape.wav", tempo, error);
    ok &= Check(!rejected && !error.empty() &&
                locus::core::EffectiveTempoMap(
                    newDifficulty.Data().route,
                    newDifficulty.Data().offsetSeconds) == originalTempo,
                "invalid new difficulty creation preserves active document");

    fs::remove_all(root, ec);
    if (!ok) return 1;
    std::cout << "EDITOR_DOCUMENT_PASS failed-load=1 failed-save=1\n";
    return 0;
}
