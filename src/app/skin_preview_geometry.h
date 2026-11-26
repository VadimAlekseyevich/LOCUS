#pragma once

#include "app/skin.h"
#include "app/viewport.h"
#include "core/timed_route.h"

#include <algorithm>

namespace locus::app {

// A thumbnail must scale route distances and all world-space dimensions
// together. Do not modify the saved skin or its dimensionless multipliers.
inline float SkinPreviewWorldScale(Rectangle field) noexcept {
    return std::min(field.width,field.height)/kVirtualHeight;
}

inline Skin SkinForPreview(const Skin& skin,float scale) {
    Skin result=skin;
    result.pathWidth*=scale;
    result.junctionRadius*=scale;
    result.playerRadius*=scale;
    return result;
}

// Readable moderate-density demonstration, not a stress-test chart.
// Absolute timestamps deliberately include a diagonal and a straight note.
inline const core::TimedRoute& SkinPreviewRoute() {
    static const core::TimedRoute route=[] {
        auto chart=core::MakeDemoChart();
        chart.turns={{1,core::Heading::UpRight,280},
                     {2,core::Heading::DownRight,520},
                     {3,core::Heading::Right,740},
                     {4,core::Heading::Right,980}};
        core::TimedRoute sample;
        sample.Compile(chart,0.0);
        return sample;
    }();
    return route;
}

} // namespace locus::app
