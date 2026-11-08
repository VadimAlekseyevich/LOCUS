#pragma once
namespace locus::app {
// These checks run without creating a raylib window or audio device.
bool CheckLoopState();
bool CheckScreenTransitions();
bool CheckSessionFlow();
} // namespace locus::app
