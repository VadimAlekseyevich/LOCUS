#include "app/path_mesh.h"
#include "app/visual_style.h"
#include "core/route_view.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << "PATH_MESH_FAIL " << message << '\n';
    return condition;
}

bool Near(double a, double b, double eps = 0.0001) {
    return std::abs(a - b) <= eps;
}

bool Finite(const locus::app::PathRibbonRow& row) {
    return std::isfinite(row.center.x) && std::isfinite(row.center.y) &&
           std::isfinite(row.offset.x) && std::isfinite(row.offset.y) &&
           std::isfinite(row.opacity) && std::isfinite(row.tick);
}

bool CheckRibbon(const std::vector<locus::app::PathRibbonRow>& rows,
                 double start, double end, const locus::app::Skin& skin,
                 int ticksPerBeat, const char* message) {
    if (rows.size() < 2 || !Near(rows.front().tick, start) ||
        !Near(rows.back().tick, end) || !rows.front().startsRun)
        return Check(false, message);
    const double horizon = locus::app::ScoredPreviewTicks(
        skin.lookaheadMs, 120.0, ticksPerBeat);
    const double maxStep = 1.0 /
        std::max(1, locus::app::kScoredPathSlices / ticksPerBeat);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        const double distance = std::hypot(row.offset.x, row.offset.y);
        const double expected = locus::app::ScoredPathWidth(
            row.tick - start, horizon, skin.pathWidth, skin.fadePower) / 2.0;
        if (!Finite(row) ||
            row.tick < start - 1e-7 || row.tick > end + 1e-7 ||
            row.opacity < 0.0f || row.opacity > 1.0f ||
            distance > expected * 2.751 + 1e-5)
            return Check(false, message);
        if (i != 0) {
            const auto& previous = rows[i - 1];
            const double delta = row.tick - previous.tick;
            if (delta < -1e-8 || delta > maxStep + 1e-8)
                return Check(false, message);
            // A new strip starts only at an identical junction position:
            // it cannot jump ahead, split the previous stroke or teleport.
            if (row.startsRun &&
                (!Near(delta, 0.0, 1e-8) ||
                 !Near(row.center.x, previous.center.x) ||
                 !Near(row.center.y, previous.center.y)))
                return Check(false, message);
            // The actual GPU quads use these SAME two row pairs as their
            // shared endpoints; there is no independently rounded segment.
        }
    }
    return Check(true, message);
}

} // namespace

