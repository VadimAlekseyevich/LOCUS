#pragma once

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/RenderInterface.h>
#include <array>
#include <filesystem>
#include <string>

namespace locus::tests {

struct NullRenderer final : Rml::RenderInterface {
    std::size_t texturedDraws = 0;
    Rml::TextureHandle nextTexture = 1;
    Rml::CompiledGeometryHandle CompileGeometry(
        Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override {
        return 1;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f,
                        Rml::TextureHandle texture) override {
        if (texture != 0) ++texturedDraws;
    }
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override {
        return 0;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,
                                        Rml::Vector2i) override {
        return nextTexture++;
    }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};


inline bool TestFontAvailable() {
#ifdef _WIN32
    const std::array<const char*, 2> fonts = {
        "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"
    };
#else
    const std::array<const char*, 2> fonts = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"
    };
#endif
    for (const char* path : fonts) {
        if (!std::filesystem::exists(path) ||
            !Rml::LoadFontFace(path, "Locus",
                Rml::Style::FontStyle::Normal,
                Rml::Style::FontWeight::Normal))
            continue;
        // The production adapter also registers the bold face. HOME is
        // made entirely of heavyweight button labels, unlike PLAY which
        // includes normal-weight body text. A regular-only test font setup
        // would fail to render HOME despite a working production renderer.
        const std::filesystem::path regular(path);
        const std::filesystem::path bold = regular.parent_path() /
            (regular.filename() == "segoeui.ttf" ? "segoeuib.ttf" :
             regular.filename() == "arial.ttf" ? "arialbd.ttf" :
             regular.filename() == "DejaVuSans.ttf" ? "DejaVuSans-Bold.ttf" :
             "LiberationSans-Bold.ttf");
        if (std::filesystem::exists(bold))
            Rml::LoadFontFace(bold.u8string(), "Locus",
                Rml::Style::FontStyle::Normal,
                Rml::Style::FontWeight::Bold);
#ifdef _WIN32
        const char* symbols="C:/Windows/Fonts/seguisym.ttf";
#else
        const char* symbols="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#endif
        if (!std::filesystem::exists(symbols)) symbols=path;
        Rml::LoadFontFace(symbols,"LocusIcons",Rml::Style::FontStyle::Normal,
                          Rml::Style::FontWeight::Normal,true);
        return true;
    }
    return false;
}

} // namespace locus::tests
