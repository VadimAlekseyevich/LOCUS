#pragma once

#include "app/atomic_text_file.h"
#include "app/chart_file.h"
#include "app/difficulty_identity.h"
#include "app/song_set_storage.h"
#include "app/song_timing.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <utility>

namespace locus::app::detail {
namespace fs = std::filesystem;

// Identity is scoped to the managed Song Set, never to its display name.
// Read disk under the writer locks, not the potentially stale UI index.
struct InstalledDifficulty {
    fs::path file;
    core::LoadedChart chart;
};

inline bool ReadInstalledDifficulties(
    const fs::path& folder, const fs::path& audio,
    std::vector<InstalledDifficulty>& result, std::string& error) {
    std::error_code ec;
    std::set<std::string> ids;
    for (fs::directory_iterator it(folder, ec), end; !ec && it != end;
         it.increment(ec)) {
        const auto path = it->path();
        if (path.extension() != L".json" ||
            path.filename() == L"set.locus.json") continue;
        if (it->is_symlink(ec) || !it->is_regular_file(ec) || ec) {
            error = "Existing difficulty is not a regular chart file.";
            return false;
        }
        InstalledDifficulty entry{path, {}};
        std::wstring resolved;
        if (!OpenChartWithAudioPath(path.wstring(), entry.chart, resolved, error))
            return false;
        if (!fs::equivalent(fs::path(resolved), audio, ec) || ec) {
            error = "Existing difficulty does not use the Song Set audio.";
            return false;
        }
        if (!entry.chart.difficultyId.empty() &&
            !ids.insert(entry.chart.difficultyId).second) {
            error = "Existing Song Set contains duplicate difficulty IDs.";
            return false;
        }
        result.push_back(std::move(entry));
    }
    if (ec) { error = "Cannot inspect installed difficulties: " + ec.message(); return false; }
    return true;
}

// A batch is fully prepared before any visible file is replaced. The same
// helper handles standalone imports and complete packages. Preserve the
// installed manifest and shared audio; incoming notes keep absolute times.
struct DifficultyImportWrite {
    fs::path destination;
    std::string before;
    std::string after;
    bool replace = false;
};

inline bool EncodeImportedDifficulty(core::LoadedChart chart,
    const SongSetManifest& metadata, std::string& encoded, std::string& error) {
    chart.audioPath = metadata.audioFile;
    if (!ApplySongTiming(chart, metadata.tempoMap, error) ||
        !core::SerializeChartJson(chart, encoded, error)) return false;
    if (encoded.size() > kMaxChartFileBytes) {
        error = "Imported chart exceeds the supported file size.";
        return false;
    }
    return true;
}

inline bool PrepareDifficultyImport(
    const fs::path& folder, const SongSetManifest& metadata,
    const std::vector<InstalledDifficulty>& existing,
    const fs::path& preferred, core::LoadedChart chart, bool repairLegacy,
    std::vector<DifficultyImportWrite>& writes, fs::path& destination,
    std::string& error) {
    std::error_code ec;
    const InstalledDifficulty* match = nullptr;
    if (!chart.difficultyId.empty()) {
        for (const auto& entry : existing)
            if (entry.chart.difficultyId == chart.difficultyId) {
                match = &entry;
                break;
            }
    }
    // Only an ID-less draft imported from its original path, or an ID-less
    // installed chart at the exact package filename, may use filename fallback.
    if (!match && (chart.difficultyId.empty() || repairLegacy)) {
        for (const auto& entry : existing) {
            if (entry.file != preferred) {
                const bool same = fs::equivalent(entry.file, preferred, ec);
                ec.clear();
                if (!same) continue;
            }
            if (chart.difficultyId.empty() ||
                (entry.chart.difficultyId.empty() && entry.chart.title == chart.title)) {
                match = &entry;
                break;
            }
        }
    }
    if (chart.difficultyId.empty())
        chart.difficultyId = match && !match->chart.difficultyId.empty()
            ? match->chart.difficultyId : NewDifficultyId();
    for (const auto& entry : existing) {
        if (&entry != match && entry.chart.difficultyId == chart.difficultyId) {
            error = "Imported difficulty identity is already occupied.";
            return false;
        }
    }
    destination = match ? match->file : preferred;
    const auto reserved = [&](const fs::path& path) {
        for (const auto& write : writes) {
#ifdef _WIN32
            const auto a = write.destination.filename().wstring();
            const auto b = path.filename().wstring();
            if (CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL)
                return true;
#else
            if (write.destination == path) return true;
#endif
        }
        return false;
    };
    if (match && reserved(destination)) {
        error = "Multiple incoming difficulties target the same installed file.";
        return false;
    }
    if (!match) {
        for (int suffix = 0; ; ++suffix) {
            const bool occupied = fs::exists(destination, ec);
            if (ec) { error = "Cannot inspect import destination."; return false; }
            if (!occupied && !reserved(destination)) break;
            if (suffix >= 10000) { error = "Cannot allocate difficulty filename."; return false; }
            destination = folder / (chart.difficultyId +
                (suffix == 0 ? "" : "-" + std::to_string(suffix)) + ".locus.json");
        }
    }
    DifficultyImportWrite write;
    write.destination = destination;
    write.replace = match != nullptr;
    if (!EncodeImportedDifficulty(chart, metadata, write.after, error)) return false;
    if (match) {
        std::string comparable;
        if (!EncodeImportedDifficulty(match->chart, metadata, comparable, error)) return false;
        // Formatting, file layout and stale embedded BPM are not edits.
        if (comparable == write.after) return true;
        std::ifstream file(destination, std::ios::binary);
        if (!file) { error = "Cannot read installed difficulty for recovery."; return false; }
        write.before.assign(std::istreambuf_iterator<char>(file),
                            std::istreambuf_iterator<char>());
        if (file.bad() || write.before.empty()) {
            error = "Cannot read installed difficulty for recovery.";
            return false;
        }
    }
    writes.push_back(std::move(write));
    return true;
}

inline bool PublishDifficultyImports(
    const std::vector<DifficultyImportWrite>& writes, std::string& error) {
    for (const auto& write : writes)
        if (write.replace && !BackUpChartBeforeReplace(write.destination, error))
            return false;

    std::size_t committed = 0;
    for (; committed < writes.size(); ++committed) {
        const auto& write = writes[committed];
        const auto status = AtomicWriteTextFile(write.destination, write.after,
            write.replace ? AtomicTextWriteMode::Replace : AtomicTextWriteMode::CreateNew,
            error);
        if (status == AtomicTextWriteStatus::Published) continue;
        if (error.empty()) error = "Import destination already exists.";
        // Roll back only changes published by this batch. Durable history
        // backups remain available even if an OS failure prevents rollback.
        while (committed > 0) {
            const auto& previous = writes[--committed];
            std::string restoreError;
            if (previous.replace) {
                if (!AtomicReplaceTextFile(previous.destination, previous.before, restoreError))
                    error += " (rollback failed: " + restoreError + ")";
            } else {
                std::error_code ec;
                fs::remove(previous.destination, ec);
                if (ec) error += " (rollback failed: " + ec.message() + ")";
            }
        }
        return false;
    }
    error.clear();
    return true;
}

} // namespace locus::app::detail
