#pragma once
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementText.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FontEngineInterface.h>
#include <algorithm>
#include <limits>

namespace locus::app {

// Align the actual glyph ink bounds, not the font's advance box or the
// ascender/descender box. The native font engine still renders the text.
// Cache by face (including DPI-scaled size) and glyph; no per-frame meshes.
struct RmlGlyphAlignment {
    Rml::FontFaceHandle face{};
    Rml::String glyph;
    float lineHeight=0;
};

inline void AlignRmlGlyph(Rml::Element& label,RmlGlyphAlignment& cache) {
    auto* text=dynamic_cast<Rml::ElementText*>(label.GetChild(0));
    if (!text) return;
    const auto face=label.GetFontFaceHandle();
    const auto line=label.GetLineHeight();
    auto* manager=label.GetRenderManager();
    if (!face || !manager || !(line>0)) return;
    const auto& glyph=text->GetText();
    if (cache.face==face && cache.glyph==glyph && cache.lineHeight==line) return;
    auto* engine=Rml::GetFontEngineInterface();
    if (!engine) return;
    const Rml::String language;
    const Rml::TextShapingContext shaping{language};
    Rml::TexturedMeshList meshes;
    const int advance=engine->GenerateString(*manager,face,0,glyph,{0,0},
        Rml::ColourbPremultiplied{255,255,255,255},1.0f,shaping,meshes);
    float left=std::numeric_limits<float>::max(),top=left;
    float right=-left,bottom=-left;
    for (const auto& entry:meshes) for (const auto& v:entry.mesh.vertices) {
        left=std::min(left,v.position.x); right=std::max(right,v.position.x);
        top=std::min(top,v.position.y); bottom=std::max(bottom,v.position.y);
    }
    if (right<=left || bottom<=top) return;
    const auto& metrics=engine->GetFontMetrics(face);
    // Text-align:center centers advance, and line-height centers the
    // font metric box. Compensate both using actual font geometry.
    const float x=(advance-left-right)*0.5f;
    const float y=-(metrics.ascent-metrics.descent+top+bottom)*0.5f;
    const auto px=[](float value){return std::to_string(value)+"px";};
    label.SetProperty("margin-left",px(x));
    label.SetProperty("margin-right",px(-x));
    label.SetProperty("margin-top",px(-line*0.5f+y));
    cache={face,glyph,line};
}
} // namespace locus::app
