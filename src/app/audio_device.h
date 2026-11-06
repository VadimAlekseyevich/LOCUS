#pragma once

namespace locus::app {

// Sole owner of the raylib audio device. Construct before all music streams,
// destroy after they are unloaded. Transports borrow the device and must not
// initialize or close it (even when a decode or song load fails).
class AudioDevice {
public:
    AudioDevice() = default;
    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;
    ~AudioDevice();

    bool Initialize();
    void Shutdown() noexcept;
    bool Ready() const noexcept { return initialized_; }

private:
    bool initialized_ = false;
};

} // namespace locus::app
