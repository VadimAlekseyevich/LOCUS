#include "app/image_cover_geometry.h"
#include "app/image_asset_file.h"
#include "app/image_asset_decode.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

int main() {
    using namespace locus::app;
    bool ok = true;
    const auto check = [&](bool value, const char* what) {
        if (!value) std::cerr << "IMAGE_ASSET_FAIL " << what << '\n';
        ok &= value;
    };
    const auto landscape = ComputeImageCoverCrop(1920, 1080, 540, 540);
    const auto portrait = ComputeImageCoverCrop(1080, 1920, 540, 540);
    const auto cinema = ComputeImageCoverCrop(1920, 1080, 2560, 1080);
    check(landscape.x == 420 && landscape.y == 0 &&
          landscape.width == 1080 && landscape.height == 1080,
          "wide image crops both horizontal edges");
    check(portrait.x == 0 && portrait.y == 420 &&
          portrait.width == 1080 && portrait.height == 1080,
          "tall image crops vertical edges");
    check(std::abs(cinema.height - 810.0f) < 0.01f &&
          std::abs(cinema.y - 135.0f) < 0.01f,
          "fill widescreen without stretching image dimensions");
    check(ComputeImageCoverCrop(0, 1080, 540, 540).width == 0,
          "invalid dimensions safely yield an empty crop");
    check(SafeImageFilename("cover.PNG") && SafeImageFilename("cover.jpeg") &&
          !SafeImageFilename("../outside.png") &&
          !SafeImageFilename("..\\outside.jpg") &&
          !SafeImageFilename("not-image.mp3") &&
          !SafeImageFilename("x:bad.png"),
          "manifest names stay flat and image-only");
    const auto directory = std::filesystem::temp_directory_path() /
                           "locus-image-asset-headless-check";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    const auto png = directory / "header.png";
    // Real 2x2 PNG, not an incomplete 24-byte IHDR used as a fake fixture.
    const unsigned char bytes[] = {
        137,80,78,71,13,10,26,10,0,0,0,13,
        73,72,68,82,0,0,0,2,0,0,0,2,8,2,0,0,0,253,212,154,115,
        0,0,0,22,73,68,65,84,120,156,99,108,144,75,97,96,96,96,
        98,96,96,96,96,96,0,0,12,94,1,6,140,139,168,168,
        0,0,0,0,73,69,78,68,174,66,96,130
    };
    { std::ofstream out(png, std::ios::binary);
      out.write(reinterpret_cast<const char*>(bytes), sizeof bytes); }
    std::string error;
    check(ValidateImageAssetFile(png, error),
          "complete PNG dimensions, signature and end marker are validated");
    const auto truncated = directory / "truncated.png";
    { std::ofstream out(truncated, std::ios::binary);
      out.write(reinterpret_cast<const char*>(bytes), 24); }
    check(!ValidateImageAssetFile(truncated, error),
          "a plausible PNG header without image data cannot be imported");
    const auto wrong = directory / "header.jpg";
    std::filesystem::copy_file(png, wrong,
                               std::filesystem::copy_options::overwrite_existing, ec);
    check(!ValidateImageAssetFile(wrong, error),
          "extension spoofing is rejected before GPU decode");
    // Exercise the actual raylib 6 decoder without a graphics context.
    // Both codecs must be enabled in the linked library (JPEG is off by default).
    const Image generated = GenImageColor(8, 8, Color{40, 100, 220, 255});
    for (const char* extension : {".png", ".jpg"}) {
        int length = 0;
        unsigned char* encoded = ExportImageToMemory(extension, generated, &length);
        const bool encodedOk = encoded && length > 24;
        check(encodedOk, "PNG and JPEG encoders must both be enabled");
        if (encodedOk) {
            const auto file = directory / (std::string("generated") + extension);
            { std::ofstream out(file, std::ios::binary);
              out.write(reinterpret_cast<const char*>(encoded), length); }
            Image decoded{};
            check(DecodeImageAssetFile(file, decoded, error) &&
                  IsImageValid(decoded) && decoded.width == 8 && decoded.height == 8,
                  "both formats decode from real on-disk files without a GPU");
            if (IsImageValid(decoded)) UnloadImage(decoded);
        }
        if (encoded) UnloadFileData(encoded);
    }
    UnloadImage(generated);
    Image invalid{};
    check(!DecodeImageAssetFile(png, invalid, error),
          "signature-only fixture does not count as a decodable background");
    std::filesystem::remove_all(directory, ec);
    if (ok) std::cout << "IMAGE_ASSET_PASS crop=1 paths=1 signatures=1\n";
    return ok ? 0 : 1;
}
