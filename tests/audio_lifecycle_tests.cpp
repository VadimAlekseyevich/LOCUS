#include "app/audio_device.h"
#include "app/demo_transport.h"
#include "core/route.h"

#include "raylib.h"

#include <iostream>

int main() {
    // CTest deliberately has no InitAudioDevice(): the demo transport is
    // not permitted to initialize, close, or claim a shared audio device.
    if (IsAudioDeviceReady()) {
        std::cerr << "AUDIO_LIFECYCLE_FAIL unexpected preinitialized device\n";
        return 1;
    }
    locus::app::AudioDevice device;
    locus::app::DemoTransport demo;
    if (device.Ready() || demo.Ready() ||
        demo.Initialize(locus::core::MakeDemoChart()) ||
        demo.Ready() || device.Ready() || IsAudioDeviceReady()) {
        std::cerr << "AUDIO_LIFECYCLE_FAIL transport initialized an absent device\n";
        return 1;
    }
    demo.Shutdown();
    demo.Shutdown();
    device.Shutdown();
    device.Shutdown();
    if (IsAudioDeviceReady()) {
        std::cerr << "AUDIO_LIFECYCLE_FAIL invalid shutdown closed someone else's device\n";
        return 1;
    }
    std::cout << "AUDIO_LIFECYCLE_PASS borrowed-transport=1 shutdown-idempotent=1\n";
    return 0;
}
