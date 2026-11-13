#pragma once

#include "raylib.h"
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/SystemInterface.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace locus::app {

// Only RmlUi menu pixels go through this adapter. Scored gameplay keeps
// raylib's original renderer, camera, hitboxes and frame timing.
class RmlRaylibRenderer final : public Rml::RenderInterface {
public:
    Rml::CompiledGeometryHandle CompileGeometry(
        Rml::Span<const Rml::Vertex> vertices,
        Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry,
                        Rml::Vector2f translation,
                        Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions,
                                   const Rml::String& path) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> pixels,
                                       Rml::Vector2i dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    void ReleaseAllTextures();

private:
    struct Geometry {
        std::vector<Rml::Vertex> vertices;
        std::vector<int> indices;
    };
    std::unordered_map<Rml::TextureHandle, Texture2D> textures_;
    Rml::TextureHandle nextTexture_ = 1;
    bool clipEnabled_ = false;
    bool scissorActive_ = false;
    Rml::Rectanglei clipBounds_{};
    void ApplyClip();
};

class RmlRaylibSystem final : public Rml::SystemInterface {
public:
    double GetElapsedTime() override;
    void SetClipboardText(const Rml::String& text) override;
    void GetClipboardText(Rml::String& text) override;
    void SetMouseCursor(const Rml::String& cursor_name) override;
};

} // namespace locus::app
