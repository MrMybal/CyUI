#pragma once
#include "cyui/modules/Plots.h"

namespace cyui {
struct Peak {
    float minimum = 0, maximum = 0;
};
// Prepared envelope covers [start,end]; one view per channel. No decoder or FFT runs in a widget.
struct WaveformData {
    Range coverage;
    std::vector<ArrayView<Peak>> channels;
};
struct WaveformState {
    Range view{0, 1}, selection{0, 0};
    bool selecting = false, panning = false;
};
struct WaveformResult {
    bool seek = false, selectionChanged = false;
    double time = 0;
};
CYUI_API WaveformResult waveform(const char *id, WaveformState &, const WaveformData &, double playhead, ImVec2 size);
CYUI_API std::vector<Peak> makePeaks(ArrayView<float> samples, std::size_t columns);
CYUI_API PlotResult spectrum(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size);
CYUI_API PlotResult oscilloscope(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size);
CYUI_API PlotResult stereoScope(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size);
} // namespace cyui
