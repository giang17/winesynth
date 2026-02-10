#pragma once

#include "pluginterfaces/base/funknown.h"

#define PluginCategory "Instrument|Synth"

namespace WineSynth {

// Unique IDs — generated specifically for WineSynth, must not collide with other plugins
static const Steinberg::FUID ProcessorUID  (0x2B3C4D5E, 0x6F7A8B9C, 0x0D1E2F3A, 0x4B5C6D7E);
static const Steinberg::FUID ControllerUID (0x8F9A0B1C, 0x2D3E4F5A, 0x6B7C8D9E, 0x0F1A2B3C);

} // namespace WineSynth
