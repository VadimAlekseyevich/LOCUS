#include "app/rml_raylib_renderer.h"

#include "rlgl.h"
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>
#include <algorithm>
#include <climits>
#include <filesystem>
#include <cstring>
#include <iostream>
#include <memory>

namespace locus::app {

Rml::CompiledGeometryHandle RmlRaylibRenderer::CompileGeometry(
    Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
    auto geometry = std::make_unique<Geometry>();
    geometry->vertices.assign(vertices.begin(), vertices.end());
    geometry->indices.assign(indices.begin(), indices.end());
    return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry.release());
}

void RmlRaylibRenderer::ReleaseGeometry(Rml::CompiledGeometryHandle handle) {
    delete reinterpret_cast<Geometry*>(handle);
}

void RmlRaylibRenderer::RenderGeometry(Rml::CompiledGeometryHandle handle,
                                      Rml::Vector2f translation,
                                      Rml::TextureHandle texture) {
    const auto* geometry = reinterpret_cast<const Geometry*>(handle);
    if (!geometry || geometry->indices.empty()) return;
    // RmlUi uses handle 0 for untextured geometry (backgrounds, borders
    // and solid button fills). rlgl cannot draw these with texture ID 0:
    // the shader requires raylib's 1x1 white texture for vertex colors.
    unsigned int id = rlGetTextureIdDefault();
    if (texture != 0) {
        const auto it = textures_.find(texture);
        if (it == textures_.end()) return; // Never render broken images white.
        id = it->second.id;
    }
    rlSetTexture(id);
    rlBegin(RL_TRIANGLES);
    for (const int index : geometry->indices) {
        if (index < 0 ||
            index >= static_cast<int>(geometry->vertices.size())) continue;
        const auto& vertex = geometry->vertices[static_cast<std::size_t>(index)];
        rlColor4ub(vertex.colour.red, vertex.colour.green,
                   vertex.colour.blue, vertex.colour.alpha);
        rlTexCoord2f(vertex.tex_coord.x, vertex.tex_coord.y);
        rlVertex2f(vertex.position.x + translation.x,
                   vertex.position.y + translation.y);
    }
    rlEnd();
    rlSetTexture(0);
}

Rml::TextureHandle RmlRaylibRenderer::LoadTexture(
    Rml::Vector2i& dimensions, const Rml::String& source) {
    // Use the same file interface as RML and RCSS. This supports consistent
    // paths and will also work with future packaged asset file interfaces.
    auto* files = Rml::GetFileInterface();
    const Rml::FileHandle handle = files ? files->Open(source) : Rml::FileHandle{};
    if (!handle) {
        std::cerr << "RmlUi image not found: " << source << '\n';
        return {};
    }
    const auto size = files->Length(handle);
    if (size == 0 || size > static_cast<std::size_t>(INT_MAX)) {
        files->Close(handle);
        std::cerr << "RmlUi image invalid size: " << source << '\n';
        return {};
    }
    std::vector<unsigned char> bytes(size);
    const bool complete = files->Read(bytes.data(), size, handle) == size;
    files->Close(handle);
    if (!complete) {
        std::cerr << "RmlUi image read failed: " << source << '\n';
        return {};
    }
    const std::string extension =
        std::filesystem::u8path(source).extension().u8string();
    ::Image image = LoadImageFromMemory(
        extension.empty() ? ".png" : extension.c_str(),
        bytes.data(), static_cast<int>(bytes.size()));
    if (!IsImageValid(image)) {
        std::cerr << "RmlUi image decode failed: " << source << '\n';
        return {};
    }
    dimensions = {image.width, image.height};
    // RmlUi's generated font atlases are premultiplied. Match them for
    // disk-loaded PNGs because the menu render pass uses premultiplied blend.
    ImageAlphaPremultiply(&image);
    const Texture2D uploaded = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!IsTextureValid(uploaded)) return {};
    const auto id = nextTexture_++;
    textures_[id] = uploaded;
    return id;
}

Rml::TextureHandle RmlRaylibRenderer::GenerateTexture(
    Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) {
    if (dimensions.x <= 0 || dimensions.y <= 0 ||
        source.size() != static_cast<std::size_t>(dimensions.x) *
                         static_cast<std::size_t>(dimensions.y) * 4) return {};
    // RmlUi supplies premultiplied RGBA; preserve the raw channel bytes.
    ::Image image{};
    image.data = const_cast<Rml::byte*>(source.data());
    image.width = dimensions.x;
    image.height = dimensions.y;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    const Texture2D texture = LoadTextureFromImage(image);
    if (!texture.id) return {};
    const auto id = nextTexture_++;
    textures_[id] = texture;
    return id;
}

void RmlRaylibRenderer::ReleaseTexture(Rml::TextureHandle handle) {
    const auto it = textures_.find(handle);
    if (it == textures_.end()) return;
    UnloadTexture(it->second);
    textures_.erase(it);
}

void RmlRaylibRenderer::ReleaseAllTextures() {
    for (const auto& entry : textures_) UnloadTexture(entry.second);
    textures_.clear();
}

void RmlRaylibRenderer::ApplyClip() {
    if (scissorActive_) {
        EndScissorMode();
        scissorActive_ = false;
    }
    if (!clipEnabled_) return;
    const int x = std::max(0, clipBounds_.Left());
    const int y = std::max(0, clipBounds_.Top());
    const int w = std::max(0, std::min(GetScreenWidth(), clipBounds_.Right()) - x);
    const int h = std::max(0, std::min(GetScreenHeight(), clipBounds_.Bottom()) - y);
    BeginScissorMode(x, y, w, h);
    scissorActive_ = true;
}

void RmlRaylibRenderer::EnableScissorRegion(bool enabled) {
    clipEnabled_ = enabled;
    ApplyClip();
}
void RmlRaylibRenderer::SetScissorRegion(Rml::Rectanglei region) {
    clipBounds_ = region;
    if (clipEnabled_) ApplyClip();
}

double RmlRaylibSystem::GetElapsedTime() {
    return ::GetTime();
}
void RmlRaylibSystem::SetClipboardText(const Rml::String& text) {
    ::SetClipboardText(text.c_str());
}
void RmlRaylibSystem::GetClipboardText(Rml::String& text) {
    const char* current = ::GetClipboardText();
    text = current ? current : "";
}
void RmlRaylibSystem::SetMouseCursor(const Rml::String& name) {
    if (name == "pointer") ::SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    else if (name == "text") ::SetMouseCursor(MOUSE_CURSOR_IBEAM);
    else ::SetMouseCursor(MOUSE_CURSOR_DEFAULT);
}

} // namespace locus::app
