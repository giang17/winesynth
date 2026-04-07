#pragma once

enum {
    kGainId = 0,
    kCutoffId,
    kFineId,
    kResonanceId,
    kWaveformId,
    kAttackId,
    kReleaseId,
    kBypassId,
    kKeyboardNoteId,   // GUI keyboard note (0=off, 1-12=note C4-B4)
    kKeyboardTag = 100
};

enum WaveformType {
    kWaveSine = 0,
    kWaveSaw,
    kWaveSquare,
    kWaveTriangle,
    kNumWaveforms
};
