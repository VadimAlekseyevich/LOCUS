#include "app/image_cover_geometry.h"
#include "app/image_asset_file.h"

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
    const unsigned char bytes[] = {
        137,80,78,71,13,10,26,10,0,0,0,13,
        'I','H','D','R',0,0,4,0,0,0,2,0
    };
    { std::ofstream out(png, std::ios::binary);
      out.write(reinterpret_cast<const char*>(bytes), sizeof bytes); }
    std::string error;
    check(ValidateImageAssetFile(png, error),
          "PNG dimensions and signature are independently validated");
    const auto wrong = directory / "header.jpg";
    std::filesystem::copy_file(png, wrong,
                               std::filesystem::copy_options::overwrite_existing, ec);
    check(!ValidateImageAssetFile(wrong, error),
          "extension spoofing is rejected before GPU decode");
    std::filesystem::remove_all(directory, ec);
    if (ok) std::cout << "IMAGE_ASSET_PASS crop=1 paths=1 signatures=1\n";
    return ok ? 0 : 1;
}
