#include "app/audio_device.h"

#include "raylib.h"

namespace locus::app {

AudioDevice::~AudioDevice() {
    Shutdown();
}

bool AudioDevice::Initialize() {
    if (initialized_) return true;
    // Never close a device belonging to somebody else. LOCUS creates one
    // owner in main before constructing streams and initializes it once.
    if (IsAudioDeviceReady()) return false;
    InitAudioDevice();
    initialized_ = IsAudioDeviceReady();
    return initialized_;
}

void AudioDevice::Shutdown() noexcept {
    if (!initialized_) return;
    CloseAudioDevice();
    initialized_ = false;
}

} // namespace locus::app