int main() {
    using namespace locus;
    bool ok = true;
    app::Skin skin;
    skin.lookaheadMs = 2000.0f; // 4 beats at 120 BPM for existing mesh fixtures.
    const app::RouteCanvas canvas{
        240.0f, 300.0f, 40.0f, {1.0, 1.0}
    };
    const core::Chart chart = core::MakeDemoChart();

    const app::Skin defaults;
    ok &= Check(defaults.lookaheadMs == 800.0f &&
                Near(app::ScoredPreviewTicks(defaults.lookaheadMs,
                                             chart.bpm, chart.ticksPerBeat), 1.6),
                "800 ms means 1.6 beats at 120 BPM");
    const auto defaultRibbon = app::BuildScoredPathRibbon(
        chart, 1.25, canvas, defaults);
    ok &= Check(!defaultRibbon.empty() &&
                Near(defaultRibbon.back().tick, 2.85),
                "800 ms clips at a fractional tick rather than a whole beat");
    core::Chart faster = chart;
    faster.bpm = 240.0;
    const auto fasterRibbon = app::BuildScoredPathRibbon(
        faster, 1.25, canvas, defaults);
    ok &= Check(!fasterRibbon.empty() &&
                Near(fasterRibbon.back().tick, 4.45),
                "same 800 ms horizon spans twice as many beats at double BPM");
    core::TimedRoute compiled;
    ok &= Check(compiled.Compile(chart, 0.0),
                "compile timed route for shared preview");
    const auto compiledFrame = app::MakeRoutePreview(compiled, 1.25, 800.0f);
    const auto preparedRibbon = app::BuildScoredPathRibbon(
        compiledFrame, canvas, defaults);
    const auto compatibilityRibbon = app::BuildScoredPathRibbon(
        compiled, 1.25, canvas, defaults);
    ok &= Check(preparedRibbon.size() == compatibilityRibbon.size() &&
                !preparedRibbon.empty() &&
                Near(preparedRibbon.front().center.x,
                     compatibilityRibbon.front().center.x) &&
                Near(preparedRibbon.back().tick,
                     compatibilityRibbon.back().tick),
                "preselected preview equals standalone ribbon rendering");
    const float near = app::ScoredPathOpacity(0.0, 1.6, defaults.fadePower);
    const float far = app::ScoredPathOpacity(1.55, 1.6, defaults.fadePower);
    ok &= Check(Near(app::ScoredTurnSize(near), 1.0) &&
                app::ScoredTurnSize(far) < app::ScoredTurnSize(near) &&
                app::ScoredTurnSize(far) >= 0.4f,
                "future turn indicators brighten and grow toward the player");
    const double playhead = 1.25;
    const auto ribbon = app::BuildScoredPathRibbon(
        chart, playhead, canvas, skin);
    ok &= CheckRibbon(ribbon, playhead, 5.25, skin, chart.ticksPerBeat,
                     "full four-beat preview has connected, finite, bounded rows");
    ok &= Check(ribbon.front().opacity > 0.99f &&
                Near(ribbon.back().opacity, 0.0f) &&
                Near(ribbon.front().center.x,
                     canvas.Position(core::RoutePositionAt(chart, playhead)).x) &&
                Near(ribbon.front().center.y,
                     canvas.Position(core::RoutePositionAt(chart, playhead)).y) &&
                Near(ribbon.back().center.x,
                     canvas.Position(core::RoutePositionAt(chart, 5.25)).x),
                "ribbon starts at moving marker and fades at four-beat horizon");

    int joinedAtNinety = 0;
    for (const auto& row : ribbon)
        if (Near(row.tick, 2.0)) {
            ++joinedAtNinety;
            const double ordinaryHalfWidth = app::ScoredPathWidth(
                row.tick - playhead, locus::app::ScoredPreviewTicks(skin.lookaheadMs, chart.bpm, chart.ticksPerBeat),
                skin.pathWidth, skin.fadePower) / 2.0;
            ok &= Check(!row.startsRun &&
                        Near(std::hypot(row.offset.x, row.offset.y) /
                             ordinaryHalfWidth, std::sqrt(2.0), 0.005),
                        "90-degree corner uses one shared mitered join row");
        }
    ok &= Check(joinedAtNinety == 1,
                "one physical corner has one shared row, not separate line ends");

    // A 135-degree corner is the sharpest mitered authorable turn.
    core::Chart sharp = chart;
    sharp.turns = {{2, core::Heading::UpLeft}};
    const auto sharpRibbon = app::BuildScoredPathRibbon(
        sharp, playhead, canvas, skin);
    int sharpJunctions = 0;
    for (const auto& row : sharpRibbon) {
        if (!Near(row.tick, 2.0)) continue;
        ++sharpJunctions;
        const double base = app::ScoredPathWidth(
            row.tick - playhead, locus::app::ScoredPreviewTicks(skin.lookaheadMs, chart.bpm, chart.ticksPerBeat),
            skin.pathWidth, skin.fadePower) / 2.0;
        ok &= Check(Near(std::hypot(row.offset.x, row.offset.y) / base,
                         1.0 / std::cos(135.0 * 3.141592653589793 /
                                         360.0), 0.008),
                    "135-degree corner is finite and uses bounded shared miter");
    }
    ok &= Check(sharpJunctions == 1,
                "sharp corner does not split or create multiple overlapping tips");

    core::Chart reverse = chart;
    reverse.turns = {{2, core::Heading::Left}, {4, core::Heading::Right}};
    const auto returnRibbon = app::BuildScoredPathRibbon(
        reverse, playhead, canvas, skin);
    ok &= CheckRibbon(returnRibbon, playhead, 5.25, skin,
                     reverse.ticksPerBeat,
                     "reversed path still covers exactly the future four beats");
    int reverseBreaks = 0;
    for (std::size_t i = 1; i < returnRibbon.size(); ++i) {
        const auto& row = returnRibbon[i];
        if (!row.startsRun) continue;
        ++reverseBreaks;
        const auto& before = returnRibbon[i - 1];
        ok &= Check(Near(row.tick, before.tick) &&
                    Near(row.center.x, before.center.x) &&
                    Near(row.center.y, before.center.y) &&
                    Near(row.offset.x, -before.offset.x, 0.0005) &&
                    Near(row.offset.y, -before.offset.y, 0.0005),
                    "180-degree turn splits only the normal, not path position/time");
    }
    ok &= Check(reverseBreaks == 2,
                "two half-turns create exactly two safe ribbon run boundaries");

    // The render layer must remain immune to camera translations: shifting
    // the virtual viewport cannot introduce a different corner normal.
    app::RouteCanvas shifted = canvas;
    shifted.left += 13.25f;
    shifted.top -= 7.5f;
    const auto shiftedRibbon = app::BuildScoredPathRibbon(
        chart, playhead, shifted, skin);
    ok &= Check(shiftedRibbon.size() == ribbon.size(),
                "camera offset does not change the number of future vertices");
    if (shiftedRibbon.size() == ribbon.size()) {
        for (std::size_t i = 0; i < ribbon.size(); ++i) {
            if (!Near(shiftedRibbon[i].center.x - ribbon[i].center.x, 13.25) ||
                !Near(shiftedRibbon[i].center.y - ribbon[i].center.y, -7.5) ||
                !Near(shiftedRibbon[i].offset.x, ribbon[i].offset.x) ||
                !Near(shiftedRibbon[i].offset.y, ribbon[i].offset.y)) {
                ok &= Check(false,
                    "moving fractional camera does not break joined normals");
                break;
            }
        }
    }

    // Dense chart (24 ticks per beat), with all types of change, including
    // overlapping return geometry. No duplicate vanishing micro-segments.
    core::Chart dense = chart;
    dense.ticksPerBeat = 24;
    dense.snapDivision = 24;
    dense.lengthBeats = 240;
    dense.turns = {{24, core::Heading::DownRight},
                   {48, core::Heading::UpLeft},
                   {72, core::Heading::DownRight},
                   {96, core::Heading::UpRight},
                   {120, core::Heading::DownLeft}};
    const auto denseRibbon = app::BuildScoredPathRibbon(
        dense, 22.375, canvas, skin);
    ok &= CheckRibbon(denseRibbon, 22.375, 118.375, skin, 24,
                     "24-tick snap and multiple turn types keep exact continuity");

    core::Chart shortChart = chart;
    shortChart.lengthBeats = 4;
    shortChart.turns.clear();
    const auto ending = app::BuildScoredPathRibbon(
        shortChart, 3.5, canvas, skin);
    ok &= CheckRibbon(ending, 3.5, 4.0, skin, 1,
                     "song-end trims the horizon without drawing past last beat");
    ok &= Check(app::BuildScoredPathRibbon(
                    shortChart, 4.0, canvas, skin).empty() &&
                app::BuildScoredPathRibbon(
                    shortChart, std::numeric_limits<double>::quiet_NaN(),
                    canvas, skin).empty(),
                "empty, invalid or completed playback has no ribbon");

    // REGRESSION: valid mesh geometry is not enough. Wrong GPU-facing
    // winding caused ALL trajectory triangles to be culled while the
    // separately drawn junction dots remained visible on Windows.
    // Raylib's quads use triangles (0,1,2) and (0,2,3). Screen Y is down:
    // both signed areas must be NEGATIVE for the visible front side.
    auto winding = [](Vector2 a, Vector2 b, Vector2 c) {
        return (b.x - a.x) * (c.y - a.y) -
               (b.y - a.y) * (c.x - a.x);
    };
    core::Chart straight = chart;
    straight.turns.clear();
    const core::Heading directions[] = {
        core::Heading::Up, core::Heading::UpRight,
        core::Heading::Right, core::Heading::DownRight,
        core::Heading::Down, core::Heading::DownLeft,
        core::Heading::Left, core::Heading::UpLeft
    };
    for (const auto heading : directions) {
        straight.initialHeading = heading;
        const auto rows = app::BuildScoredPathRibbon(
            straight, 1.25, canvas, skin);
        int visibleQuads = 0;
        for (std::size_t i = 1; i < rows.size(); ++i) {
            if (rows[i].startsRun || rows[i - 1].opacity <= 0.5f ||
                rows[i].opacity <= 0.5f) continue;
            const auto quad = app::MakePathRibbonQuad(
                rows[i - 1], rows[i], 1.0f);
            const float front1 = winding(quad.vertices[0],
                                          quad.vertices[1],
                                          quad.vertices[2]);
            const float front2 = winding(quad.vertices[0],
                                          quad.vertices[2],
                                          quad.vertices[3]);
            if (front1 < -1e-3f && front2 < -1e-3f) ++visibleQuads;
        }
        ok &= Check(visibleQuads > 20,
                    "quad faces the GPU's visible side in all eight headings");
    }
    // Winding is preserved for BOTH outline and foreground stroke widths.
    const auto rows = app::BuildScoredPathRibbon(straight, 2.25, canvas, skin);
    for (float scale : {1.0f, 1.8f}) {
        const auto quad = app::MakePathRibbonQuad(
            rows[4], rows[5], scale);
        ok &= Check(winding(quad.vertices[0], quad.vertices[1],
                             quad.vertices[2]) < -1e-3f &&
                    winding(quad.vertices[0], quad.vertices[2],
                             quad.vertices[3]) < -1e-3f,
                    "path outline and foreground both use front-facing winding");
    }


    // The scored renderer uses the exact timestamp instead of the rounded
    // authoring tick. It must share the same vertex at a 500 ms bend.
    core::Chart offGrid;
    offGrid.bpm = 130.0;
    offGrid.start = {0, 0};
    offGrid.firstBeat = 0;
    offGrid.lengthBeats = 16;
    offGrid.initialHeading = core::Heading::Right;
    offGrid.turns = {{1, core::Heading::Up, 500}};
    core::TimedRoute timed;
    app::Skin timedSkin;
    timedSkin.lookaheadMs = 800.0f;
    const bool compiled = timed.Compile(offGrid, 0.0);
    ok &= Check(compiled, "compile off-grid note for scored ribbon");
    if (compiled) {
        const auto timedRows = app::BuildScoredPathRibbon(
            timed, 0.45, canvas, timedSkin);
        const Vector2 target = canvas.Position(timed.PositionAt(0.5));
        int corners = 0;
        for (const auto& row : timedRows) {
            if (!Near(row.tick, 0.5, 1e-8)) continue;
            ++corners;
            ok &= Check(Near(row.center.x, target.x) &&
                        Near(row.center.y, target.y) && !row.startsRun,
                        "scored route uses one joined vertex at exact 500ms");
        }
        ok &= Check(corners == 1 && !timedRows.empty() &&
                    Near(timedRows.front().tick, 0.45) &&
                    Near(timedRows.back().tick, 1.25),
                    "time-based preview spans precisely 800ms");
    }

    if (!ok) return 1;
    std::cout << "PATH_MESH_PASS joined=90,135 reverse=180 dense=24 continuous=1\n";
    return 0;
}
