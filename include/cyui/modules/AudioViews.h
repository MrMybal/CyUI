#pragma once
#include "cyui/modules/Plots.h"
#include <limits>

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
    Range view{0, 1}, selection{0, 0}; // selection is kept ordered (min <= max)
    bool selecting = false, panning = false;
    double anchor = 0; // where the selection drag started
};
struct WaveformResult {
    bool seek = false, selectionChanged = false;
    double time = 0;
};
CYUI_API WaveformResult waveform(const char *id, WaveformState &, const WaveformData &, double playhead, ImVec2 size);
CYUI_API std::vector<Peak> makePeaks(ArrayView<float> samples, std::size_t columns);
// Logarithmic X; an invalid or non-positive X range resets to 20..20000.
CYUI_API PlotResult spectrum(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size);
CYUI_API PlotResult spectrum(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size, const PlotOptions &);
// Linear axes.
CYUI_API PlotResult oscilloscope(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size);
CYUI_API PlotResult oscilloscope(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size, const PlotOptions &);
// Square X/Y scope: both axes share one range symmetric around 0 (X sets the extent; zoom scales it).
CYUI_API PlotResult stereoScope(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size);
CYUI_API PlotResult stereoScope(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size, const PlotOptions &);

// Frame-major matrix: values[frame * bins + bin], increasing time/frequency.
// Missing cells are NaN. Bin spacing may be linear or logarithmic independently of the view.
struct SpectrogramData {
    ArrayView<float> values;
    std::size_t frames = 0, bins = 0;
    Range time{0, 1}, frequency{20, 20000};
    bool logarithmicBins = false;
};
struct SpectrogramOptions {
    Range intensity{-100, 0}; // dB, or another host-defined unit
    const char *unit = "dB";
    bool navigation = true, axes = true;
    std::size_t maxCells = 65536; // screen-space sampling budget, capped at 262144
};
struct SpectrogramResult {
    bool valid = false, hovered = false, clicked = false, viewChanged = false;
    std::size_t frame = 0, bin = 0;
    Point mouse; // time, Hz
    float intensity = std::numeric_limits<float>::quiet_NaN();
};
CYUI_API SpectrogramResult spectrogram(const char *id, PlotState &, const SpectrogramData &,
                                       ImVec2 size, const SpectrogramOptions & = {});

inline constexpr double missingAudioValue = std::numeric_limits<double>::quiet_NaN();
struct AudioMeterResult {
    bool resetRequested = false; // host clears its own integrated/held measurements
    std::size_t clippedChannels = 0;
};
struct LoudnessData {
    double momentary = missingAudioValue, shortTerm = missingAudioValue, integrated = missingAudioValue;
    double rangeLow = missingAudioValue, rangeHigh = missingAudioValue; // absolute LUFS bounds
    ArrayView<PlotSeries> history; // seconds / LUFS, caller-owned labels
};
struct LoudnessOptions {
    Range levels{-60, 0};
    double target = -23;
    bool resetButton = true;
};
CYUI_API AudioMeterResult loudness(const char *id, PlotState &historyView, const LoudnessData &,
                                   ImVec2 size, const LoudnessOptions & = {});
struct TruePeakChannel {
    const char *label = "";
    double level = missingAudioValue, held = missingAudioValue; // dBTP; -infinity is silence
    std::uint64_t overs = 0;
};
struct TruePeakOptions {
    Range levels{-60, 6};
    double warning = -6, ceiling = 0;
    bool resetButton = true;
};
CYUI_API AudioMeterResult truePeak(const char *id, ArrayView<TruePeakChannel>, ImVec2 size,
                                   const TruePeakOptions & = {});
// Gauge -1..+1; optional time/correlation history. Pair with stereoScope for L/R or M/S traces.
CYUI_API PlotResult correlation(const char *id, PlotState &historyView, double value,
                                 ArrayView<PlotSeries> history, ImVec2 size);

struct ResponsePoint {
    ItemId id = 0;
    double frequency = 1000, gain = 0; // Hz / dB
    bool enabled = true;
};
struct ResponseState {
    PlotState view{{{20, 20000}, true}, {{-24, 24}, false}};
    std::vector<ResponsePoint> before, preview;
    ItemId active = 0;
    std::uint64_t revision = 0;
    bool dragging = false;
};
struct ResponseOptions {
    Range frequency{20, 20000}, gain{-24, 24}, phase{-180, 180};
    bool readOnly = false;
    std::uint64_t revision = 0;
    PlotOptions plot;
};
struct ResponseResult {
    PlotResult plot;
    Edit<std::vector<ResponsePoint>> points;
};
// Gain curves use the left dB axis; optional phase curves use the right degree axis.
// Stable unique nonzero handle IDs. Drag proposes Hz/dB edits, Escape cancels; revision/model
// changes or readOnly cancel active edits. Curves remain supplied by the host, including previews.
CYUI_API ResponseResult filterResponse(const char *id, ResponseState &, ArrayView<PlotSeries> gain,
                                        ArrayView<PlotSeries> phase, const std::vector<ResponsePoint> &,
                                        ImVec2 size, const ResponseOptions & = {});
struct DynamicsData {
    ArrayView<PlotSeries> transfer; // input dB / output dB
    double input = missingAudioValue, output = missingAudioValue, gainReduction = missingAudioValue;
};
struct DynamicsOptions {
    Range reduction{0, 36}; // positive dB of attenuation
    PlotOptions plot;
};
// Transfer curves, unity reference, operating point and gain-reduction bar.
CYUI_API PlotResult dynamics(const char *id, PlotState &, const DynamicsData &, ImVec2 size,
                              const DynamicsOptions & = {});
} // namespace cyui
