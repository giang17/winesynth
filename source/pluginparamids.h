#pragma once

enum {
    kGainId = 0,
    kCutoffId,
    kFineId,
    kResonanceId,
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
