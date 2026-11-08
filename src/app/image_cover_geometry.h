#pragma once

#include <algorithm>

namespace locus::app {

// Aspect-preserving "cover": fill the entire target; crop the source rather
// than stretch it, letterbox it, or distort player art. GPU-independent math
// is also reusable for image-based skin sprites / buttons later.
struct ImageSourceCrop {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};
inline ImageSourceCrop ComputeImageCoverCrop(float imageWidth,
                                             float imageHeight,
                                             float targetWidth,
                                             float targetHeight) noexcept {
    if (imageWidth <= 0.0f || imageHeight <= 0.0f ||
        targetWidth <= 0.0f || targetHeight <= 0.0f)
        return {};
    const float sourceAspect = imageWidth / imageHeight;
    const float targetAspect = targetWidth / targetHeight;
    if (sourceAspect > targetAspect) {
        const float width = imageHeight * targetAspect;
        return {(imageWidth - width) * 0.5f, 0.0f, width, imageHeight};
    }
    const float height = imageWidth / targetAspect;
    return {0.0f, (imageHeight - height) * 0.5f, imageWidth, height};
}
} // namespace locus::app
