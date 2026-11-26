#pragma once
#include "app/run_result_model.h"
#include <string>
#include <cstddef>
#include <cstdint>
#include <algorithm>

namespace locus::app {
enum class ProfileLoadState { Loading, Ready, Failed };
struct PlayerProfileView {
    ProfileLoadState state=ProfileLoadState::Loading;
    PlayerProfileSnapshot snapshot;
    std::string label="LOADING LOCAL PROFILE";
    std::uint64_t revision=0;
    std::size_t page=0;
    static constexpr std::size_t kRows=8;
    std::size_t PageCount() const noexcept {
        return std::max<std::size_t>(1,(snapshot.bestScores.size()+kRows-1)/kRows);
    }
    void StepPage(int delta) noexcept {
        if (state!=ProfileLoadState::Ready) return;
        if (delta<0 && page>0) --page;
        else if (delta>0 && page+1<PageCount()) ++page;
    }
};
} // namespace locus::app
