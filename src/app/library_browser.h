#pragma once

#include "app/menu_ui.h"
#include "app/song_library.h"

#include <filesystem>
#include <string>

namespace locus::app {

// UI-level persistence actions; Windows native file dialogs stay in main.
// The selected difficulty is preserved by chart PATH across library rescans
// and sorting, rather than relying on positional indices that may change.
bool RefreshLibraryBrowser(SongLibrary& library, SongSelectUi& ui,
                           std::string& menuStatus);

bool ImportLibraryChartToSet(SongLibrary& library, SongSelectUi& ui,
                             const std::filesystem::path& sourceChart,
                             const std::filesystem::path& setFolder,
                             std::string& menuStatus);

// Import (or reimport) a chart and select the resulting difficulty.
// Failures never replace menu selection or a previously successful status.
bool ImportLibraryChart(SongLibrary& library, SongSelectUi& ui,
                        const std::filesystem::path& sourceChart,
                        std::string& menuStatus);

} // namespace locus::app
