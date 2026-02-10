#pragma once

enum {
    kGainId = 0,
    kFrequencyId,
    kFineId,
    kWaveformId,
    kAttackId,
    kReleaseId,
    kBypassId
};

enum WaveformType {
    kWaveSine = 0,
    kWaveSaw,
    kWaveSquare,
    kWaveTriangle,
    kNumWaveforms
};
