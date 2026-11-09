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
    // PNG is generated with raylib and JPEG is an independent real 2x2
    // image fixture. ExportImageToMemory only exports PNG in raylib 6.
    const Image generated = GenImageColor(8, 8, Color{40, 100, 220, 255});
    int length = 0;
    unsigned char* encoded = ExportImageToMemory(generated, ".png", &length);
    const bool encodedOk = encoded && length > 24;
    check(encodedOk, "real PNG encoder is available");
    if (encodedOk) {
        const auto file = directory / "generated.png";
        { std::ofstream out(file, std::ios::binary);
          out.write(reinterpret_cast<const char*>(encoded), length); }
        Image decoded{};
        check(DecodeImageAssetFile(file, decoded, error) &&
              IsImageValid(decoded) && decoded.width == 8 && decoded.height == 8,
              "complete PNG decodes from disk without a graphics context");
        if (IsImageValid(decoded)) UnloadImage(decoded);
    }
    if (encoded) UnloadFileData(encoded);
    UnloadImage(generated);
    const unsigned char jpeg[] = {
        255,216,255,224,0,16,74,70,73,70,0,1,1,0,0,1,0,1,
        0,0,255,219,0,67,0,8,6,6,7,6,5,8,7,7,7,9,
        9,8,10,12,20,13,12,11,11,12,25,18,19,15,20,29,26,31,
        30,29,26,28,28,32,36,46,39,32,34,44,35,28,28,40,55,41,
        44,48,49,52,52,52,31,39,57,61,56,50,60,46,51,52,50,255,
        219,0,67,1,9,9,9,12,11,12,24,13,13,24,50,33,28,33,
        50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,
        50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,
        50,50,50,50,50,50,50,50,50,50,50,50,255,192,0,17,
        8,0,2,0,2,3,1,34,0,2,17,1,3,17,1,255,196,0,
        21,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,6,255,196,0,20,16,1,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,255,196,0,20,1,1,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,6,255,196,0,20,17,1,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,218,0,12,
        3,1,0,2,17,3,17,0,63,0,150,0,224,81,255,217
    };
    const auto jpgPath = directory / "real-photo.jpg";
    { std::ofstream out(jpgPath, std::ios::binary);
      out.write(reinterpret_cast<const char*>(jpeg), sizeof(jpeg)); }
    Image jpgDecoded{};
    check(DecodeImageAssetFile(jpgPath, jpgDecoded, error) &&
          IsImageValid(jpgDecoded) && jpgDecoded.width == 2 && jpgDecoded.height == 2,
          "real JPEG decoder is enabled in raylib and works without a GPU");
    if (IsImageValid(jpgDecoded)) UnloadImage(jpgDecoded);
    Image invalid{};
    check(!DecodeImageAssetFile(truncated, invalid, error),
          "truncated images cannot be decoded or imported");
    std::filesystem::remove_all(directory, ec);
    if (ok) std::cout << "IMAGE_ASSET_PASS crop=1 paths=1 signatures=1\n";
    return ok ? 0 : 1;
}
