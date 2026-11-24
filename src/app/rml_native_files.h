#pragma once
#include <RmlUi/Core/FileInterface.h>
#include <cstdio>
#include <filesystem>

namespace locus::app {
// RmlUi paths are UTF-8. Use native Unicode filesystem I/O for all RML,
// RCSS, fonts and images rather than the default narrow fopen on Windows.
class RmlNativeFiles final : public Rml::FileInterface {
public:
    Rml::FileHandle Open(const Rml::String& path) override {
        try {
            const auto native = std::filesystem::u8path(path);
#ifdef _WIN32
            return reinterpret_cast<Rml::FileHandle>(_wfopen(native.c_str(), L"rb"));
#else
            return reinterpret_cast<Rml::FileHandle>(std::fopen(native.c_str(), "rb"));
#endif
        } catch (...) {
            return {};
        }
    }
    void Close(Rml::FileHandle handle) override {
        std::fclose(reinterpret_cast<std::FILE*>(handle));
    }
    size_t Read(void* buffer, size_t size, Rml::FileHandle handle) override {
        return std::fread(buffer, 1, size, reinterpret_cast<std::FILE*>(handle));
    }
    bool Seek(Rml::FileHandle handle, long offset, int origin) override {
        return std::fseek(reinterpret_cast<std::FILE*>(handle), offset, origin) == 0;
    }
    size_t Tell(Rml::FileHandle handle) override {
        const auto pos = std::ftell(reinterpret_cast<std::FILE*>(handle));
        return pos < 0 ? 0 : static_cast<size_t>(pos);
    }
};
}
